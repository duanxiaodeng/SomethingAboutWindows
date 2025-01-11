#include "MyTestServiceController.h"
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include <TlHelp32.h>
#include "MyTestServiceUtils.h"

bool CMyTestServiceController::installService()
{
	MyTestServiceUtils::DebugLog("Service installing...!");
	
	bool res = false;
	SC_HANDLE schSCManager = NULL; // SCM数据库句柄
	SC_HANDLE schService = NULL; // 服务句柄

	do
	{
		MyTestServiceUtils::DebugLog("Try to uinstall MyTestService.");
		uninstallService();

		// 防止上次卸载服务时服务被保活，先强杀一次
		killServiceProcess();
		Sleep(3 * 1000);

		// 取当前模块路径
		TCHAR szUnquotedPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
		{
			MyTestServiceUtils::DebugLog("Failed to get the path of current module, errno = %d.\n", GetLastError());
			break;
		}

		// 将短路径转换为长路径格式
		TCHAR szLongPath[MAX_PATH];
		DWORD dwLen = GetLongPathName(szUnquotedPath, szLongPath, MAX_PATH);
		if (dwLen > MAX_PATH || dwLen <= 0)
		{
			MyTestServiceUtils::DebugLog("GetLongPathName failed, errno = %d.\n", GetLastError());
			std::copy(szUnquotedPath, szUnquotedPath + std::wcslen(szUnquotedPath) + 1, szLongPath); // szLongPath = szUnquotedPath
		}

		// 防止路径包含空格，必须被引号引起来，以便正确解析
		TCHAR szPath[MAX_PATH];
		StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szLongPath);
		
		// SCM数据库的句柄
		schSCManager = OpenSCManagerA(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		// 创建服务
		schService = CreateService(
			schSCManager,							// SCM数据库 
			SVCNAME,								// 服务名
			SVCNAME,								// 服务显示名称 
			SERVICE_ALL_ACCESS,						// 期望访问权限 
			SERVICE_WIN32_OWN_PROCESS,				// 服务类型
			SERVICE_AUTO_START,						// 服务启动类型
			SERVICE_ERROR_NORMAL,					// 错误控制类型 
			szPath,									// 服务程序路径
			L"",									// 服务分组排列：no load ordering group 
			NULL,									// 服务标识标签：no tag identifier 
			DEPENDENCY,								// 服务依赖
			NULL,									// 用户：NULL表示LocalSystem账户
			NULL);									// 密码

		if (schService == NULL)
		{
			MyTestServiceUtils::DebugLog("CreateService failed, errno = %d.\n", GetLastError());
			break;
		}		
		MyTestServiceUtils::DebugLog("Service installed successfully!\n");

		// 修改服务描述信息
		SERVICE_DESCRIPTION sd;
		sd.lpDescription = (LPWSTR)std::wstring(DESCRIPTION).c_str();
		if (!ChangeServiceConfig2(
			schService,                 // handle to service
			SERVICE_CONFIG_DESCRIPTION, // change: description
			&sd))                       // new description
		{
			MyTestServiceUtils::DebugLog("ChangeServiceConfig2 failed, errno = %d.\n", GetLastError());
			break;
		}
		MyTestServiceUtils::DebugLog("Service description updated successfully!\n");

		res = true;
	} while (0);

	if(schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if(schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return res;
}

bool CMyTestServiceController::uninstallService()
{
	MyTestServiceUtils::DebugLog("Service uninstalling...!");
	
	bool res = false;
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	do
	{
		if (!stopService())
		{
			MyTestServiceUtils::DebugLog("Failed to stop MyTestService, terminate it.\n");
		}
		else
		{
			MyTestServiceUtils::DebugLog("Service stopped successfully.\n");
		}

		Sleep(5 * 1000); // 等待服务退出
		killServiceProcess();
			
		// 获取SCM数据库句柄
		schSCManager = OpenSCManager(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		schService = OpenService(
			schSCManager,	// SCM database 
			SVCNAME,		// name of service 
			DELETE);		// need delete access 

		if (schService == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenService failed, errno = %d.\n", GetLastError());
			break;
		}

		// 删除服务
		if (!DeleteService(schService))
		{
			MyTestServiceUtils::DebugLog("DeleteService failed, errno = %d.\n", GetLastError());
			break;
		}

		// 防止服务停止失败，强杀
		killServiceProcess();

		MyTestServiceUtils::DebugLog("Service deleted successfully.\n");
		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return res;
}

void CMyTestServiceController::waitPendingService(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		MyTestServiceUtils::DebugLog("Service stop pending...\n");
		// 等待时间不要超过等待提示。
		// 间隔最好是等待提示的十分之一，但不少于1秒，也不超过10秒。
		DWORD dwWaitTime = ssp.dwWaitHint / 10;
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
			MyTestServiceUtils::DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
			break;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			MyTestServiceUtils::DebugLog("Service stopped successfully.\n");
			break;
		}

		if (GetTickCount() - dwStartTime > dwTimeout)
		{
			MyTestServiceUtils::DebugLog("Service stop timed out.\n");
			break;
		}
	}
}

void CMyTestServiceController::enSureServiceStop(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp))
	{
		MyTestServiceUtils::DebugLog("ControlService failed, errno = %d.\n", GetLastError());
	}
	else
	{
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
				MyTestServiceUtils::DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
				break;
			}

			if (ssp.dwCurrentState == SERVICE_STOPPED)
			{
				MyTestServiceUtils::DebugLog("Service stopped successfully. \n");
				break;
			}

			if (GetTickCount() - dwStartTime > dwTimeout)
			{
				MyTestServiceUtils::DebugLog("Wait timed out. \n");
				break;
			}
		}
	}

}

