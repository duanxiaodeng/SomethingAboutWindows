#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#ifdef _DEBUG
#ifndef X64 
#define INJECTDLL_PATH "G:\\1-��������\\15-GDP\\6-���Գ���\\77-DLL�������\\3-DLLע��\\DLLInject\\Debug\\injectDLL.dll"
#else
#define INJECTDLL_PATH "G:\\1-��������\\15-GDP\\6-���Գ���\\77-DLL�������\\3-DLLע��\\DLLInject\\x64\\Debug\\injectDLL.dll"
#endif
#else
#ifndef X64 
#define INJECTDLL_PATH "G:\\1-��������\\15-GDP\\6-���Գ���\\77-DLL�������\\3-DLLע��\\DLLInject\\x64\\Release\\injectDLL.dll"
#else
#define INJECTDLL_PATH "G:\\1-��������\\15-GDP\\6-���Գ���\\77-DLL�������\\3-DLLע��\\DLLInject\\Release\\injectDLL.dll"
#endif
#endif // _DEBUG

#define WINDOW_NAME "VisDraw - VisDraw1"
#define PROCESS_NAME "VisDraw.exe"
#define MODULE_NAME "injectDLL.dll"

#ifndef X64
bool eipInjectForX86(std::string windowName, std::string dllPath)
{
	unsigned char shellCode[] =
	{
		0x68, 0xef, 0xbe, 0xad, 0xde,	// push 0xDEADBEEF
		0x9c,							// pushfd
		0x60,							// pushad
		0x68, 0xef, 0xbe, 0xad, 0xde,	// push 0xDEADBEEF
		0xb8, 0xef, 0xbe, 0xad, 0xde,	// mov eax, 0xDEADBEEF
		0xff, 0xd0,						// call eax
		0x61,							// popad
		0x9d,							// popfd
		0xc3
	};

	// 1.��ȡҪע����̵Ĵ��ھ��
	HWND hwnd = FindWindow(NULL, windowName.c_str());
	if (hwnd == NULL)
		return false;

	// 2. ��ȡ����ID���߳�ID
	// �˴���ȡ�����̵߳��߳�ID���������û�д����̣߳����Ա��������߳��б���һ���������е��߳�������
	DWORD pid = 0;
	DWORD threadID = GetWindowThreadProcessId(hwnd, &pid);

	// 3. ��ȡ�߳̾���ͽ��̾��
	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadID);
	if (hThread == NULL)
		return false;

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
		return false;

	// 4. �����߳�
	SuspendThread(hThread);

	// 5. ��ȡLoadLibraryA������ַ
	HMODULE hKernel32Module = GetModuleHandle("kernel32.dll");
	if (hKernel32Module == NULL)
		return false;

	FARPROC hFarProc = GetProcAddress(hKernel32Module, "LoadLibraryA");

	// 6. �ڽ����з����ڴ�
	LPVOID lpDllAddr = VirtualAllocEx(hProcess, NULL, dllPath.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (lpDllAddr == NULL)
		return false;

	LPVOID lpNewEip = VirtualAllocEx(hProcess, NULL, sizeof(shellCode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (lpNewEip == NULL)
		return false;

	// 7. ��ȡָ���̵߳������Ļ���CONTEXT
	CONTEXT cont = { 0 };
	cont.ContextFlags = CONTEXT_CONTROL;
	bool res = GetThreadContext(hThread, &cont);

	// 9. �޸�shellCode���ݣ�����Ҫִ�еĻ�����
	DWORD oldEip = cont.Eip; // ������EIP��ַ��֮����Ҫ��ת��������ִ��
	memcpy((void *)((unsigned long)shellCode + 1), &oldEip, sizeof(oldEip));
	memcpy((void *)((unsigned long)shellCode + 8), &lpDllAddr, sizeof(lpDllAddr));
	memcpy((void *)((unsigned long)shellCode + 13), &hFarProc, sizeof(hFarProc));

	// 10. ��ָ���ڴ�д�����ݣ�dll·����Ҫִ�еĻ�����
	WriteProcessMemory(hProcess, lpDllAddr, dllPath.c_str(), dllPath.size(), 0);
	WriteProcessMemory(hProcess, lpNewEip, shellCode, sizeof(shellCode), 0);

	// 11. ��EIP�޸ĳ��·����ڴ�ĵ�ַ��ͬʱ�����߳������Ļ����ٰ��̷߳��£���������ִ�С�
	cont.Eip = (DWORD)lpNewEip;
	SetThreadContext(hThread, &cont);
	ResumeThread(hThread);

	// 12. �ͷ��ڴ棬�رվ��
	Sleep(10 * 1000); // �����ͷ��ڴ���ܻᵼ�½��̱�����Ҫô��һ�����ͷţ�Ҫô���ͷ�
	VirtualFreeEx(hProcess, lpDllAddr, dllPath.size(), MEM_DECOMMIT);
	VirtualFreeEx(hProcess, lpNewEip, sizeof(shellCode), MEM_DECOMMIT);
	CloseHandle(hThread);
	CloseHandle(hProcess);

	return true;
}


#else
bool ripInjectForX64(std::string windowName, std::string dllPath)
{
	unsigned char shellCode[] =
	{
		0x51, 0x50, 0x48, 0xB9, 0x00, 0x00 , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 , 0x00 , 0x00,
		0x48, 0x83, 0xEC, 0x28, 0xFF, 0xD0,
		0x48, 0x83, 0xC4, 0x28, 0x58, 0x59, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	// 1.��ȡҪע����̵Ĵ��ھ��
	HWND hwnd = FindWindow(NULL, windowName.c_str());
	if (hwnd == NULL)
		return false;

	// 2. ��ȡ����ID���߳�ID
	// �˴���ȡ�����̵߳��߳�ID���������û�д����̣߳����Ա��������߳��б���һ���������е��߳�������
	DWORD pid = 0;
	DWORD threadID = GetWindowThreadProcessId(hwnd, &pid);

	// 3. ��ȡ�߳̾���ͽ��̾��
	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadID);
	if (hThread == NULL)
		return false;

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL)
		return false;

	// 4. �����߳�
	SuspendThread(hThread);

	// 5. ��ȡLoadLibraryA������ַ
	HMODULE hKernel32Module = GetModuleHandle("kernel32.dll");
	if (hKernel32Module == NULL)
		return false;

	FARPROC hFarProc = GetProcAddress(hKernel32Module, "LoadLibraryA");

	// 6. �ڽ����з����ڴ�
	LPVOID lpDllAddr = VirtualAllocEx(hProcess, NULL, dllPath.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (lpDllAddr == NULL)
		return false;

	LPVOID lpNewRip = VirtualAllocEx(hProcess, NULL, sizeof(shellCode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (lpNewRip == NULL)
		return false;

	// 7. ��ȡָ���̵߳������Ļ���CONTEXT
	CONTEXT cont = { 0 };
	cont.ContextFlags = CONTEXT_CONTROL;
	bool res = GetThreadContext(hThread, &cont);

	// 9. �޸�shellCode���ݣ�����Ҫִ�еĻ�����
	DWORD64 oldRip = cont.Rip; // ������RIP��ַ��֮����Ҫ��ת��������ִ��
	memcpy(shellCode + 4, &lpDllAddr, sizeof(lpDllAddr));
	memcpy(shellCode + 14, &hFarProc, sizeof(hFarProc));
	memcpy(shellCode + sizeof(shellCode) - 8, &oldRip, sizeof(oldRip));

	// 10. ��ָ���ڴ�д�����ݣ�dll·����Ҫִ�еĻ�����
	WriteProcessMemory(hProcess, lpDllAddr, dllPath.c_str(), dllPath.size(), 0);
	WriteProcessMemory(hProcess, lpNewRip, shellCode, sizeof(shellCode), 0);

	// 11. ��RIP�޸ĳ��·����ڴ�ĵ�ַ��ͬʱ�����߳������Ļ����ٰ��̷߳��£���������ִ�С�
	cont.Rip = (DWORD64)lpNewRip;
	SetThreadContext(hThread, &cont);
	ResumeThread(hThread);

	// 12. �ͷ��ڴ棬�رվ��
	Sleep(10 * 1000); // �����ͷ��ڴ���ܻᵼ�½��̱�����Ҫô��һ�����ͷţ�Ҫô���ͷ�
	VirtualFreeEx(hProcess, lpDllAddr, dllPath.size(), MEM_DECOMMIT);
	VirtualFreeEx(hProcess, lpNewRip, sizeof(shellCode), MEM_DECOMMIT);
	CloseHandle(hThread);
	CloseHandle(hProcess);

	return true;
}
#endif // !X64

int main()
{
#ifndef X64
	eipInjectForX86(WINDOW_NAME, INJECTDLL_PATH);
#else
	ripInjectForX64(WINDOW_NAME, INJECTDLL_PATH);
#endif // !X64

	system("pause");
	return 0;
}