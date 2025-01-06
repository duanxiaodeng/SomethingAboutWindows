#include "APIHook.h"
#include "ToolHelp.h"
#include <strsafe.h>
#include <ImageHlp.h>
#include "APIProvider.h"
#include <iostream>
#pragma comment(lib, "ImageHlp")

// CAPIHook中保存函数对象的链表头部
CAPIHook* CAPIHook::sm_pHead = NULL;

// 默认调用CAPHook()的模块不被拦截
BOOL CAPIHook::ExcludeAPIHookMod = TRUE;

CAPIHook::CAPIHook(PSTR pszCalleeModName, PSTR pszFuncName, PROC pfnHook)
{
	// 注意：只有要拦截的模块被加载时，函数才能被拦截。一个解决方案是，把函数名称保存起来，
	//       然后在LoadLibrary*函数的拦截处理器中分析CAPIHook各实例
	//       检查pszCalleeModName模块是否是要被拦截的模块

	DebugLog("要拦截的模块：%s，要拦截的函数：%s", pszCalleeModName, pszFuncName);

	m_pNext = sm_pHead;		//下一个节点
	sm_pHead = this;		//因为每拦截一个函数都要创建一个CAPIHook的实例

	// 将被拦截函数信息保存起来
	m_pszCalleeModName = pszCalleeModName;
	m_pszFuncName = pszFuncName;
	m_pfnHook = pfnHook;
	HMODULE hmod = GetModuleHandleA(pszCalleeModName);
	m_pfnOrig = GetProcAddressRaw(hmod, m_pszFuncName);
	DebugLog("原函数地址：%x，新函数地址：%x", m_pfnOrig, pfnHook);

	// 如果函数不存在，则退出，这种情况发生在模块未被加载进来
	if (m_pfnOrig == NULL)
	{
		wchar_t szPathname[MAX_PATH];
		GetModuleFileNameW(NULL, szPathname, _countof(szPathname));
		wchar_t sz[1024];
		StringCchPrintfW(sz, _countof(sz),
			TEXT("[%4u - %s] 找不到 %S\r\n"),
			GetCurrentProcessId(), szPathname, pszFuncName);
		OutputDebugString(sz);
		return;
	}

	// 拦截当前被载入的所有模块中指定的函数
	ReplaceIATEntryInAllMods(m_pszCalleeModName, m_pfnOrig, m_pfnHook);
}

CAPIHook::~CAPIHook()
{
	// 取消拦截
	// 调用ReplaceIATEntryInAllMods将每个模块的导入表中的地址重置为原来的地址
	ReplaceIATEntryInAllMods(m_pszCalleeModName, m_pfnHook, m_pfnOrig);

	// 删除链表
	CAPIHook* p = sm_pHead;
	if (p == this)  // 删除头结点
	{
		sm_pHead = p->m_pNext;
	}
	else
	{
		BOOL bFound = FALSE;

		//遍历链表,并删除当前结点
		for (; !bFound && (p->m_pNext != NULL); p = p->m_pNext)
		{
			if (p->m_pNext == this)
			{
				p->m_pNext = p->m_pNext->m_pNext;
				bFound = TRUE;
			}
		}
	}
}

// 返回包含指定内存地址的模块句柄
static HMODULE ModuleFromAddress(PVOID pv)
{
	MEMORY_BASIC_INFORMATION mbi;
	return ((VirtualQuery(pv, &mbi, sizeof(mbi)) != 0) ? (HMODULE)mbi.AllocationBase : NULL);
}

// 异常过滤函数
LONG WINAPI InvalidReadExceptionFilter(PEXCEPTION_POINTERS pep)
{
	// 处理所有非预期的异常，在这种情况下不处理任何模块
	// 注意：pep->ExceptionRecord->ExceptionCode的值可能为0xC0000005
	LONG lDisposition = EXCEPTION_EXECUTE_HANDLER;
	// 返回EXCEPTION_EXECUTE_HANDLER表示可以进入__except模块处理了
	return lDisposition;
}

