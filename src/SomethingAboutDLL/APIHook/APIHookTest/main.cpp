// mainExe.cpp : 定义控制台应用程序的入口点。
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
	// 注入前
	implicitLinkDLLFun();
	// 等待注入
	Sleep(10 * 1000);
	// 注入后
	implicitLinkDLLFun();

	// 问题4
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
			std::cout << "DLL加载失败" << std::endl;
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