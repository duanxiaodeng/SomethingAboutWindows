#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include "sample.h"

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("MyTestSvc") //服务名称

SERVICE_STATUS          gSvcStatus; // 服务状态结构
SERVICE_STATUS_HANDLE   gSvcStatusHandle; // 服务状态句柄
HANDLE                  ghSvcStopEvent = NULL; // 服务停止响应句柄

VOID WINAPI SvcMain(DWORD, LPTSTR *); // 服务入口
VOID WINAPI SvcCtrlHandler(DWORD); // 服务控制
VOID ReportSvcStatus(DWORD, DWORD, DWORD); // 报告服务状态
VOID SvcInit(DWORD, LPTSTR *); // 服务初始化
VOID SvcReportEvent(LPTSTR); // 服务报告事件

VOID __stdcall DoInstallSvc(void);
VOID __stdcall DoStartSvc(void);
VOID __stdcall DoStopSvc(void);
VOID __stdcall DoDeleteSvc(void);
VOID __stdcall DoQuerySvc(void);

VOID __stdcall DoUpdateSvcDesc(void);
VOID __stdcall DoSetAutoStartSvc(void);
VOID __stdcall DoUpdateSvcDacl(void);
BOOL __stdcall StopDependentServices(void);

int __cdecl _tmain(int argc, TCHAR *argv[])
{
	// 如果命令行参数为-install，则安装服务
	// 其他情况下，服务被SCM启动
	if (lstrcmpi(argv[1], TEXT("-install")) == 0)
	{
		DoInstallSvc();
		DoUpdateSvcDesc();
		DoSetAutoStartSvc();
		DoUpdateSvcDacl(); 
		DoStartSvc();
		return 0;
	}
	else if (lstrcmpi(argv[1], TEXT("-start")) == 0)
	{
		DoStartSvc();
	}
	else if (lstrcmpi(argv[1], TEXT("-stop")) == 0)
	{
		DoStopSvc();
	}
	else if (lstrcmpi(argv[1], TEXT("-delete")) == 0)
	{
		DoStopSvc();
		DoDeleteSvc();
	}
	
	// 服务表
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL } // 数组必须以结尾
	};

	// 将服务的主线程连接到服务控制管理器SCM，从而使该线程成为服务的控制调度线程
	// 服务主线程应该尽快调用此函数(30s内)
	// 此函数只有当运行在当前进程中的所有服务都进入SERVICE_STOPPED状态才会返回
	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
	}
}

// 安装服务到SCM数据库
VOID __stdcall DoInstallSvc()
{
	SC_HANDLE schSCManager; // SCM数据库句柄
	SC_HANDLE schService; // 服务句柄
	TCHAR szUnquotedPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH)) // 取当前模块名
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	// 防止路径包含空格，必须被引号引起来，以便正确解析
	// 例如：d:\my share\myservice.exe -> "d:\my share\myservice.exe"
	TCHAR szPath[MAX_PATH];
	StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

	// 获得SCM数据库的句柄
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// 创建服务
	schService = CreateService(
		schSCManager,              // SCM数据库 
		SVCNAME,                   // 服务名
		SVCNAME,                   // 服务显示名称 
		SERVICE_ALL_ACCESS,        // 期望访问权限 
		SERVICE_WIN32_OWN_PROCESS, // 服务类型 
		SERVICE_DEMAND_START,      // 服务启动类型 
		SERVICE_ERROR_NORMAL,      // 错误控制类型 
		szPath,                    // 服务程序路径
		NULL,                      // 服务分组排列：no load ordering group 
		NULL,                      // 服务标识标签：no tag identifier 
		NULL,                      // 服务依赖：no dependencies 
		NULL,                      // system用户
		NULL);                     // 无密码

	if (schService == NULL)
	{
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}
	else 
		printf("Service installed successfully\n");

	CloseServiceHandle(schService); // 关闭服务句柄
	CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄
}

