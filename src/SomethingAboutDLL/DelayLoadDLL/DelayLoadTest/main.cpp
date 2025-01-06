// DelayLoadTest.cpp : �������̨Ӧ�ó������ڵ㡣
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
	// ��һ�׶Σ��ȼ��DLL�Ƿ����
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL������" << std::endl :
		std::cout << "DLLδ������" << std::endl;
	std::cout << "-----------------" << std::endl;

	Sleep(5 * 1000);

	// �ڶ��׶Σ�����DLL�ĺ��������Զ�����DLL
	delayFun1(10);
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL������" << std::endl :
		std::cout << "DLLδ������" << std::endl;
	std::cout << "-----------------" << std::endl;

	Sleep(5 * 1000);

	// �����׶Σ�ж���ӳټ��ص�DLL
	__FUnloadDelayLoadedDLL2(DELAYLOADDLL);
	GetModuleHandleA(DELAYLOADDLL) != NULL ?
		std::cout << "DLL������" << std::endl :
		std::cout << "DLLδ������" << std::endl;

	system("pause");
	return 0;
}