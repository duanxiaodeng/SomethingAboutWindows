#include "MyTestServiceUtils.h"
#include <Shlwapi.h>
#include <Shlobj.h>
#include <io.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <TlHelp32.h>
#include <WtsApi32.h>
#include <wincrypt.h>
#include <winsock.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace MyTestServiceUtils
{
	bool terminateProcessByID(int pid)
	{
		return TerminateProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pid), 0);
	}

	std::string getProgramDataDir()
	{
		int ok = -1;
		static std::string dir;
		while (dir.empty())
		{
			CHAR szPath[MAX_PATH + 1];
			memset(szPath, 0, MAX_PATH + 1);
			//Be carefull, in COM Service, getLocalDataPath() returns C:\Windows\SysWOW64\C
			ok = SHGetSpecialFolderPathA(NULL, szPath, CSIDL_COMMON_APPDATA, FALSE);
			if (ok)
			{
				dir = szPath;
				break;
			}

			memset(szPath, 0, MAX_PATH + 1);
			ok = GetEnvironmentVariableA("APPDATA", szPath, MAX_PATH);
			if (ok)
			{
				dir = szPath;
				break;
			}
			memset(szPath, 0, MAX_PATH + 1);
			ok = GetEnvironmentVariableA("TEMP", szPath, MAX_PATH);
			if (ok)
			{
				dir = szPath;
				break;
			}
			dir.clear();
			return dir;
		}

		return dir;
	}

	// copy from CA2WEX
	wchar_t* GA2WSTR(char const* psz, int codepage/* = CP_ACP*/)
	{
		wchar_t* pwsz = 0;
		if (psz == NULL)
			return pwsz;

		int nLengthA = lstrlenA(psz) + 1;
		int nLengthW = nLengthA;

		pwsz = new wchar_t[nLengthW]();
		if (0 == ::MultiByteToWideChar(codepage, 0, psz, nLengthA, pwsz, nLengthW))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				nLengthW = ::MultiByteToWideChar(codepage, 0, psz, nLengthA, NULL, 0);
				delete[] pwsz;
				pwsz = new wchar_t[nLengthW]();
				if ((0 == ::MultiByteToWideChar(codepage, 0, psz, nLengthA, pwsz, nLengthW)))
				{
					delete[] pwsz;
					return 0;
				}
			}
		}

		return pwsz;
	}

	// copy from CW2AEX
	char* GW2ASTR(wchar_t const* pwsz, int codepage/* = CP_ACP*/)
	{
		char* psz = 0;
		if (pwsz == NULL)
			return psz;

		int nLengthW = lstrlenW(pwsz) + 1;
		int nLengthA = nLengthW * 4;

		psz = new char[nLengthA]();
		if (0 == ::WideCharToMultiByte(codepage, 0, pwsz, nLengthW, psz, nLengthA, NULL, NULL))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				nLengthA = ::WideCharToMultiByte(codepage, 0, pwsz, nLengthW, NULL, 0, NULL, NULL);
				delete[] psz;
				psz = new char[nLengthA]();
				if (0 == ::WideCharToMultiByte(codepage, 0, pwsz, nLengthW, psz, nLengthA, NULL, NULL))
				{
					delete[] psz;
					return 0;
				}
			}
		}

		return psz;
	}

	std::wstring GA2W(std::string const& str, int codepage /*= CP_ACP*/)
	{
		wchar_t* tmp = GA2WSTR(str.c_str(), codepage);
		std::wstring res = tmp;
		delete[] tmp;
		return res;
	}

	std::string GW2A(std::wstring const& wstr, int codepage /*= CP_ACP*/)
	{
		char* tmp = GW2ASTR(wstr.c_str(), codepage);
		std::string res = tmp;
		delete[] tmp;
		return res;
	}

	bool ensureDirExist(std::string const& path, bool hidden /*=false*/)
	{
		return ensureDirExist(path, NULL, NULL, INVALID_HANDLE_VALUE, hidden);
	}

	bool ensureDirExist(std::string const& path,
		FunCreateDirectoryTransacted createDirectoryTransacted,
		FunSetFileAttributesTransacted setFileAttributesTransacted,
		HANDLE hTransact, bool hidden)
	{
		bool ret = false;
		std::string dir;
		int n = 0, m = 0;
		do
		{
			n = path.find('\\', m);
			if (n == std::string::npos)
				n = path.find('/', m);
			if (n != std::string::npos)
				dir = path.substr(0, n + 1/*with trailing slash to ensure C:\ not C: */);
			else
				break; // at last, this is a file path.

			if (-1 == _access(dir.c_str(), 0))
			{
				//ok = _mkdir(dir.c_str());
				if (createDirectoryTransacted && hTransact != INVALID_HANDLE_VALUE)
					ret = createDirectoryTransacted(NULL, dir.c_str(), NULL, hTransact);
				else
					ret = CreateDirectoryA(dir.c_str(), NULL);

				if (!ret)
				{
					if (GetLastError() != ERROR_ALREADY_EXISTS)
					{
						return false;
					}
				}
			}

			if (hidden)
			{
				if (setFileAttributesTransacted && hTransact != INVALID_HANDLE_VALUE)
					ret = setFileAttributesTransacted(dir.c_str(), FILE_ATTRIBUTE_HIDDEN, hTransact);
				else
					ret = SetFileAttributesA(dir.c_str(), FILE_ATTRIBUTE_HIDDEN);
			}

			m = n + 1;
		} while (n != std::string::npos);

		return true;
	}

	void DebugLog(const char* fmt, ...)
	{
		#define LOG_BUF_SIZE 1024
		char buf[LOG_BUF_SIZE] = { 0 };
		va_list ap;
		va_start(ap, fmt);
		_vsnprintf_s(buf, LOG_BUF_SIZE - 1, fmt, ap);
		va_end(ap);

		char outputBuf[LOG_BUF_SIZE] = { 0 };
		_snprintf_s(outputBuf, LOG_BUF_SIZE - 1, "---MyTestService---:%-5d %-5d %s\n", GetCurrentProcessId(), GetCurrentThreadId(), buf);
		OutputDebugStringA(outputBuf);
	}

	GUID CreateGuid()
	{
		GUID guid;
		CoCreateGuid(&guid);
		return guid;
	}

	std::string GuidToString(const GUID& guid)
	{
		char buf[64] = { 0 };
#ifdef __GNUC__
		snprintf(
#else // MSVC
		_snprintf_s(
#endif
			buf,
			sizeof(buf),
			"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1],
			guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);
		return std::string(buf);
	}

	GUID StringToGuid(const std::string& str)
	{
		GUID guid;
		sscanf(str.c_str(),
			"{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
			&guid.Data1, &guid.Data2, &guid.Data3,
			&guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
			&guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

		return guid;
	}
}