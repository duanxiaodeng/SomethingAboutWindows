// main.cpp : �������̨Ӧ�ó������ڵ㡣
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
	// ԭ�ȵĺ�����ַ
	PROC pOrig = GetProcAddress(GetModuleHandle(L"implicitLinkDLL.dll"), "implicitLinkDLLFun");
	// �µĺ�����ַ
	PROC pNew = (PROC)CAPIProvider::newImplicitLinkDLLFun;
	// ��ȡ��ǰ���̵�ģ��������Ϊ�Ǿ�̬���ӣ����Ե���implicitLinkDLL.dll�ǵ�ǰ����
	HMODULE hmodCaller = GetModuleHandle(NULL);
	// ���غ���
	CAPIHook::ReplaceIATEntryInOneMod("implicitLinkDLL.dll", pOrig, pNew, hmodCaller);
}

int main(void)
{
	// ��������ǰ
	implicitLinkDLLFun();
	// ���к�������
	apiHook();
	std::cout << "------------------------------" << std::endl;
	// �������غ�
	implicitLinkDLLFun();
	std::cout << "------------------------------" << std::endl;

	// ����4
	typedef void(*myFun)();
	myFun pFun = (myFun)GetProcAddress(GetModuleHandle(L"implicitLinkDLL.dll"), "implicitLinkDLLFun");
	pFun();

	system("pause");
	return 0;
}