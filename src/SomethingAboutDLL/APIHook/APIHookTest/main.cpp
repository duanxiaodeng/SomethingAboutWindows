// mainExe.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include "../implicitLinkDLL/implicitLinkDLL.h"

#ifdef _DEBUG
#ifndef X64 
#pragma comment(lib, "../Debug/implicitLinkDLL.lib")
#define DYNAMIC_DLL "../Debug/dynamicLinkDLL.dll"
#else
#pragma comment(lib, "../x64/Debug/implicitLinkDLL.lib")
#define DYNAMIC_DLL "../x64/Debug/dynamicLinkDLL.dll"
#endif
#else
#ifndef X64 
#pragma comment(lib, "../Release/implicitLinkDLL.lib")
#define DYNAMIC_DLL  "../Release/dynamicLinkDLL.dll"
#else
#pragma comment(lib, "../x64/Release/implicitLinkDLL.lib")
#define DYNAMIC_DLL "../x64/Release/dynamicLinkDLL.dll"
#endif
#endif // _DEBUG

typedef void(*myFun)();

void implicitLink()
{
	// ע��ǰ
	implicitLinkDLLFun();
	// �ȴ�ע��
	Sleep(10 * 1000);
	// ע���
	implicitLinkDLLFun();

	// ����4
	myFun pFun = (myFun)GetProcAddress(GetModuleHandle(L"implicitLinkDLL.dll"), "implicitLinkDLLFun");
	pFun();
}

void dynamicLink()
{
	HMODULE hDll = NULL;
	do
	{
		hDll = LoadLibraryA(DYNAMIC_DLL);
		if (hDll == NULL)
		{
			std::cout << "DLL����ʧ��" << std::endl;
			break;
		}

		myFun pFun = (myFun)GetProcAddress(hDll, "dynamicLinkDLLFun");
		pFun();
	} while (0);
}

int main()
{
	implicitLink();
	dynamicLink();

	system("pause");
	return 0;
}