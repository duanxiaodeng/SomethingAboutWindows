#include "MyTestService.h"
#include "MyTestServiceUtils.h"

CMyTestService::CMyTestService()
{
}

CMyTestService::~CMyTestService()
{
}

CMyTestService* CMyTestService::getInstance()
{
	static CMyTestService ins;
	return &ins;
}

void CMyTestService::onStart()
{
	// 保证进程单例运行
	if (!buildGlobalEvent())
	{
		exit(0);
	}

	// Todo...
}

void CMyTestService::onStop()
{
	// 释放全局互斥量
	ReleaseMutex(m_hMutex);
	CloseHandle(m_hMutex);
	// Todo...
}

bool CMyTestService::buildGlobalEvent()
{
	bool ret = false;
	int loop = 10;
	do
	{
		SECURITY_DESCRIPTOR sd;
		InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = &sd;
		sa.bInheritHandle = FALSE;
		m_hMutex = CreateMutex(&sa, FALSE, TEXT("Global\\{6E023156-6693-4C70-A710-A6D123C3C15C}"));
		DWORD dwRet = GetLastError();
		if (ERROR_ALREADY_EXISTS == dwRet || m_hMutex == NULL)
		{
			MyTestServiceUtils::DebugLog("Create mutex for MyTestService failed, errno: %ld. And try it 3 second later.", dwRet);
			Sleep(3000);
		}
		else
		{
			MyTestServiceUtils::DebugLog("Create mutex for MyTestService success.");
			ret = true;
			break;
		}

	} while (loop--);

	return ret;
}