// main.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include "../common/APIHook.h"
#include "../common/APIProvider.h"

#ifdef _DEBUG
#ifndef X64 
#define DYNAMIC_DLL "../Debug/dynamicLinkDLL.dll"
#else
#define DYNAMIC_DLL "../x64/Debug/dynamicLinkDLL.dll"
#endif
#else
#ifndef X64 
#define DYNAMIC_DLL  "../Release/dynamicLinkDLL.dll"
#else
#define DYNAMIC_DLL "../x64/Release/dynamicLinkDLL.dll"
#endif
#endif // _DEBUG

typedef void(*myFun)();

void apiHook()
{
	// �µĺ�����ַ
	PROC pNew = (PROC)CAPIProvider::newDynamicLinkDLLFun;
	// ��ȡ��ǰ�����е�dynamicLinkDLL.dllģ����
	HMODULE hDll = GetModuleHandle(L"dynamicLinkDLL.dll");
	// ����dynamicLinkDLLFun����
	CAPIHook::ReplaceEATEntryInOneMod(hDll, "dynamicLinkDLLFun", pNew);
}

int main()
{
	HMODULE hDll = NULL;
	do
	{
		hDll = LoadLibraryA(DYNAMIC_DLL);
		if (hDll == NULL)
			break;

		myFun pFun = (myFun)GetProcAddress(hDll, "dynamicLinkDLLFun");
		// ��������ǰ
		pFun();
		std::cout << "---------------" << std::endl;
		// ��������
		apiHook();
		// �������غ�--����1
		pFun();
		std::cout << "---------------" << std::endl;
		// ���»�ȡ������ַ
		pFun = (myFun)GetProcAddress(hDll, "dynamicLinkDLLFun");
		pFun();
	} while (0);

	if (hDll == NULL)
	{
		FreeLibrary(hDll);
		hDll = nullptr;
	}
	
	system("pause");
    return 0;
}

