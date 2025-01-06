#pragma once
#include <windows.h>

class CAPIHook
{
public:
	// 拦截将被注入的进程所有模块中指定的API
	CAPIHook(PSTR pszCalleeModName, // 被拦截函数所在的模块（DLL）
		PSTR pszFuncName,		// 要被拦截函数的名称
		PROC pfnHook);			// 拦截函数（也叫替代函数）地址

	// 从被注入的进程所有模块中卸载一个函数（即:拦截函数）
	~CAPIHook();

	// 返回最初的、被拦截函数的地址
	// 如果我们希望在调用拦截函数以后再调用原先的函数，则需要使用它
	operator PROC() { return (m_pfnOrig); }

	// 是否连当前调用CAPIHook的模块(即该DLL本身）也要拦截？
	// 这个变量在ReplaceIATEntryInAllMods中会使用到，这里被声明为static
	static BOOL ExcludeAPIHookMod;

public:
	// 调用真正的GetProcAddress
	static FARPROC WINAPI GetProcAddressRaw(HMODULE hmod, PCSTR pszProcName);
	static void DebugLog(const char *fmt, ...);

public:
	// 在一个模块的导入表中替换指定函数的地址，
	// 即：hmodCaller模块导入了pszCalleeModeName模块里的pfnOrg函数，
	// 现在要将hmodCaller导入表中的pfnOrg函数替换为pfnHook函数。
	static void WINAPI ReplaceIATEntryInOneMod(
		PCSTR pszCalleeModName,		// 被调用模块的名称(DLL)
		PROC pfnOrig,				// 被拦截函数的地址，pszCalleeModeName模块中
		PROC pfnHook,				// 拦截函数（即：替换函数）的地址
		HMODULE hmodCaller);		// 调用模块，即：要修改导入段的模块

	// 替换当前进程中，所有模块的导入表中指定函数地址为被拦截函数的地址
	static void WINAPI ReplaceIATEntryInAllMods(PCSTR pszCalleeModName, PROC pfnOrig, PROC pfnHook);

	// 在一个模块的导出表中替换指定函数的地址
	static void WINAPI ReplaceEATEntryInOneMod(HMODULE hmod, PCSTR pszFunctionName, PROC pfnNew);

private:
	// 当DLL是在被拦截以后加载进来时，使用下面这个函数
	static void WINAPI FixupNewlyLoadedModuled(HMODULE hmod, DWORD dwFlags);

	// 当DLL是运行时动态被加载进行时使用
	static HMODULE WINAPI LoadLibraryA(PCSTR pszModulePath);
	static HMODULE WINAPI LoadLibraryW(PCWSTR pszModulePath);
	static HMODULE WINAPI LoadLibraryExA(PCSTR pszModulePath, HANDLE hFile, DWORD dwFlags);
	static HMODULE WINAPI LoadLibraryExW(PCWSTR pszModulePath, HANDLE hFile, DWORD dwFlags);
	// 如果函数已经被拦截过，则返回被替换函数的真实地址（保存在链表中）
	static FARPROC WINAPI GetProcAddress(HMODULE hmod, PCSTR pszProcName);

private:
	static CAPIHook* sm_pHead;	// 链表中，第一个被拦截函数
	CAPIHook* m_pNext;			// 下一个被拦截函数

	PCSTR m_pszCalleeModName;	// 含被拦截函数的模块名称（ANSI）
	PCSTR m_pszFuncName;		// 被拦截的函数名称（ANSI）
	PROC m_pfnOrig;				// 在被调用者中原始的函数,也就是被拦截的函数
	PROC m_pfnHook;				// 拦截函数，用来替代m_pfnOrig

private:
	// 实例化下面这些函数
	static CAPIHook  sm_LoadLibraryA;
	static CAPIHook  sm_LoadLibrarayW;
	static CAPIHook  sm_LoadLibraryExA;
	static CAPIHook  sm_LoadLibrarayExW;
	static CAPIHook  sm_GetProcAddress;
};