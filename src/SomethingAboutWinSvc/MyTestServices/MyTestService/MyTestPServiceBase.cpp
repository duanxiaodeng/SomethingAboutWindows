#include "MyTestServiceBase.h"
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include <cassert>

CMyTestServiceBase* CMyTestServiceBase::m_pService = NULL;

CMyTestServiceBase::CMyTestServiceBase()
	: m_dwCurrentState(SERVICE_STOPPED)
{
	m_dwControlsAccepted = SERVICE_ACCEPT_STOP	// The service can be stopped.
				| SERVICE_ACCEPT_PAUSE_CONTINUE	// The service can be paused and continued.
				| SERVICE_ACCEPT_SESSIONCHANGE	// The service is notified when the computer's session status has changed.
				| SERVICE_ACCEPT_SHUTDOWN;		// The service is notified when system shutdown occurs.

	memset(&m_SvcStatus, 0, sizeof(m_SvcStatus));
	m_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // 服务类型：独立进程服务
	m_SvcStatus.dwServiceSpecificExitCode = 0;
}

CMyTestServiceBase::~CMyTestServiceBase()
{
}

bool CMyTestServiceBase::run(int argc, char **argv)
{
	return serviceRun(this);
}

bool CMyTestServiceBase::serviceRun(CMyTestServiceBase* service)
{
	m_pService = service;
		
	// 服务入口点表
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ (LPWSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION)(serviceMain)},
		{ NULL, NULL } // 数组必须以此结尾
	};

	return StartServiceCtrlDispatcher(DispatchTable) == TRUE;
}

void WINAPI CMyTestServiceBase::serviceMain(int argc, char* argv[])
{
	assert(m_pService);
	// 注册服务的控制处理函数
	m_pService->m_SvcStatusHandle = RegisterServiceCtrlHandlerExA(
		m_pService->m_svcName.c_str(),
		serviceCtrlHandlerEx,
		NULL);

	if (!m_pService->m_SvcStatusHandle)
	{
		return;
	}

	m_pService->start();
}

void CMyTestServiceBase::ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	m_dwCurrentState = dwCurrentState;
	static DWORD dwCheckPoint = 1;

	m_SvcStatus.dwCurrentState = dwCurrentState;
	m_SvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	m_SvcStatus.dwWaitHint = dwWaitHint;

	if ((dwCurrentState != SERVICE_START_PENDING) &&
		(dwCurrentState != SERVICE_STOP_PENDING) &&
		(dwCurrentState != SERVICE_CONTINUE_PENDING) &&
		(dwCurrentState != SERVICE_PAUSE_PENDING))
	{
		m_SvcStatus.dwControlsAccepted = m_dwControlsAccepted;
		m_SvcStatus.dwCheckPoint = 0;
	}
	else
	{
		m_SvcStatus.dwControlsAccepted = 0;
		m_SvcStatus.dwCheckPoint = dwCheckPoint++;
	}

	// 向SCM报告服务状态
	SetServiceStatus(m_SvcStatusHandle, &m_SvcStatus);
}

DWORD WINAPI CMyTestServiceBase::serviceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
		m_pService->stop();
		break;

	case SERVICE_CONTROL_PAUSE:
		m_pService->pause();
		break;

	case SERVICE_CONTROL_CONTINUE:
		m_pService->resume();
		break;

	case SERVICE_CONTROL_SHUTDOWN:
		m_pService->shutdown();
		break;

	case SERVICE_CONTROL_SESSIONCHANGE:
		m_pService->sessionChange(dwEventType, lpEventData);
		break;

	case SERVICE_CONTROL_INTERROGATE:
		m_pService->interrogate();
		break;

	default:
		break;
	}
	return NO_ERROR;
}

void CMyTestServiceBase::start()
{
	ReportStatusToSCMgr(SERVICE_START_PENDING);
	onStart();
	ReportStatusToSCMgr(SERVICE_RUNNING);
}

void CMyTestServiceBase::stop()
{
	ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	onStop();
	ReportStatusToSCMgr(SERVICE_STOPPED);
}

void CMyTestServiceBase::pause()
{
	ReportStatusToSCMgr(SERVICE_PAUSE_PENDING);
	onPause();
	ReportStatusToSCMgr(SERVICE_PAUSED);
}

void CMyTestServiceBase::resume()
{
	ReportStatusToSCMgr(SERVICE_CONTINUE_PENDING);
	onResume();
	ReportStatusToSCMgr(SERVICE_RUNNING);
}

void CMyTestServiceBase::shutdown()
{
	onShutdown();
	ReportStatusToSCMgr(SERVICE_STOPPED);
}

void CMyTestServiceBase::sessionChange(DWORD dwEventType, LPVOID lpEventData)
{
	onSessionChanged(dwEventType, reinterpret_cast<WTSSESSION_NOTIFICATION*>(lpEventData));
}

void CMyTestServiceBase::interrogate()
{
	ReportStatusToSCMgr(m_dwCurrentState);
}