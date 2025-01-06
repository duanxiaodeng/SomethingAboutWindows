// DelayLoadTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <delayimp.h>
#include <iostream>
#include "../noDelayLoadDLL/noDelayLoadDLL.h"

#define DELAYLOADDLL "noDelayLoadDLL.dll"

#ifdef _DEBUG
#ifndef X64 
#pragma comment(lib, "../Debug/noDelayLoadDLL.lib")
#else
#pragma comment(lib, "../x64/Debug/noDelayLoadDLL.lib")
#endif
#else
#ifndef X64 
#pragma comment(lib, "../Release/noDelayLoadDLL.lib")
#else
#pragma comment(lib, "../x64/Release/noDelayLoadDLL.lib")
#endif
#endif // _DEBUG

int main()
{
	// 加上以下这行代码会编译失败
	//int num = nnoDelayLoadDLL;

	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;
	std::cout << "-----------------" << std::endl;

	noDelayLoadDLL();
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;

	std::cout << "-----------------" << std::endl;
	__FUnloadDelayLoadedDLL2(DELAYLOADDLL);
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL被加载" << std::endl :
		std::cout << "DLL未被加载" << std::endl;

	system("pause");
	return 0;
}