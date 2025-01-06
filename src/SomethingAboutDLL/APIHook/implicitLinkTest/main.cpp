// main.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include "../common/APIHook.h"
#include "../common/APIProvider.h"
#include "../implicitLinkDLL/implicitLinkDLL.h"

#ifdef _DEBUG
#ifndef X64 
#pragma comment(lib, "../Debug/implicitLinkDLL.lib")
#else
#pragma comment(lib, "../x64/Debug/implicitLinkDLL.lib")
#endif
#else
#ifndef X64 
#pragma comment(lib, "../Release/implicitLinkDLL.lib")
#else
#pragma comment(lib, "../x64/Release/implicitLinkDLL.lib")
#endif
#endif // _DEBUG

void apiHook()
{
	// 原先的函数地址
	PROC pOrig = GetProcAddress(GetModuleHandle(L"implicitLinkDLL.dll"), "implicitLinkDLLFun");
	// 新的函数地址
	PROC pNew = (PROC)CAPIProvider::newImplicitLinkDLLFun;
	// 获取当前进程的模块句柄，因为是静态链接，所以调用implicitLinkDLL.dll是当前进行
	HMODULE hmodCaller = GetModuleHandle(NULL);
	// 拦截函数
	CAPIHook::ReplaceIATEntryInOneMod("implicitLinkDLL.dll", pOrig, pNew, hmodCaller);
}

int main(void)
{
	// 函数拦截前
	implicitLinkDLLFun();
	// 进行函数拦截
	apiHook();
	std::cout << "------------------------------" << std::endl;
	// 函数拦截后
	implicitLinkDLLFun();
	std::cout << "------------------------------" << std::endl;

	// 问题4
	typedef void(*myFun)();
	myFun pFun = (myFun)GetProcAddress(GetModuleHandle(L"implicitLinkDLL.dll"), "implicitLinkDLLFun");
	pFun();

	system("pause");
	return 0;
}