#pragma once
#include <Windows.h>

class CMyTestServiceCrashHandler
{
public:
	static void registerCrashHandler(bool isRegister = true);

private:
	static LONG __stdcall MyUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo); // 崩溃回调函数
	static void CreateMiniDump(PEXCEPTION_POINTERS pep, LPCTSTR strFileName); // 生成dump文件

private:
	static LPTOP_LEVEL_EXCEPTION_FILTER LastFilter;
};