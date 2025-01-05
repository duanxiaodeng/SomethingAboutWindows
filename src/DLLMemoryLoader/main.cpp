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
	// 1. ��DLL�ļ�����ȡDLL�ļ���С
	HANDLE hFile = CreateFile(TEST_DLL, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_ARCHIVE, NULL); // dll������ڣ����򱨴�
	if (hFile == INVALID_HANDLE_VALUE)
	{
		ShowError("CreateFile");
		return 1;
	}
	DWORD dwFileSize = GetFileSize(hFile, NULL);

	// 2. ���붯̬�ڴ沢��ȡDLL���ڴ���
	BYTE* lpData = new BYTE[dwFileSize];
	if (NULL == lpData)
	{
		ShowError("new");
		return 2;
	}
	DWORD dwRet = 0;
	ReadFile(hFile, lpData, dwFileSize, &dwRet, NULL);

	// 3. ���ڴ�DLL���ص�������
	LPVOID lpBaseAddress = MmLoadLibrary(lpData, dwFileSize);
	if (lpBaseAddress == NULL)
	{
		ShowError("MmLoadLibrary");
		return 3;
	}
	printf("DLL���سɹ�\n");

	// 4. ��ȡDLL��������������
	typedef int(*AddFun)(int, int);
	AddFun pfn = (AddFun)MmGetProcAddress(lpBaseAddress, "add");
	if (pfn == NULL)
	{
		ShowError("MmGetProcAddress");
		return 4;
	}
	std::cout << pfn(10, 20) << std::endl;

	// 5. �ͷŴ��ڴ���ص�DLL
	BOOL bRet = MmFreeLibrary(lpBaseAddress);
	if (bRet == FALSE)
	{
		ShowError("MmFreeLirbary");
	}
	printf("DLLж�سɹ�\n");

	// 6. �ͷ���Դ
	delete[] lpData;
	lpData = NULL;
	CloseHandle(hFile);

	system("pause");
	return 0;
}