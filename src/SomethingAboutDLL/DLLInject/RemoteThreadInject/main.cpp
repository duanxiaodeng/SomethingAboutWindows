// myDllInjectTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

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

HMODULE GetModuleHandleByName(const char* ModuleName, DWORD pid)
{
#ifdef x64
	DWORD dwFlags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;
#else
	DWORD dwFlags = TH32CS_SNAPMODULE;
#endif // WIN32
	HANDLE Processes = CreateToolhelp32Snapshot(dwFlags, pid);
	MODULEENTRY32 ModuleInfo = { 0 };
	ModuleInfo.dwSize = sizeof(MODULEENTRY32);
	char buf[0x100];

	while (Module32Next(Processes, &ModuleInfo))
	{
		if (strcmp(ModuleInfo.szModule, ModuleName) == 0)
			return ModuleInfo.hModule;
	}

	return NULL;
}

bool InjectDllToProcess(int pid, std::string dllPath)
{
	if (pid <= 0 || dllPath.empty())
	{
		std::cout << "进程不存在或DLL路径为空" << std::endl;
		return false;
	}
		
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

	//4. 获取函数的地址
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

	//5. 在远程的进程创建一个线程调用LoadLibraryA
	HANDLE hThread = CreateRemoteThread(hProcess
		, NULL
		, 0
		, (LPTHREAD_START_ROUTINE)hFarProc //函数地址
		, lpAddr //传给线程的参数,必须是目标进程能访问到的地址
		, 0
		, NULL);

	if (hThread == NULL) {
		DWORD err = GetLastError();
		OutputDebugString("Cannot create thread to call LoadLibraryA.\n");
		return false;
	}

	WaitForSingleObject(hThread, INFINITE);
	VirtualFreeEx(hThread, lpAddr, 0, MEM_RELEASE);

	CloseHandle(hThread);
	CloseHandle(hProcess);
	return true;
}

int UninjectDllFromProcess(int pid, std::string ModuleName)
{
	if (pid <= 0 || ModuleName.empty())
	{
		std::cout << "进程不存在或模块为空" << std::endl;
		return false;
	}

	//1. 打开已存在的目标进程
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
	{
		OutputDebugString("Cannot open this process.\n");
		return false;
	}

	//2. 获取函数的地址
	HMODULE hKernel32Module = GetModuleHandle("kernel32.dll");
	if (hKernel32Module == NULL)
	{
		OutputDebugString("Cannot find kernel32.dll.\n");
		return false;
	}

	FARPROC hFarProc = GetProcAddress(hKernel32Module, "FreeLibrary");
	if (hFarProc == NULL)
	{
		OutputDebugString("Cannot get function address.\n");
		return false;
	}

	//3. 获取模块句柄 
	HMODULE hModule = GetModuleHandleByName(ModuleName.c_str(), pid);
	if (hModule == NULL)
	{
		OutputDebugString("Cannot find this module.\n");
		return false;
	}

	//4. 在远程的进程创建一个线程调用FreeLibrary
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0
		, (LPTHREAD_START_ROUTINE)hFarProc //函数地址
		, hModule //传给线程的参数,必须是目标进程能访问到的地址
		, 0
		, NULL
	);

	if (hThread == NULL)
	{
		OutputDebugString("Cannot create thread to call FreeLibrary.\n");
		return false;
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	CloseHandle(hProcess);
	return 0;
}

int main()
{
	int pid = GetPidByProcessName(PROCESS_NAME);
	InjectDllToProcess(pid, INJECTDLL_PATH);
	getchar();
	UninjectDllFromProcess(pid, MODULE_NAME);
	system("pause");
	return 0;
}