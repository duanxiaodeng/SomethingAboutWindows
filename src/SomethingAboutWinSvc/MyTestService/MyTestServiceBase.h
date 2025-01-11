#pragma once
#include <windows.h>
#include <string>
#include "MyTestServiceInstaller.h"

class CMyTestServiceBase
{
public:
	CMyTestServiceBase();
	CMyTestServiceBase(const CMyTestServiceBase& service) = delete;
	CMyTestServiceBase& operator=(const CMyTestServiceBase& service) = delete;
	CMyTestServiceBase(CMyTestServiceBase&& service) = delete;
	CMyTestServiceBase& operator=(CMyTestServiceBase&& service) = delete;
	virtual ~CMyTestServiceBase();

	bool run(int argc, char** argv);

public:
	enum AppControlCode
	{
		ACC_PAUSE,
		ACC_RESUME,
		ACC_STOP
	};

protected:
	virtual void onStart() {};
	virtual void onStop() {}
	virtual void onPause() {}
	virtual void onResume() {}
	virtual void onShutdown() {}
	virtual void onSessionChanged(DWORD dwEventType, WTSSESSION_NOTIFICATION* notification) {}
	
private:
	static bool serviceRun(CMyTestServiceBase* service);
	static void WINAPI serviceMain(int argc, char* argv[]);
	static DWORD WINAPI serviceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
	void ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode = NO_ERROR, DWORD dwWaitHint = 0);

	void start();
	void stop();
	void pause();
	void resume();
	void shutdown();
	void sessionChange(DWORD dwEventType, LPVOID lpEventData);
	void interrogate();

private:
	DWORD					m_dwCurrentState;			// ����ǰ״̬
	DWORD					m_dwControlsAccepted;		// ����ɿ��Ʋ���
	SERVICE_STATUS          m_SvcStatus;				// ����״̬�ṹ
	SERVICE_STATUS_HANDLE   m_SvcStatusHandle = NULL;	// ����״̬���
	std::string				m_svcName;

	static CMyTestServiceBase* m_pService;
};