#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include "sample.h"

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("MyTestSvc") //��������

SERVICE_STATUS          gSvcStatus; // ����״̬�ṹ
SERVICE_STATUS_HANDLE   gSvcStatusHandle; // ����״̬���
HANDLE                  ghSvcStopEvent = NULL; // ����ֹͣ��Ӧ���

VOID WINAPI SvcMain(DWORD, LPTSTR *); // �������
VOID WINAPI SvcCtrlHandler(DWORD); // �������
VOID ReportSvcStatus(DWORD, DWORD, DWORD); // �������״̬
VOID SvcInit(DWORD, LPTSTR *); // �����ʼ��
VOID SvcReportEvent(LPTSTR); // ���񱨸��¼�

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
	// ��������в���Ϊ-install����װ����
	// ��������£�����SCM����
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
	
	// �����
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL } // ��������Խ�β
	};

	// ����������߳����ӵ�������ƹ�����SCM���Ӷ�ʹ���̳߳�Ϊ����Ŀ��Ƶ����߳�
	// �������߳�Ӧ�þ�����ô˺���(30s��)
	// �˺���ֻ�е������ڵ�ǰ�����е����з��񶼽���SERVICE_STOPPED״̬�Ż᷵��
	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
	}
}

// ��װ����SCM���ݿ�
VOID __stdcall DoInstallSvc()
{
	SC_HANDLE schSCManager; // SCM���ݿ���
	SC_HANDLE schService; // ������
	TCHAR szUnquotedPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH)) // ȡ��ǰģ����
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	// ��ֹ·�������ո񣬱��뱻�������������Ա���ȷ����
	// ���磺d:\my share\myservice.exe -> "d:\my share\myservice.exe"
	TCHAR szPath[MAX_PATH];
	StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

	// ���SCM���ݿ�ľ��
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// ��������
	schService = CreateService(
		schSCManager,              // SCM���ݿ� 
		SVCNAME,                   // ������
		SVCNAME,                   // ������ʾ���� 
		SERVICE_ALL_ACCESS,        // ��������Ȩ�� 
		SERVICE_WIN32_OWN_PROCESS, // �������� 
		SERVICE_DEMAND_START,      // ������������ 
		SERVICE_ERROR_NORMAL,      // ����������� 
		szPath,                    // �������·��
		NULL,                      // ����������У�no load ordering group 
		NULL,                      // �����ʶ��ǩ��no tag identifier 
		NULL,                      // ����������no dependencies 
		NULL,                      // system�û�
		NULL);                     // ������

	if (schService == NULL)
	{
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}
	else 
		printf("Service installed successfully\n");

	CloseServiceHandle(schService); // �رշ�����
	CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���
}

// �������ڵ�ص�����
// ������
//	dwArgc��lpszArgv�����в�������
//	lpszArgv���ַ������飬��һ���Ƿ������ƣ������ַ����ɵ���StartService��������������Ľ��̴��ݡ�
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// SvcMain����Ӧ�������RegisterServiceCtrlHandlerEx������ע�����Ŀ��ƴ�����
	gSvcStatusHandle = RegisterServiceCtrlHandler(
		SVCNAME,
		SvcCtrlHandler);

	if (!gSvcStatusHandle)
	{
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // ��������
	gSvcStatus.dwServiceSpecificExitCode = 0; // �����˳���

	// ��SCM�������ǰ״̬ΪSERVICE_START_PENDING
	// �������ڴ�״̬ʱ���������κο�����
	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	// ִ�з���ĳ�ʼ���ͷ�����幤����
	SvcInit(dwArgc, lpszArgv);
}

// �����ʼ���Ϳ�ʼִ�о��幤��
// ������
//	dwArgc��lpszArgv�����в�������
//	lpszArgv���ַ������飬��һ���Ƿ������ƣ������ַ����ɵ���StartService��������������Ľ��̴��ݡ�
VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// ȷ�����ڵ���ReportSvcStatus() with SERVICE_START_PENDING
	// �����ʼ��ʧ�ܣ�����ReportSvcStatus with SERVICE_STOPPED

	// ����һ��event��������ƴ�����SvcCtrlHandler���յ�ֹͣ������ʱ���ô�event���ź�
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

	// ��ʼ����ɣ���SCM���������SERVICE_RUNNING״̬
	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// ִ�й���ֱ������ֹͣ
	while (1)
	{
		// �ȴ�ֹͣ�ź�
		WaitForSingleObject(ghSvcStopEvent, INFINITE); // ���Կ���ʹ��RegisterWaitForSingleObject������ִ�ж�����������
		// ����ֹͣ����SCM����SERVICE_STOPPED״̬
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return; //ע��˴��������뷵�أ������ܵ������� ExitThread��Exit��������Ϊreturn��������Ϊ����������ڴ�
	}
}

