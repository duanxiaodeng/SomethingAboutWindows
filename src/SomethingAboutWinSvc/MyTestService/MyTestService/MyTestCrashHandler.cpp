#include "MyTestCrashHandler.h"
#include <string>
#include <Dbghelp.h>
#include "MyTestServiceUtils.h"

#pragma comment(lib, "DbgHelp")

LPTOP_LEVEL_EXCEPTION_FILTER CMyTestServiceCrashHandler::LastFilter = NULL;

void CMyTestServiceCrashHandler::registerCrashHandler(bool isRegister)
{
	if(isRegister)
		LastFilter = SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
	else
		SetUnhandledExceptionFilter(LastFilter);
}

LONG CMyTestServiceCrashHandler::MyUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
	MyTestServiceUtils::DebugLog("Some exception has occurred...");

	std::string dumpDir = MyTestServiceUtils::getProgramDataDir() + "\\MyTestService\\crashDump\\";
	MyTestServiceUtils::ensureDirExist(dumpDir);
	std::string dumpName = MyTestServiceUtils::GuidToString(MyTestServiceUtils::CreateGuid()) + ".dmp";
	std::string dumpPath = dumpDir + dumpName;
	MyTestServiceUtils::DebugLog("The path of dump is %s", dumpPath.c_str());

	std::wstring wstrDumpPath = MyTestServiceUtils::GA2W(dumpPath);
	CreateMiniDump(pExceptionInfo, wstrDumpPath.c_str());

	MyTestServiceUtils::DebugLog("Exception handling completed.");
	return EXCEPTION_EXECUTE_HANDLER;
}

void CMyTestServiceCrashHandler::CreateMiniDump(PEXCEPTION_POINTERS pep, LPCTSTR strFileName)
{
	MyTestServiceUtils::DebugLog("Create minidump for exception");
	HANDLE hFile = CreateFile(strFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = pep;
		mdei.ClientPointers = NULL;

		BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, (pep != 0) ? &mdei : 0, NULL, NULL);
		if(ok)
			MyTestServiceUtils::DebugLog("Dump file generated successfully.");
		else
			MyTestServiceUtils::DebugLog("MiniDumpWriteDump failed, errno = %d", GetLastError());

		CloseHandle(hFile);
	}
	else
	{
		MyTestServiceUtils::DebugLog("CreateFile failed, errno = ", GetLastError());
	}
}