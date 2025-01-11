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
	SC_HANDLE schSCManager = NULL; // SCM���ݿ���
	SC_HANDLE schService = NULL; // ������

	do
	{
		MyTestServiceUtils::DebugLog("Try to uinstall MyTestService.");
		uninstallService();

		// ��ֹ�ϴ�ж�ط���ʱ���񱻱����ǿɱһ��
		killServiceProcess();
		Sleep(3 * 1000);

		// ȡ��ǰģ��·��
		TCHAR szUnquotedPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
		{
			MyTestServiceUtils::DebugLog("Failed to get the path of current module, errno = %d.\n", GetLastError());
			break;
		}

		// ����·��ת��Ϊ��·����ʽ
		TCHAR szLongPath[MAX_PATH];
		DWORD dwLen = GetLongPathName(szUnquotedPath, szLongPath, MAX_PATH);
		if (dwLen > MAX_PATH || dwLen <= 0)
		{
			MyTestServiceUtils::DebugLog("GetLongPathName failed, errno = %d.\n", GetLastError());
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
			MyTestServiceUtils::DebugLog("OpenSCManager failed, errno = %d.\n", GetLastError());
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
			MyTestServiceUtils::DebugLog("CreateService failed, errno = %d.\n", GetLastError());
			break;
		}		
		MyTestServiceUtils::DebugLog("Service installed successfully!\n");

		// �޸ķ���������Ϣ
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
		CloseServiceHandle(schService); // �رշ�����

	if(schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

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

		Sleep(5 * 1000); // �ȴ������˳�
		killServiceProcess();
			
		// ��ȡSCM���ݿ���
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

		// ɾ������
		if (!DeleteService(schService))
		{
			MyTestServiceUtils::DebugLog("DeleteService failed, errno = %d.\n", GetLastError());
			break;
		}

		// ��ֹ����ֹͣʧ�ܣ�ǿɱ
		killServiceProcess();

		MyTestServiceUtils::DebugLog("Service deleted successfully.\n");
		res = true;
	} while (0);

	if (schService != NULL)
		CloseServiceHandle(schService); // �رշ�����

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

	return res;
}

void CMyTestServiceController::waitPendingService(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout)
{
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		MyTestServiceUtils::DebugLog("Service stop pending...\n");
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
		// ��ȡSCM���ݿ���
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

int CMyTestServiceController::getServicePid()
{
	int pid = -1;
	SC_HANDLE schSCManager = NULL; // SCM���ݿ���
	SC_HANDLE schService = NULL; // ������

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
		CloseServiceHandle(schService); // �رշ�����

	if (schSCManager != NULL)
		CloseServiceHandle(schSCManager); // �ر�SCM���ݿ���

	return pid;
}