// dllmain.cpp : ���� DLL Ӧ�ó������ڵ㡣
#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		MessageBox(NULL,L"DLL_PROCESS_ATTACH", L"DllMain", MB_OK);
		break;
	}
	case DLL_THREAD_ATTACH:
	{
		//MessageBox(NULL, L"DLL_THREAD_ATTACH", L"DllMain", MB_OK);
		break;
	}
	case DLL_THREAD_DETACH:
	{
		//MessageBox(NULL, L"DLL_THREAD_DETACH", L"DllMain", MB_OK);
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		MessageBox(NULL, L"DLL_PROCESS_DETACH", L"DllMain", MB_OK);
		break;
	}
	}
	return TRUE;
}