// 服务的入口点回调函数
// 参数：
//	dwArgc：lpszArgv数组中参数个数
//	lpszArgv：字符串数组，第一个是服务名称，后续字符串由调用StartService函数来启动服务的进程传递。
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// SvcMain函数应尽快调用RegisterServiceCtrlHandlerEx函数，注册服务的控制处理函数
	gSvcStatusHandle = RegisterServiceCtrlHandler(
		SVCNAME,
		SvcCtrlHandler);

	if (!gSvcStatusHandle)
	{
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // 服务类型
	gSvcStatus.dwServiceSpecificExitCode = 0; // 服务退出码

	// 向SCM报告服务当前状态为SERVICE_START_PENDING
	// 当服务处于此状态时，不接收任何控制码
	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	// 执行服务的初始化和服务具体工作。
	SvcInit(dwArgc, lpszArgv);
}

// 服务初始化和开始执行具体工作
// 参数：
//	dwArgc：lpszArgv数组中参数个数
//	lpszArgv：字符串数组，第一个是服务名称，后续字符串由调用StartService函数来启动服务的进程传递。
VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// 确保定期调用ReportSvcStatus() with SERVICE_START_PENDING
	// 如果初始化失败，调用ReportSvcStatus with SERVICE_STOPPED

	// 创建一个event。服务控制处理函数SvcCtrlHandler在收到停止控制码时设置此event的信号
	ghSvcStopEvent = CreateEvent(
		NULL,    // default security attributes
		TRUE,    // manual reset event
		FALSE,   // not signaled
		NULL);   // no name

	if (ghSvcStopEvent == NULL)
	{
		ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	// 初始化完成，向SCM报告服务处于SERVICE_RUNNING状态
	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// 执行工作直到服务停止
	while (1)
	{
		// 等待停止信号
		WaitForSingleObject(ghSvcStopEvent, INFINITE); // 可以考虑使用RegisterWaitForSingleObject函数来执行额外的清理操作
		// 服务停止，向SCM报告SERVICE_STOPPED状态
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return; //注意此处函数必须返回，而不能调用类似 ExitThread或Exit函数，因为return可以清理为参数分配的内存
	}
}

// 设置当前服务状态，并将其报告给SCM
// 参数：
//  dwCurrentState：服务当前状态（SERVICE_STATUS）
//  dwWin32ExitCode：服务错误码
//  dwWaitHint：挂起操作（Pending）的估计时间（毫秒）
VOID ReportSvcStatus(DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// 填充SERVICE_STATUS结构体
	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else 
		gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else 
		gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// 向SCM报告服务状态
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

// 服务控制处理函数。每当使用ControlService函数发送控制码给服务时，由SCM调用此函数
// 参数：
//  dwCtrl：控制码
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	// 处理服务控制码
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		// 设置服务退出信号
		SetEvent(ghSvcStopEvent);
		// 当服务控制处理函数收到控制码时，仅当处理控制码导致服务状态发生更改时，它才应报告服务状态。 
		// 如果服务不对控制码执行任何操作，则它不应向服务控制管理器报告状态。
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0); // 向SCM数据库报告服务状态
		return;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

// 将消息记录到事件日志
// 参数：
//  szFunction：失败的函数名
// 注意：
//  服务必须在应用程序事件日志中有一个入口。
VOID SvcReportEvent(LPTSTR szFunction)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];
	
	hEventSource = RegisterEventSource(NULL, SVCNAME);

	if (hEventSource != NULL)
	{
		StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource,        // event log handle
			EVENTLOG_ERROR_TYPE, // event type
			0,                   // event category
			SVC_ERROR,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}

