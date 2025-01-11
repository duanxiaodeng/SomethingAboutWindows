#pragma once
#include "MyTestServiceBase.h"

class CMyTestService : public CMyTestServiceBase
{
public:
	virtual ~CMyTestService();
	static CMyTestService* getInstance();

	virtual void onStart();
	virtual void onStop();

private:
	CMyTestService();
	bool buildGlobalEvent();

private:
	HANDLE m_hMutex = NULL;
};