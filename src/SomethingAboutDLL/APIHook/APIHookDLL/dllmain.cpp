// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include <iostream>
#include "../common/APIHook.h"
#include "../common/APIProvider.h"

// 为了在注入DLL时实现拦截指定的API，定义一个这样的静态实例。
static CAPIHook implicitLinkHook("implicitLinkDLL.dll", "implicitLinkDLLFun", (PROC)CAPIProvider::newImplicitLinkDLLFun);

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		CAPIHook::DebugLog("DLL_PROCESS_ATTACH");
		break;
	}
	case DLL_THREAD_ATTACH:
	{
		CAPIHook::DebugLog("DLL_THREAD_ATTACH");
		break;
	}
	case DLL_THREAD_DETACH:
	{
		CAPIHook::DebugLog("DLL_THREAD_DETACH");
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		CAPIHook::DebugLog("DLL_PROCESS_DETACH");
		break;
	}
	}
	return TRUE;
}