// 更新服务描述信息
VOID __stdcall DoUpdateSvcDesc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_DESCRIPTION sd;
	LPTSTR szDesc = TEXT("这是一条测试描述信息");

	// 获取SCM数据库的句柄
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// 获取服务句柄
	schService = OpenService(
		schSCManager,            // SCM database 
		SVCNAME,               // name of service 
		SERVICE_CHANGE_CONFIG);  // need change config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// 修改服务描述信息
	sd.lpDescription = szDesc;

	if (!ChangeServiceConfig2(
		schService,                 // handle to service
		SERVICE_CONFIG_DESCRIPTION, // change: description
		&sd))                       // new description
	{
		printf("ChangeServiceConfig2 failed\n");
	}
	else 
		printf("Service description updated successfully.\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

// 设置服务自动启动
VOID __stdcall DoSetAutoStartSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	// 获取SCM数据库句柄
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// 获取服务句柄
	schService = OpenService(
		schSCManager,            // SCM database 
		SVCNAME,               // name of service 
		SERVICE_CHANGE_CONFIG);  // need change config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// 修改服务启动类型
	if (!ChangeServiceConfig(
		schService,            // handle of service 
		SERVICE_NO_CHANGE,     // service type: no change 
		SERVICE_AUTO_START,    // service start type 
		SERVICE_NO_CHANGE,     // error control: no change 
		NULL,                  // binary path: no change 
		NULL,                  // load order group: no change 
		NULL,                  // tag ID: no change 
		NULL,                  // dependencies: no change 
		NULL,                  // account name: no change 
		NULL,                  // password: no change 
		NULL))                 // display name: no change
	{
		printf("ChangeServiceConfig failed (%d)\n", GetLastError());
	}
	else printf("Service enabled successfully.\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

// 启动服务
VOID __stdcall DoStartSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	DWORD dwBytesNeeded;
 
	// 获取SCM数据库的句柄
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // servicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// 获取服务句柄
	schService = OpenService(
		schSCManager,         // SCM database 
		SVCNAME,              // name of service 
		SERVICE_ALL_ACCESS);  // full access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Check the status in case the service is not stopped. 
	if (!QueryServiceStatusEx(
		schService,                     // handle to service 
		SC_STATUS_PROCESS_INFO,         // information level
		(LPBYTE)&ssStatus,             // address of structure
		sizeof(SERVICE_STATUS_PROCESS), // size of structure
		&dwBytesNeeded))              // size needed if buffer is too small
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// 检查服务是否已在运行。
	// 在这里可以停止服务，但为了简单起见，这个示例只返回。
	if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
	{
		printf("Cannot start the service because it is already running\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// Save the tick count and initial checkpoint.
	dwStartTickCount = GetTickCount();
	dwOldCheckPoint = ssStatus.dwCheckPoint;

	// 等待服务停止，然后再尝试启动它。
	while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
	{
		// 等待时间不要超过等待提示。
		// 良好的间隔是等待提示的十分之一，但不少于1秒，不超过10秒。
		dwWaitTime = ssStatus.dwWaitHint / 10;

		if (dwWaitTime < 1000) // 不少于1秒
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000) // 不超过10秒 
			dwWaitTime = 10000;

		Sleep(dwWaitTime);

		// 检查状态，直到服务不再处于SERVICE_STOP_PENDING
		if (!QueryServiceStatusEx(
			schService,                     // handle to service 
			SC_STATUS_PROCESS_INFO,         // information level
			(LPBYTE)&ssStatus,             // address of structure
			sizeof(SERVICE_STATUS_PROCESS), // size of structure
			&dwBytesNeeded))              // size needed if buffer is too small
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}

		if (ssStatus.dwCheckPoint > dwOldCheckPoint)
		{
			// 继续等待和检查
			dwStartTickCount = GetTickCount();
			dwOldCheckPoint = ssStatus.dwCheckPoint;
		}
		else
		{
			if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
			{
				printf("Timeout waiting for service to stop\n");
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				return;
			}
		}
	}

	// 尝试启动服务
	if (!StartService(
		schService,  // handle to service 
		0,           // number of arguments 
		NULL))       // no arguments 
	{
		printf("StartService failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}
	else 
		printf("Service start pending...\n");

	// 检查状态，直到服务不再处于SERVICE_START_PENDING
	if (!QueryServiceStatusEx(
		schService,                     // handle to service 
		SC_STATUS_PROCESS_INFO,         // info level
		(LPBYTE)&ssStatus,             // address of structure
		sizeof(SERVICE_STATUS_PROCESS), // size of structure
		&dwBytesNeeded))              // if buffer too small
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return;
	}

	// Save the tick count and initial checkpoint.
	dwStartTickCount = GetTickCount();
	dwOldCheckPoint = ssStatus.dwCheckPoint;

	while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
	{
		// 等待时间不要超过等待提示。
		// 好的间隔是等待提示的十分之一，但不少于1秒，也不超过10秒。
		dwWaitTime = ssStatus.dwWaitHint / 10;

		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;

		Sleep(dwWaitTime);

		// 再次检查状态
		if (!QueryServiceStatusEx(
			schService,             // handle to service 
			SC_STATUS_PROCESS_INFO, // info level
			(LPBYTE)&ssStatus,             // address of structure
			sizeof(SERVICE_STATUS_PROCESS), // size of structure
			&dwBytesNeeded))              // if buffer too small
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			break;
		}

		if (ssStatus.dwCheckPoint > dwOldCheckPoint)
		{
			// 继续等待和检查
			dwStartTickCount = GetTickCount();
			dwOldCheckPoint = ssStatus.dwCheckPoint;
		}
		else
		{
			if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
			{
				// 在等待提示中未取得任何进展。
				break;
			}
		}
	}

	// 确定服务是否正在运行
	if (ssStatus.dwCurrentState == SERVICE_RUNNING)
	{
		printf("Service started successfully.\n");
	}
	else
	{
		printf("Service not started. \n");
		printf("  Current State: %d\n", ssStatus.dwCurrentState);
		printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
		printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
		printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

// 更新服务DACL，授予guest帐户启动、停止、删除和读取控制权限。
VOID __stdcall DoUpdateSvcDacl()
{
	EXPLICIT_ACCESS      ea;
	SECURITY_DESCRIPTOR  sd;
	PSECURITY_DESCRIPTOR psd = NULL;
	PACL                 pacl = NULL;
	PACL                 pNewAcl = NULL;
	BOOL                 bDaclPresent = FALSE;
	BOOL                 bDaclDefaulted = FALSE;
	DWORD                dwError = 0;
	DWORD                dwSize = 0;
	DWORD                dwBytesNeeded = 0;
 
	// 获取SCM数据库句柄
	SC_HANDLE schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// 获取服务句柄
	SC_HANDLE schService = OpenService(
		schSCManager,              // SCManager database 
		SVCNAME,                 // name of service 
		READ_CONTROL | WRITE_DAC); // access

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// 获取当前的安全描述符
	if (!QueryServiceObjectSecurity(schService,
		DACL_SECURITY_INFORMATION,
		&psd,           // using NULL does not work on all versions
		0,
		&dwBytesNeeded))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			dwSize = dwBytesNeeded;
			psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, dwSize);
			if (psd == NULL)
			{
				// Note: HeapAlloc does not support GetLastError.
				printf("HeapAlloc failed\n");
				goto dacl_cleanup;
			}

			if (!QueryServiceObjectSecurity(schService,
				DACL_SECURITY_INFORMATION, psd, dwSize, &dwBytesNeeded))
			{
				printf("QueryServiceObjectSecurity failed (%d)\n", GetLastError());
				goto dacl_cleanup;
			}
		}
		else
		{
			printf("QueryServiceObjectSecurity failed (%d)\n", GetLastError());
			goto dacl_cleanup;
		}
	}

	// Get the DACL.
	if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl,
		&bDaclDefaulted))
	{
		printf("GetSecurityDescriptorDacl failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}

	// Build the ACE.
	BuildExplicitAccessWithName(&ea, TEXT("GUEST"),
		SERVICE_START | SERVICE_STOP | READ_CONTROL | DELETE,
		SET_ACCESS, NO_INHERITANCE);

	dwError = SetEntriesInAcl(1, &ea, pacl, &pNewAcl);
	if (dwError != ERROR_SUCCESS)
	{
		printf("SetEntriesInAcl failed(%d)\n", dwError);
		goto dacl_cleanup;
	}

	// 初始化新的安全描述符
	if (!InitializeSecurityDescriptor(&sd,
		SECURITY_DESCRIPTOR_REVISION))
	{
		printf("InitializeSecurityDescriptor failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}

	// 在安全描述符中设置新的DACL
	if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE))
	{
		printf("SetSecurityDescriptorDacl failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}

	// 为服务对象设置新的DACL
	if (!SetServiceObjectSecurity(schService,
		DACL_SECURITY_INFORMATION, &sd))
	{
		printf("SetServiceObjectSecurity failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}
	else printf("Service DACL updated successfully\n");

dacl_cleanup:
	CloseServiceHandle(schSCManager);
	CloseServiceHandle(schService);

	if (NULL != pNewAcl)
		LocalFree((HLOCAL)pNewAcl);
	if (NULL != psd)
		HeapFree(GetProcessHeap(), 0, (LPVOID)psd);
}

// 停止服务
VOID __stdcall DoStopSvc()
{
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwStartTime = GetTickCount();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = 30000; // 30-second time-out
	DWORD dwWaitTime;

	// Get a handle to the SCM database. 
	SC_HANDLE schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.
	SC_HANDLE schService = OpenService(
		schSCManager,         // SCM database 
		SVCNAME,            // name of service 
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// 确保服务没有已经停止
	if (!QueryServiceStatusEx(
		schService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded))
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	if (ssp.dwCurrentState == SERVICE_STOPPED)
	{
		printf("Service is already stopped.\n");
		goto stop_cleanup;
	}

	// 如果服务正在挂起，等待
	while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
	{
		printf("Service stop pending...\n");

		// 等待时间不要超过等待提示。
		// 好的间隔是等待提示的十分之一，但不少于1秒，也不超过10秒。

		dwWaitTime = ssp.dwWaitHint / 10;

		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;

		Sleep(dwWaitTime);

		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			goto stop_cleanup;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			printf("Service stopped successfully.\n");
			goto stop_cleanup;
		}

		if (GetTickCount() - dwStartTime > dwTimeout)
		{
			printf("Service stop timed out.\n");
			goto stop_cleanup;
		}
	}

	// If the service is running, dependencies must be stopped first.
	// 如果服务正在运行，则必须先停止依赖。
	StopDependentServices();

	// 发送停止状态给服务
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp))
	{
		printf("ControlService failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	// 等待服务停止
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		Sleep(ssp.dwWaitHint);
		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			goto stop_cleanup;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
			break;

		if (GetTickCount() - dwStartTime > dwTimeout)
		{
			printf("Wait timed out\n");
			goto stop_cleanup;
		}
	}
	printf("Service stopped successfully\n");

stop_cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

// 从SCM数据库中删除服务
VOID __stdcall DoDeleteSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS ssStatus;

	// Get a handle to the SCM database. 
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.
	schService = OpenService(
		schSCManager,       // SCM database 
		SVCNAME,            // name of service 
		DELETE);            // need delete access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Delete the service.
	if (!DeleteService(schService))
		printf("DeleteService failed (%d)\n", GetLastError());
	else 
		printf("Service deleted successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

BOOL __stdcall StopDependentServices()
{
	// Get a handle to the SCM database. 
	SC_HANDLE schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return FALSE;
	}

	// Get a handle to the service.
	SC_HANDLE schService = OpenService(
		schSCManager,         // SCM database 
		SVCNAME,            // name of service 
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return FALSE;
	}

	DWORD i;
	DWORD dwBytesNeeded;
	DWORD dwCount;

	LPENUM_SERVICE_STATUS   lpDependencies = NULL;
	ENUM_SERVICE_STATUS     ess;
	SC_HANDLE               hDepService;
	SERVICE_STATUS_PROCESS  ssp;

	DWORD dwStartTime = GetTickCount();
	DWORD dwTimeout = 30000; // 30-second time-out

	// Pass a zero-length buffer to get the required buffer size.
	if (EnumDependentServices(schService, SERVICE_ACTIVE,
		lpDependencies, 0, &dwBytesNeeded, &dwCount))
	{
		// If the Enum call succeeds, then there are no dependent
		// services, so do nothing.
		return TRUE;
	}
	else
	{
		if (GetLastError() != ERROR_MORE_DATA)
			return FALSE; // Unexpected error

						  // Allocate a buffer for the dependencies.
		lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

		if (!lpDependencies)
			return FALSE;

		__try {
			// Enumerate the dependencies.
			if (!EnumDependentServices(schService, SERVICE_ACTIVE,
				lpDependencies, dwBytesNeeded, &dwBytesNeeded,
				&dwCount))
				return FALSE;

			for (i = 0; i < dwCount; i++)
			{
				ess = *(lpDependencies + i);
				// Open the service.
				hDepService = OpenService(schSCManager,
					ess.lpServiceName,
					SERVICE_STOP | SERVICE_QUERY_STATUS);

				if (!hDepService)
					return FALSE;

				__try {
					// Send a stop code.
					if (!ControlService(hDepService,
						SERVICE_CONTROL_STOP,
						(LPSERVICE_STATUS)&ssp))
						return FALSE;

					// Wait for the service to stop.
					while (ssp.dwCurrentState != SERVICE_STOPPED)
					{
						Sleep(ssp.dwWaitHint);
						if (!QueryServiceStatusEx(
							hDepService,
							SC_STATUS_PROCESS_INFO,
							(LPBYTE)&ssp,
							sizeof(SERVICE_STATUS_PROCESS),
							&dwBytesNeeded))
							return FALSE;

						if (ssp.dwCurrentState == SERVICE_STOPPED)
							break;

						if (GetTickCount() - dwStartTime > dwTimeout)
							return FALSE;
					}
				}
				__finally
				{
					// Always release the service handle.
					CloseServiceHandle(hDepService);
				}
			}
		}
		__finally
		{
			// Always free the enumeration buffer.
			HeapFree(GetProcessHeap(), 0, lpDependencies);
		}
	}
	return TRUE;
}

// 获取当前服务配置信息
VOID __stdcall DoQuerySvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	LPQUERY_SERVICE_CONFIG lpsc;
	LPSERVICE_DESCRIPTION lpsd;
	DWORD dwBytesNeeded, cbBufSize, dwError;

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager,          // SCM database 
		SVCNAME,             // name of service 
		SERVICE_QUERY_CONFIG); // need query config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Get the configuration information.

	if (!QueryServiceConfig(
		schService,
		NULL,
		0,
		&dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			lpsc = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, cbBufSize);
		}
		else
		{
			printf("QueryServiceConfig failed (%d)", dwError);
			goto cleanup;
		}
	}

	if (!QueryServiceConfig(
		schService,
		lpsc,
		cbBufSize,
		&dwBytesNeeded))
	{
		printf("QueryServiceConfig failed (%d)", GetLastError());
		goto cleanup;
	}

	if (!QueryServiceConfig2(
		schService,
		SERVICE_CONFIG_DESCRIPTION,
		NULL,
		0,
		&dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			lpsd = (LPSERVICE_DESCRIPTION)LocalAlloc(LMEM_FIXED, cbBufSize);
		}
		else
		{
			printf("QueryServiceConfig2 failed (%d)", dwError);
			goto cleanup;
		}
	}

	if (!QueryServiceConfig2(
		schService,
		SERVICE_CONFIG_DESCRIPTION,
		(LPBYTE)lpsd,
		cbBufSize,
		&dwBytesNeeded))
	{
		printf("QueryServiceConfig2 failed (%d)", GetLastError());
		goto cleanup;
	}

	// Print the configuration information.

	_tprintf(TEXT("%s configuration: \n"), SVCNAME);
	_tprintf(TEXT("  Type: 0x%x\n"), lpsc->dwServiceType);
	_tprintf(TEXT("  Start Type: 0x%x\n"), lpsc->dwStartType);
	_tprintf(TEXT("  Error Control: 0x%x\n"), lpsc->dwErrorControl);
	_tprintf(TEXT("  Binary path: %s\n"), lpsc->lpBinaryPathName);
	_tprintf(TEXT("  Account: %s\n"), lpsc->lpServiceStartName);

	if (lpsd->lpDescription != NULL && lstrcmp(lpsd->lpDescription, TEXT("")) != 0)
		_tprintf(TEXT("  Description: %s\n"), lpsd->lpDescription);
	if (lpsc->lpLoadOrderGroup != NULL && lstrcmp(lpsc->lpLoadOrderGroup, TEXT("")) != 0)
		_tprintf(TEXT("  Load order group: %s\n"), lpsc->lpLoadOrderGroup);
	if (lpsc->dwTagId != 0)
		_tprintf(TEXT("  Tag ID: %d\n"), lpsc->dwTagId);
	if (lpsc->lpDependencies != NULL && lstrcmp(lpsc->lpDependencies, TEXT("")) != 0)
		_tprintf(TEXT("  Dependencies: %s\n"), lpsc->lpDependencies);

	LocalFree(lpsc);
	LocalFree(lpsd);

cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}