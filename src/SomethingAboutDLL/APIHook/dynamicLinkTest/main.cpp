// main.cpp : 定义控制台应用程序的入口点。
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
	// 新的函数地址
	PROC pNew = (PROC)CAPIProvider::newDynamicLinkDLLFun;
	// 获取当前进程中的dynamicLinkDLL.dll模块句柄
	HMODULE hDll = GetModuleHandle(L"dynamicLinkDLL.dll");
	// 拦截dynamicLinkDLLFun函数
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
		// 函数拦截前
		pFun();
		std::cout << "---------------" << std::endl;
		// 函数拦截
		apiHook();
		// 函数拦截后--问题1
		pFun();
		std::cout << "---------------" << std::endl;
		// 重新获取函数地址
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

