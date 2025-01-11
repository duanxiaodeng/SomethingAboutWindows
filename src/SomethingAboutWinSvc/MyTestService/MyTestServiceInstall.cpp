#include "MyTestServiceInstaller.h"
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include <TlHelp32.h>

bool CMyTestServiceInstaller::installService()
{
	DebugLog("Service installing...!");
	
	bool res = false;
	SC_HANDLE schSCManager = NULL; // SCM数据库句柄
	SC_HANDLE schService = NULL; // 服务句柄

	do
	{
		// 取当前模块路径
		TCHAR szUnquotedPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
		{
			DebugLog("Failed to get the path of current module, errno = %d.\n", GetLastError());
			break;
		}

		// 将短路径转换为长路径格式
		TCHAR szLongPath[MAX_PATH];
		DWORD dwLen = GetLongPathName(szUnquotedPath, szLongPath, MAX_PATH);
		if (dwLen > MAX_PATH || dwLen <= 0)
		{
			DebugLog("GetLongPathName failed, errno = %d.\n", GetLastError());
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
			DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
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
			DebugLog("CreateService failed, errno = %d.\n", GetLastError());
			break;
		}		
		DebugLog("Service installed successfully!\n");

		// 修改服务描述信息
		SERVICE_DESCRIPTION sd;
		sd.lpDescription = (LPWSTR)std::wstring(DESCRIPTION).c_str();
		if (!ChangeServiceConfig2(
			schService,                 // handle to service
			SERVICE_CONFIG_DESCRIPTION, // change: description
			&sd))                       // new description
		{
			DebugLog("ChangeServiceConfig2 failed, errno = %d.\n", GetLastError());
			break;
		}
		DebugLog("Service description updated successfully!\n");

		res = true;
	} while (0);

	if(schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if(schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return res;
}

bool CMyTestServiceInstaller::uninstallService()
{
	DebugLog("Service uninstalling...!");
	
	bool res = false;
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	do
	{
		// 先停服务
		if (!stopService())
		{
			DebugLog("Failed to stop MyTestService, terminate it.\n");
		}
		else
		{
			DebugLog("Service stopped successfully.\n");
		}
			
		// 获取SCM数据库句柄
		schSCManager = OpenSCManager(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		schService = OpenService(
			schSCManager,	// SCM database 
			SVCNAME,		// name of service 
			DELETE);		// need delete access 

		if (schService == NULL)
		{
			DebugLog("OpenService failed, errno = %d.\n", GetLastError());
			break;
		}

		// 删除服务
		if (!DeleteService(schService))
		{
			DebugLog("DeleteService failed, errno = %d.\n", GetLastError());
			break;
		}

		DebugLog("Service deleted successfully.\n");
		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // 关闭服务句柄

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // 关闭SCM数据库句柄

	return res;
}

void CMyTestServiceInstaller::waitPendingService(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		DebugLog("Service stop pending...\n");
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
			DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
			break;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			DebugLog("Service stopped successfully.\n");
			break;
		}

		if (GetTickCount() - dwStartTime > dwTimeout)
		{
			DebugLog("Service stop timed out.\n");
			break;
		}
	}
}

void CMyTestServiceInstaller::enSureServiceStop(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp))
	{
		DebugLog("ControlService failed, errno = %d.\n", GetLastError());
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
				DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
				break;
			}

			if (ssp.dwCurrentState == SERVICE_STOPPED)
			{
				DebugLog("Service stopped successfully. \n");
				break;
			}

			if (GetTickCount() - dwStartTime > dwTimeout)
			{
				DebugLog("Wait timed out. \n");
				break;
			}
		}
	}
}

bool CMyTestServiceInstaller::stopService()
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
			DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		schService = OpenService(
			schSCManager,			// SCM database 
			SVCNAME,				// name of service 
			SERVICE_STOP |
			SERVICE_QUERY_STATUS);	// need access 

		if (schService == NULL)
		{
			DebugLog("OpenService failed, errno = %d.\n", GetLastError());
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
			DebugLog("QueryServiceStatusEx failed, errno = %d.\n", GetLastError());
		}
		else
		{
			if (ssp.dwCurrentState == SERVICE_STOPPED)
			{
				DebugLog("Service is already stopped.\n");
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

void CMyTestServiceInstaller::DebugLog(const char* fmt, ...)
{
#define LOG_BUF_SIZE 1024
	char buf[LOG_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(buf, LOG_BUF_SIZE - 1, fmt, ap);
	va_end(ap);

	char outputBuf[LOG_BUF_SIZE] = { 0 };
	_snprintf_s(outputBuf, LOG_BUF_SIZE - 1, "---MyTestService---:%-5d %-5d %s\n", GetCurrentProcessId(), GetCurrentThreadId(), buf);
	OutputDebugStringA(outputBuf);
}