void CAPIHook::DebugLog(const char *fmt, ...)
{
#define LOG_BUF_SIZE 1024
	char buf[LOG_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(buf, LOG_BUF_SIZE - 1, fmt, ap);
	va_end(ap);

	char outputBuf[LOG_BUF_SIZE] = { 0 };
	_snprintf_s(outputBuf, LOG_BUF_SIZE - 1, "---APIHook---:%-5d %-5d %s\n", GetCurrentProcessId(), GetCurrentThreadId(), buf);
	OutputDebugStringA(outputBuf);
}

void CAPIHook::ReplaceIATEntryInOneMod(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew, HMODULE hmodCaller)
{
	ULONG ulSize;
	//当传入ImageDirectorEntryToData函数的是一个一个无效的模块句柄时，会产生一个0xC0000005的异常。
	// 如：进程的另一个线程快速动态载入和卸装DLL，这时可能导致该函数异常。
	// 如：hmodCaller是无效句柄，会触发0xC0000005的异常
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = NULL;
	__try {
		// 获取指定模块hmodCaller导入地址表(IAT)的首个导入模块地址
		pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
			hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
	}
	__except (InvalidReadExceptionFilter(GetExceptionInformation())) {
		//这里什么也不做，但当hmodCaller为无效句柄时，此时可以保证线程仍能正常运行，但pImportDesc为NULL。
	}

	if (pImportDesc == NULL)
		return; // 模块没有导入表或还未被加载。

	// 遍历导入表。
	// 结束条件：当前该IMAGE_IMPORT_DESCRIPTOR结构(Name字段)指针为NULL
	for (; pImportDesc->Name; pImportDesc++)
	{
		// 获取导入表中DLL的名称
		// 模块导入表的所有字符串都是以ANSI格式保存的，绝对不会是UNICODE
		PSTR pszModName = (PSTR)((PBYTE)hmodCaller + pImportDesc->Name);
		if (lstrcmpiA(pszModName, pszCalleeModName) == 0) // 如果是指定要拦截的函数所在的DLL
		{
			// 获得调用者导入表中要被拦截函数的地址
			PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hmodCaller + pImportDesc->FirstThunk);
			// 遍历IAT表
			for (; pThunk->u1.Function; pThunk++)
			{
				// 获得函数地址的地址
				PROC* ppfn = (PROC*)&pThunk->u1.Function;
				// 判断此函数是不是我们要找的函数
				BOOL bFound = (*ppfn == pfnCurrent);
				if (bFound)
				{
					DebugLog("找到要拦截的模块和函数，开始拦截...");
					// 用新的函数地址替换当前的函数地址
					if (!WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL)
						&& (ERROR_NOACCESS == GetLastError()))
					{
						// 如果权限不够则更改内存页保护属性
						DWORD dwOldProtect;
						if (VirtualProtect(ppfn, sizeof(pfnNew), PAGE_EXECUTE_WRITECOPY, &dwOldProtect))
						{
							WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL);
							VirtualProtect(ppfn, sizeof(pfnNew), dwOldProtect, &dwOldProtect);
						}
					}
					return;
				}
			}
		}
	}
}

