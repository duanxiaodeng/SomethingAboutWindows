#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <iostream>
#include "MemoryLoader.h"

#ifdef _DEBUG
#ifndef X64 
#define TEST_DLL "../Debug/TestDLL.dll"
#else
#define TEST_DLL "../x64/Debug/TestDLL.dll"
#endif
#else
#ifndef X64 
#define TEST_DLL  "../Release/TestDLL.dll"
#else
#define TEST_DLL "../x64/Release/TestDLL.dll"
#endif
#endif // _DEBUG

int _tmain(int argc, _TCHAR* argv[])
{
	// 1. 打开DLL文件并获取DLL文件大小
	HANDLE hFile = CreateFile(TEST_DLL, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_ARCHIVE, NULL); // dll必须存在，否则报错
	if (hFile == INVALID_HANDLE_VALUE)
	{
		ShowError("CreateFile");
		return 1;
	}
	DWORD dwFileSize = GetFileSize(hFile, NULL);

	// 2. 申请动态内存并读取DLL到内存中
	BYTE* lpData = new BYTE[dwFileSize];
	if (NULL == lpData)
	{
		ShowError("new");
		return 2;
	}
	DWORD dwRet = 0;
	ReadFile(hFile, lpData, dwFileSize, &dwRet, NULL);

	// 3. 将内存DLL加载到程序中
	LPVOID lpBaseAddress = MmLoadLibrary(lpData, dwFileSize);
	if (lpBaseAddress == NULL)
	{
		ShowError("MmLoadLibrary");
		return 3;
	}
	printf("DLL加载成功\n");

	// 4. 获取DLL导出函数并调用
	typedef int(*AddFun)(int, int);
	AddFun pfn = (AddFun)MmGetProcAddress(lpBaseAddress, "add");
	if (pfn == NULL)
	{
		ShowError("MmGetProcAddress");
		return 4;
	}
	std::cout << pfn(10, 20) << std::endl;

	// 5. 释放从内存加载的DLL
	BOOL bRet = MmFreeLibrary(lpBaseAddress);
	if (bRet == FALSE)
	{
		ShowError("MmFreeLirbary");
	}
	printf("DLL卸载成功\n");

	// 6. 释放资源
	delete[] lpData;
	lpData = NULL;
	CloseHandle(hFile);

	system("pause");
	return 0;
}