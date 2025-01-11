#include "MyTestService.h"

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
	// Todo...
}

void CMyTestService::onStop()
{
	// Todo...
}