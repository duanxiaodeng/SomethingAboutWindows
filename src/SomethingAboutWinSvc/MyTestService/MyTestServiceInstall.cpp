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
	SC_HANDLE schSCManager = NULL; // SCM���ݿ���
	SC_HANDLE schService = NULL; // ������

	do
	{
		// ȡ��ǰģ��·��
		TCHAR szUnquotedPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
		{
			DebugLog("Failed to get the path of current module, errno = %d.\n", GetLastError());
			break;
		}

		// ����·��ת��Ϊ��·����ʽ
		TCHAR szLongPath[MAX_PATH];
		DWORD dwLen = GetLongPathName(szUnquotedPath, szLongPath, MAX_PATH);
		if (dwLen > MAX_PATH || dwLen <= 0)
		{
			DebugLog("GetLongPathName failed, errno = %d.\n", GetLastError());
			std::copy(szUnquotedPath, szUnquotedPath + std::wcslen(szUnquotedPath) + 1, szLongPath); // szLongPath = szUnquotedPath
		}

		// ��ֹ·�������ո񣬱��뱻�������������Ա���ȷ����
		TCHAR szPath[MAX_PATH];
		StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szLongPath);
		
		// SCM���ݿ�ľ��
		schSCManager = OpenSCManagerA(
			NULL,                    // local computer
			NULL,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (schSCManager == NULL)
		{
			DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
			break;
		}

		// ��������
		schService = CreateService(
			schSCManager,							// SCM���ݿ� 
			SVCNAME,								// ������
			SVCNAME,								// ������ʾ���� 
			SERVICE_ALL_ACCESS,						// ��������Ȩ�� 
			SERVICE_WIN32_OWN_PROCESS,				// ��������
			SERVICE_AUTO_START,						// ������������
			SERVICE_ERROR_NORMAL,					// ����������� 
			szPath,									// �������·��
			L"",									// ����������У�no load ordering group 
			NULL,									// �����ʶ��ǩ��no tag identifier 
			DEPENDENCY,								// ��������
			NULL,									// �û���NULL��ʾLocalSystem�˻�
			NULL);									// ����

		if (schService == NULL)
		{
			DebugLog("CreateService failed, errno = %d.\n", GetLastError());
			break;
		}		
		DebugLog("Service installed successfully!\n");

		// �޸ķ���������Ϣ
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
		CloseServiceHandle(schService); // �رշ�����

	if(schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

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
		// ��ͣ����
		if (!stopService())
		{
			DebugLog("Failed to stop MyTestService, terminate it.\n");
		}
		else
		{
			DebugLog("Service stopped successfully.\n");
		}
			
		// ��ȡSCM���ݿ���
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

		// ɾ������
		if (!DeleteService(schService))
		{
			DebugLog("DeleteService failed, errno = %d.\n", GetLastError());
			break;
		}

		DebugLog("Service deleted successfully.\n");
		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // �رշ�����

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

	return res;
}

void CMyTestServiceInstaller::waitPendingService(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		DebugLog("Service stop pending...\n");
		// �ȴ�ʱ�䲻Ҫ�����ȴ���ʾ��
		// �������ǵȴ���ʾ��ʮ��֮һ����������1�룬Ҳ������10�롣
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
		// ��ȡSCM���ݿ���
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

		// ֹͣ����
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
				// ����ֹͣ���Ƹ�����
				enSureServiceStop(schService, ssp, dwBytesNeeded, dwStartTime, dwTimeout);
			}
		}

		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // �رշ�����

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

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