#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

#ifdef _DEBUG
#ifndef X64 
#define APIHOOK_DLL "../Debug/APIHookDLL.dll"
#else
#define APIHOOK_DLL "../x64/Debug/APIHookDLL.dll"
#endif
#else
#ifndef X64 
#define APIHOOK_DLL  "../Release/APIHookDLL.dll"
#else
#define APIHOOK_DLL "../x64/Release/APIHookDLL.dll"
#endif
#endif // _DEBUG

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
		return false;

	//1. ���Ѵ��ڵ�Ŀ�����
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
	{
		OutputDebugString("Cannot open this process.\n");
		return false;
	}

	//2. ������Ŀ����̴����ڴ�
	LPVOID lpAddr = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_COMMIT, PAGE_READWRITE);
	if (lpAddr == NULL)
	{
		OutputDebugString("Cannot alloc memory.\n");
		return false;
	}

	char buf[0x100];
	sprintf_s(buf, "Alloc memory address: %p \n", lpAddr);
	OutputDebugString(buf);

	//3. д���ڴ�
	BOOL isOk = WriteProcessMemory(hProcess		//Ҫд���Ŀ�����
		, lpAddr		//д��Ŀ����̵�λ��
		, dllPath.c_str() //д�������
		, dllPath.length() //д��ĳ���
		, NULL);
	if (!isOk)
	{
		OutputDebugString("Cannot write memory.\n");
		return false;
	}

	//4. ��ȡ�����ĵ�ַ
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

	//5. ��Զ�̵Ľ��̴���һ���̵߳���LoadLibraryA
	HANDLE hThread = CreateRemoteThread(hProcess
		, NULL
		, 0
		, (LPTHREAD_START_ROUTINE)hFarProc //������ַ
		, lpAddr //�����̵߳Ĳ���,������Ŀ������ܷ��ʵ��ĵ�ַ
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
		return false;

	//1. ���Ѵ��ڵ�Ŀ�����
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
	{
		OutputDebugString("Cannot open this process.\n");
		return false;
	}

	//2. ��ȡ�����ĵ�ַ
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

	//3. ��ȡģ���� 
	HMODULE hModule = GetModuleHandleByName(ModuleName.c_str(), pid);
	if (hModule == NULL)
	{
		OutputDebugString("Cannot find this module.\n");
		return false;
	}

	//4. ��Զ�̵Ľ��̴���һ���̵߳���FreeLibrary
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0
		, (LPTHREAD_START_ROUTINE)hFarProc //������ַ
		, hModule //�����̵߳Ĳ���,������Ŀ������ܷ��ʵ��ĵ�ַ
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
	std::string processName = "APIHookTest.exe";
	int pid = GetPidByProcessName(processName);
	std::cout << "Ҫע��Ľ���ID��" << pid << std::endl;
	std::string ModuleName = "APIHookDLL.dll";
	if (InjectDllToProcess(pid, APIHOOK_DLL))
		std::cout << "ע��ɹ�" << std::endl;

	getchar();
	UninjectDllFromProcess(pid, ModuleName);
	system("pause");
	return 0;
}