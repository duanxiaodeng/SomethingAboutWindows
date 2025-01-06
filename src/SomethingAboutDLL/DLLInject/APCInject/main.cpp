#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#ifdef _DEBUG
#ifndef X64 
#define INJECTDLL_PATH "G:\\1-工作内容\\15-GDP\\6-测试程序\\77-DLL相关内容\\3-DLL注入\\DLLInject\\Debug\\injectDLL.dll"
#else
#define INJECTDLL_PATH "G:\\1-工作内容\\15-GDP\\6-测试程序\\77-DLL相关内容\\3-DLL注入\\DLLInject\\x64\\Debug\\injectDLL.dll"
#endif
#else
#ifndef X64 
#define INJECTDLL_PATH "G:\\1-工作内容\\15-GDP\\6-测试程序\\77-DLL相关内容\\3-DLL注入\\DLLInject\\x64\\Release\\injectDLL.dll"
#else
#define INJECTDLL_PATH "G:\\1-工作内容\\15-GDP\\6-测试程序\\77-DLL相关内容\\3-DLL注入\\DLLInject\\Release\\injectDLL.dll"
#endif
#endif // _DEBUG

#define PROCESS_NAME "VisDraw.exe"
#define MODULE_NAME "injectDLL.dll"

int GetPidByProcessName(std::string ProcessName)
{
	DWORD dwPid = -1;
	PROCESSENTRY32 entry;
	memset(&entry, 0, sizeof(entry));
	entry.dwSize = sizeof(entry);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(hSnapshot, &entry))
	{
		do
		{
			if (strcmp(entry.szExeFile, ProcessName.c_str()) == 0)
			{
				dwPid = entry.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &entry));
	}

	CloseHandle(hSnapshot);
	return dwPid;
}

bool GetThreadListFromPid(DWORD dwPid, std::vector<DWORD>& dwThreadIDs)
{
	THREADENTRY32 entry;
	memset(&entry, 0, sizeof(entry));
	entry.dwSize = sizeof(entry);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return false;

	if (Thread32First(hSnapshot, &entry))
	{
		do
		{
			if (dwPid == entry.th32OwnerProcessID)
			{
				dwThreadIDs.push_back(entry.th32ThreadID);
			}
		} while (Thread32Next(hSnapshot, &entry));
	}

	CloseHandle(hSnapshot);
	return dwThreadIDs.size() > 0;
}

bool InjectModuleToProcessByPid(int pid, std::string dllPath)
{
	if (pid <= 0 || dllPath.empty())
		return false;

	//1. 打开已存在的目标进程
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
	{
		OutputDebugString("Cannot open this process.\n");
		return false;
	}

	//2. 创建在目标进程创建内存
	LPVOID lpAddr = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_COMMIT, PAGE_READWRITE);
	if (lpAddr == NULL)
	{
		OutputDebugString("Cannot alloc memory.\n");
		return false;
	}

	char buf[0x100];
	sprintf_s(buf, "Alloc memory address: %p \n", lpAddr);
	OutputDebugString(buf);

	//3. 写入内存
	BOOL isOk = WriteProcessMemory(hProcess		//要写入的目标进程
		, lpAddr		//写入目标进程的位置
		, dllPath.c_str() //写入的内容
		, dllPath.length() //写入的长度
		, NULL);
	if (!isOk)
	{
		OutputDebugString("Cannot write memory.\n");
		return false;
	}

	//4. 创建线程快照
	std::vector<DWORD> threadIDs;
	if (!GetThreadListFromPid(pid, threadIDs))
	{
		OutputDebugString("GetThreadListFromPid failed.\n");
		return false;
	}

	//5. 获取函数的地址
	HMODULE hKernel32Module = GetModuleHandle("kernel32.dll");
	if (hKernel32Module == NULL)
	{
		OutputDebugString("Cannot find kernel32.dll.\n");
		return false;
	}

	FARPROC hFarProc = GetProcAddress(hKernel32Module, "LoadLibraryA");
	if (hFarProc == NULL)
	{
		OutputDebugString("Cannot get function address.\n");
		return false;
	}

	// 6. 注入所有线程
	bool res = false;
	for (int i = 0; i < threadIDs.size(); i++)
	{
		HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadIDs[i]);
		if (hThread)
		{
			DWORD dwRet1 = QueueUserAPC((PAPCFUNC)LoadLibraryA, hThread, (ULONG_PTR)lpAddr);
			if (dwRet1 > 0)
			{
				res = true;
			}
			CloseHandle(hThread);
		}
	}

	VirtualFreeEx(hProcess, lpAddr, 0, MEM_RELEASE);
	CloseHandle(hProcess);
	return res;
}

int main()
{
	int pid = GetPidByProcessName(PROCESS_NAME);
	InjectModuleToProcessByPid(pid, INJECTDLL_PATH);

	system("pause");
	return 0;
}