void CAPIHook::ReplaceEATEntryInOneMod(HMODULE hmod, PCSTR pszFunctionName, PROC pfnNew)
{
	ULONG ulSize;

	PIMAGE_EXPORT_DIRECTORY pExportDir = NULL;
	__try
	{
		// 获取指定模块hmod导出表(EAT)的地址
		pExportDir = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryEntryToData(
			hmod, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &ulSize);
	}
	__except (InvalidReadExceptionFilter(GetExceptionInformation()))
	{
		// 这里什么也不错，但当hmod为无效句柄时，此时可以保证线程仍能
		// 正常运行，但pExportDir为NULL。
	}

	if (pExportDir == NULL)
		return; // 模块没有导出表或还未被加载。

	// 获取导出表的数据
	PDWORD pdwNamesRvas = (PDWORD)((PBYTE)hmod + pExportDir->AddressOfNames);
	PWORD pwNameOrdinals = (PWORD)((PBYTE)hmod + pExportDir->AddressOfNameOrdinals);
	PDWORD pdwFunctionAddresses = (PDWORD)((PBYTE)hmod + pExportDir->AddressOfFunctions);

	// 遍历导出表的导出函数的名称, 并进行匹配
	for (DWORD n = 0; n < pExportDir->NumberOfNames; n++)
	{
		// 模块导出表的所有字符串都是以ANSI格式保存的
		PSTR pszFuncName = (PSTR)((PBYTE)hmod + pdwNamesRvas[n]);
		if (lstrcmpiA(pszFuncName, pszFuncName) != 0)
			continue;
		// 找到指定的函数
		WORD ordinal = pwNameOrdinals[n];
		// 获取函数的地址
		PROC* ppfn = (PROC*)&pdwFunctionAddresses[ordinal];
		// 将新的地址写入RVA
		pfnNew = (PROC)((PBYTE)pfnNew - (PBYTE)hmod);
		// 将被拦截函数替换为替换函数
		if (!WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL) && (ERROR_NOACCESS == GetLastError()))
		{
			// 如果失败则更改内存页保护属性
			DWORD dwOldProtect;
			if (VirtualProtect(ppfn, sizeof(pfnNew), PAGE_EXECUTE_WRITECOPY, &dwOldProtect))
			{
				WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL);
				VirtualProtect(ppfn, sizeof(pfnNew), dwOldProtect, &dwOldProtect);
			}
		}
		break;
	}
}

void CAPIHook::ReplaceIATEntryInAllMods(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew)
{
	// 是否连该DLL本身的模块也替换，默认是被排除的，即：本DLL不替换。
	HMODULE hmodThisMod = ExcludeAPIHookMod ? ModuleFromAddress(ReplaceIATEntryInAllMods) : NULL;
	// 获取进程模块列表
	CToolHelp th(TH32CS_SNAPMODULE, GetCurrentProcessId());

	MODULEENTRY32 me = { sizeof(me) };
	for (BOOL bOk = th.ModuleFirst(&me); bOk; bOk = th.ModuleNext(&me))
	{
		//注意：不替换自己模块中的函数
		if (me.hModule != hmodThisMod)
		{
			ReplaceIATEntryInOneMod(pszCalleeModName, pfnCurrent, pfnNew, me.hModule);
		}
	}
}

void WINAPI CAPIHook::FixupNewlyLoadedModuled(HMODULE hmod, DWORD dwFlags)
{
	DebugLog("有新的模块被加载");
	// 如果一个模块最近被加载，拦截相应的API
	if ((hmod != NULL) &&
		(hmod != ModuleFromAddress(FixupNewlyLoadedModuled)) && // 不要拦截自己的模块
		((dwFlags & LOAD_LIBRARY_AS_DATAFILE) == 0) &&
		((dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) == 0) &&
		((dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) == 0)
		)
	{
		for (CAPIHook* p = sm_pHead; p != NULL; p = p->m_pNext)
		{
			if (p->m_pfnOrig != NULL)
			{
				// 本来只需对这个被加载的DLL修复那些被拦截的函数地址即可，
				// 但新载入的DLL，如果静态链接（注意，不是动态）了其他DLL，这另外的这个DLL也调
				// 用了要被拦截的函数时，就产生了链接依赖。所以可以简单地调用替换所有模块的这个函数
				ReplaceIATEntryInAllMods(p->m_pszCalleeModName, p->m_pfnOrig, p->m_pfnHook);
			}
			else
			{
#ifdef _DEBUG
				wchar_t szPathname[MAX_PATH];
				GetModuleFileNameW(NULL, szPathname, _countof(szPathname));
				wchar_t sz[1024];
				StringCchPrintfW(sz, _countof(sz),
					TEXT("[%4u - %s]找不到 %S\r\n"),
					GetCurrentProcessId(), szPathname, p->m_pszCalleeModName);
				OutputDebugString(sz);
#endif
			}
		}
	}
}

