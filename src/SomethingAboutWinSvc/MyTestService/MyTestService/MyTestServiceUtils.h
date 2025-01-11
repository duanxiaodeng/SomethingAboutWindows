#pragma once
#include <Windows.h>
#include <string>

namespace MyTestServiceUtils
{
	bool terminateProcessByID(int pid);
	std::string getProgramDataDir();
	// should delete[] after use
	wchar_t* GA2WSTR(char const* psz, int codepage = CP_ACP);
	char* GW2ASTR(wchar_t const* pwsz, int codepage = CP_ACP);
	// add G to avoid conflict with A2W & W2A in Windows
	std::wstring GA2W(std::string const& str, int codepage = CP_ACP);
	std::string GW2A(std::wstring const& wstr, int codepage = CP_ACP);
	typedef BOOL(WINAPI* FunCreateDirectoryTransacted)(
		LPCSTR lpTemplateDirectory,
		LPCSTR lpNewDirectory,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		HANDLE hTransaction
		);

	typedef BOOL(WINAPI* FunSetFileAttributesTransacted)(
		LPCSTR lpFileName,
		DWORD dwFileAttributes,
		HANDLE hTransaction
		);

	bool ensureDirExist(std::string const& dir, bool hidden = false);
	bool ensureDirExist(std::string const& dir,
		FunCreateDirectoryTransacted createDirectoryTransacted,
		FunSetFileAttributesTransacted setFileAttributeTransacted,
		HANDLE hTransact, bool hidden);
	void DebugLog(const char* fmt, ...);

	GUID CreateGuid();
	std::string GuidToString(const GUID& guid);
	GUID StringToGuid(const std::string& str);
}
