#pragma once
#include "MyTestServiceBase.h"

#define SVCNAME TEXT("MyTestService")
#define DESCRIPTION TEXT("我的测试服务")
#define DEPENDENCY TEXT("Tcpip")

class CMyTestServiceController
{
public:
	static bool installService();
	static bool uninstallService();

private:
	static bool killServiceProcess();
	static bool stopService();
	static void waitPendingService(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout);
	static void enSureServiceStop(SC_HANDLE& schService, SERVICE_STATUS_PROCESS& ssp, DWORD& dwBytesNeeded, DWORD dwStartTime, DWORD dwTimeout);
	static int  getServicePid();
};