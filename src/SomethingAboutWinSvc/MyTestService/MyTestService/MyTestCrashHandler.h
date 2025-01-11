#pragma once
#include <Windows.h>

class CMyTestServiceCrashHandler
{
public:
	static void registerCrashHandler(bool isRegister = true);

private:
	static LONG __stdcall MyUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo); // �����ص�����
	static void CreateMiniDump(PEXCEPTION_POINTERS pep, LPCTSTR strFileName); // ����dump�ļ�

private:
	static LPTOP_LEVEL_EXCEPTION_FILTER LastFilter;
};