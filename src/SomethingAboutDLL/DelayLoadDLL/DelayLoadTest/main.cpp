// DelayLoadTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <delayimp.h>
#include <iostream>
#include "../DelayLoadDLL/DelayLoadDLL.h"

#define DELAYLOADDLL "DelayLoadDLL.dll"

#ifdef _DEBUG
#ifndef X64 
#pragma comment(lib, "../Debug/DelayLoadDLL.lib")
#else
#pragma comment(lib, "../x64/Debug/DelayLoadDLL.lib")
#endif
#else
#ifndef X64 
#pragma comment(lib, "../Release/DelayLoadDLL.lib")
#else
#pragma comment(lib, "../x64/Release/DelayLoadDLL.lib")
#endif
#endif // _DEBUG

int main()
{
	// 第一阶段：先检测DLL是否加载
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;
	std::cout << "-----------------" << std::endl;

	Sleep(5 * 1000);

	// 第二阶段：调用DLL的函数，会自动加载DLL
	delayFun1(10);
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;
	std::cout << "-----------------" << std::endl;

	Sleep(5 * 1000);

	// 第三阶段：卸载延迟加载的DLL
	__FUnloadDelayLoadedDLL2(DELAYLOADDLL);
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;

	system("pause");
	return 0;
}