bool CMyTestServiceController::killServiceProcess()
{
	MyTestServiceUtils::DebugLog("Terminate MyTestService.exe.\n");
	int pid = getServicePid();
	if (pid > 0)
	{
		if (!MyTestServiceUtils::terminateProcessByID(pid))
		{
			MyTestServiceUtils::DebugLog("Failed to terminate MyTestService.exe, errno = %d.\n", GetLastError());
		}
		else
		{
			MyTestServiceUtils::DebugLog("Terminate MyTestService.exe successfully.\n");
		}
	}
	else
	{
		MyTestServiceUtils::DebugLog("Failed to get the pid of MyTestService.exe, skip\n");
	}
	return true;
}

bool CMyTestServiceController::stopService()
{
	bool res = false;
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	do
	{
		// 获取SCM数据库句柄
		schSCManager = OpenSCManagerA(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		schService = OpenService(
			schSCManager,			// SCM database 
			SVCNAME,				// name of service 
			SERVICE_STOP |
			SERVICE_QUERY_STATUS);	// need access 

		if (schService == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenService failed, errno = %d.\n", GetLastError());
			break;
		}

		// 停止服务
		SERVICE_STATUS_PROCESS ssp;
		DWORD dwStartTime = GetTickCount();
		DWORD dwBytesNeeded;
		DWORD dwTimeout = 30000; // 30 second time-out
		DWORD dwWaitTime;
		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			MyTestServiceUtils::DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
		}
		else
		{
			if (ssp.dwCurrentState == SERVICE_STOPPED)
			{
				MyTestServiceUtils::DebugLog("Service is already stopped.\n");
			}
			else if (ssp.dwCurrentState == SERVICE_STOP_PENDING)
			{
				waitPendingService(schService,ssp, dwBytesNeeded, dwStartTime, dwTimeout);
			}
			else
			{
				// 发送停止控制给服务
				enSureServiceStop(schService, ssp, dwBytesNeeded, dwStartTime, dwTimeout);
			}
		}

		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return res;
}

int CMyTestServiceController::getServicePid()
{
	int pid = -1;
	SC_HANDLE schSCManager = NULL; // SCM数据库句柄
	SC_HANDLE schService = NULL; // 服务句柄

	do
	{
		schSCManager = OpenSCManagerA(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		schService = OpenService(
			schSCManager,			// SCM database 
			SVCNAME,				// name of service 
			SERVICE_QUERY_STATUS);	// need access 

		if (schService == NULL)
		{
			MyTestServiceUtils::DebugLog("OpenService failed, errno = %d.\n", GetLastError());
			break;
		}

		SERVICE_STATUS_PROCESS ssp;
		DWORD dwBytesNeeded;
		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			MyTestServiceUtils::DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
			break;
		}
		else
		{
			pid = ssp.dwProcessId;
		}

	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return pid;
}