// ���õ�ǰ����״̬�������䱨���SCM
// ������
//  dwCurrentState������ǰ״̬��SERVICE_STATUS��
//  dwWin32ExitCode�����������
//  dwWaitHint�����������Pending���Ĺ���ʱ�䣨���룩
VOID ReportSvcStatus(DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// ���SERVICE_STATUS�ṹ��
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

	// ��SCM�������״̬
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

// ������ƴ�������ÿ��ʹ��ControlService�������Ϳ����������ʱ����SCM���ô˺���
// ������
//  dwCtrl��������
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	// ������������
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		// ���÷����˳��ź�
		SetEvent(ghSvcStopEvent);
		// ��������ƴ������յ�������ʱ��������������뵼�·���״̬��������ʱ������Ӧ�������״̬�� 
		// ������񲻶Կ�����ִ���κβ�����������Ӧ�������ƹ���������״̬��
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0); // ��SCM���ݿⱨ�����״̬
		return;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

// ����Ϣ��¼���¼���־
// ������
//  szFunction��ʧ�ܵĺ�����
// ע�⣺
//  ���������Ӧ�ó����¼���־����һ����ڡ�
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

// ���·���������Ϣ
VOID __stdcall DoUpdateSvcDesc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_DESCRIPTION sd;
	LPTSTR szDesc = TEXT("����һ������������Ϣ");

	// ��ȡSCM���ݿ�ľ��
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// ��ȡ������
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

	// �޸ķ���������Ϣ
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

// ���÷����Զ�����
VOID __stdcall DoSetAutoStartSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	// ��ȡSCM���ݿ���
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// ��ȡ������
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

	// �޸ķ�����������
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

// ��������
VOID __stdcall DoStartSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	DWORD dwBytesNeeded;
 
	// ��ȡSCM���ݿ�ľ��
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // servicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// ��ȡ������
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

	// �������Ƿ��������С�
	// ���������ֹͣ���񣬵�Ϊ�˼���������ʾ��ֻ���ء�
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

	// �ȴ�����ֹͣ��Ȼ���ٳ�����������
	while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
	{
		// �ȴ�ʱ�䲻Ҫ�����ȴ���ʾ��
		// ���õļ���ǵȴ���ʾ��ʮ��֮һ����������1�룬������10�롣
		dwWaitTime = ssStatus.dwWaitHint / 10;

		if (dwWaitTime < 1000) // ������1��
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000) // ������10�� 
			dwWaitTime = 10000;

		Sleep(dwWaitTime);

		// ���״̬��ֱ�������ٴ���SERVICE_STOP_PENDING
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
			// �����ȴ��ͼ��
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

	// ������������
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

	// ���״̬��ֱ�������ٴ���SERVICE_START_PENDING
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
		// �ȴ�ʱ�䲻Ҫ�����ȴ���ʾ��
		// �õļ���ǵȴ���ʾ��ʮ��֮һ����������1�룬Ҳ������10�롣
		dwWaitTime = ssStatus.dwWaitHint / 10;

		if (dwWaitTime < 1000)
			dwWaitTime = 1000;
		else if (dwWaitTime > 10000)
			dwWaitTime = 10000;

		Sleep(dwWaitTime);

		// �ٴμ��״̬
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
			// �����ȴ��ͼ��
			dwStartTickCount = GetTickCount();
			dwOldCheckPoint = ssStatus.dwCheckPoint;
		}
		else
		{
			if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
			{
				// �ڵȴ���ʾ��δȡ���κν�չ��
				break;
			}
		}
	}

	// ȷ�������Ƿ���������
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

// ���·���DACL������guest�ʻ�������ֹͣ��ɾ���Ͷ�ȡ����Ȩ�ޡ�
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
 
	// ��ȡSCM���ݿ���
	SC_HANDLE schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// ��ȡ������
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

	// ��ȡ��ǰ�İ�ȫ������
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

	// ��ʼ���µİ�ȫ������
	if (!InitializeSecurityDescriptor(&sd,
		SECURITY_DESCRIPTOR_REVISION))
	{
		printf("InitializeSecurityDescriptor failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}

	// �ڰ�ȫ�������������µ�DACL
	if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE))
	{
		printf("SetSecurityDescriptorDacl failed(%d)\n", GetLastError());
		goto dacl_cleanup;
	}

	// Ϊ������������µ�DACL
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

// ֹͣ����
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

	// ȷ������û���Ѿ�ֹͣ
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

	// ����������ڹ��𣬵ȴ�
	while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
	{
		printf("Service stop pending...\n");

		// �ȴ�ʱ�䲻Ҫ�����ȴ���ʾ��
		// �õļ���ǵȴ���ʾ��ʮ��֮һ����������1�룬Ҳ������10�롣

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
	// ��������������У��������ֹͣ������
	StopDependentServices();

	// ����ֹͣ״̬������
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp))
	{
		printf("ControlService failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	// �ȴ�����ֹͣ
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

// ��SCM���ݿ���ɾ������
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

// ��ȡ��ǰ����������Ϣ
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