HMODULE WINAPI CAPIHook::LoadLibraryA(PCSTR pszModulePath)
{
	DebugLog("调用被拦截的LoadLibraryA加载：%s", pszModulePath);
	HMODULE hmod = ::LoadLibraryA(pszModulePath);
	FixupNewlyLoadedModuled(hmod, 0);

	// 以下这部分为动态加载指定拦截示例
	// 当通过LoadLibraryA加载dynamicLinkDLL.dll时，将其导出的dynamicLinkDLLFun函数替换成我们想要的函数
	std::string dllPath = std::string(pszModulePath);
	if (dllPath.find("dynamicLinkDLL.dll") != std::string::npos)
	{
		ReplaceEATEntryInOneMod(hmod, "dynamicLinkDLLFun", (PROC)CAPIProvider::newDynamicLinkDLLFun);
	}

	return (hmod);
}

HMODULE WINAPI CAPIHook::LoadLibraryW(PCWSTR pszModulePath)
{
	DebugLog("调用被拦截的LoadLibraryW加载：%s", pszModulePath);
	HMODULE hmod = ::LoadLibraryW(pszModulePath);
	FixupNewlyLoadedModuled(hmod, 0);
	return hmod;
}
HMODULE WINAPI CAPIHook::LoadLibraryExA(PCSTR pszModulePath, HANDLE hFile, DWORD dwFlags)
{
	DebugLog("被拦截的LoadLibraryExA");
	HMODULE hmod = ::LoadLibraryExA(pszModulePath, hFile, dwFlags);
	FixupNewlyLoadedModuled(hmod, dwFlags);
	return hmod;
}
HMODULE WINAPI CAPIHook::LoadLibraryExW(PCWSTR pszModulePath, HANDLE hFile, DWORD dwFlags)
{
	DebugLog("被拦截的LoadLibraryExW");
	HMODULE hmod = ::LoadLibraryExW(pszModulePath, hFile, dwFlags);
	FixupNewlyLoadedModuled(hmod, dwFlags);
	return hmod;
}

// 获得GetProcAddess函数的真实地址
FARPROC CAPIHook::GetProcAddressRaw(HMODULE hmod, PCSTR pszProcName)
{
	DebugLog("调用原先的GetProcAddress获取%s的函数地址", pszProcName);
	return (::GetProcAddress(hmod, pszProcName)); // 调用全局的API
}

// 拦截通过对GetProcAddress调用而直接获得要被拦截函数地址的
// 获取被Hook函数被保存起来的真实地址
FARPROC WINAPI CAPIHook::GetProcAddress(HMODULE hmod, PCSTR pszProcName)
{
	DebugLog("调用被拦截的GetProcAddress获取%s的函数地址", pszProcName);
	// 获取函数的真实地址
	FARPROC pfn = GetProcAddressRaw(hmod, pszProcName);
	DebugLog("原先的函数地址：%x", pfn);
	// 判断函数是否己被hook过
	for (CAPIHook* p = sm_pHead; (pfn != NULL) && (p != NULL); p = p->m_pNext)
	{
		if (pfn == p->m_pfnOrig) // 函数地址相等，说明已经拦截过
		{
			pfn = p->m_pfnHook;
			DebugLog("此函数被拦截过，函数地址：%x", pfn);
			break;
		}
	}

	return pfn;
}

//拦截LoadLibrary*和GetProcAddress函数
CAPIHook CAPIHook::sm_LoadLibraryA("Kernel32.dll", "LoadLibraryA", (PROC)CAPIHook::LoadLibraryA);
CAPIHook CAPIHook::sm_LoadLibrarayW("Kernel32.dll", "LoadLibraryW", (PROC)CAPIHook::LoadLibraryW);
CAPIHook CAPIHook::sm_LoadLibraryExA("Kernel32.dll", "LoadLibraryExA", (PROC)CAPIHook::LoadLibraryExA);
CAPIHook CAPIHook::sm_LoadLibrarayExW("Kernel32.dll", "LoadLibraryExW", (PROC)CAPIHook::LoadLibraryExW);
CAPIHook CAPIHook::sm_GetProcAddress("Kernel32.dll", "GetProcAddress", (PROC)CAPIHook::GetProcAddress);