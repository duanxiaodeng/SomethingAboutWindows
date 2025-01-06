#include "APIHook.h"
#include "ToolHelp.h"
#include <strsafe.h>
#include <ImageHlp.h>
#include "APIProvider.h"
#include <iostream>
#pragma comment(lib, "ImageHlp")

// CAPIHook�б��溯�����������ͷ��
CAPIHook* CAPIHook::sm_pHead = NULL;

// Ĭ�ϵ���CAPHook()��ģ�鲻������
BOOL CAPIHook::ExcludeAPIHookMod = TRUE;

CAPIHook::CAPIHook(PSTR pszCalleeModName, PSTR pszFuncName, PROC pfnHook)
{
	// ע�⣺ֻ��Ҫ���ص�ģ�鱻����ʱ���������ܱ����ء�һ����������ǣ��Ѻ������Ʊ���������
	//       Ȼ����LoadLibrary*���������ش������з���CAPIHook��ʵ��
	//       ���pszCalleeModNameģ���Ƿ���Ҫ�����ص�ģ��

	DebugLog("Ҫ���ص�ģ�飺%s��Ҫ���صĺ�����%s", pszCalleeModName, pszFuncName);

	m_pNext = sm_pHead;		//��һ���ڵ�
	sm_pHead = this;		//��Ϊÿ����һ��������Ҫ����һ��CAPIHook��ʵ��

	// �������غ�����Ϣ��������
	m_pszCalleeModName = pszCalleeModName;
	m_pszFuncName = pszFuncName;
	m_pfnHook = pfnHook;
	HMODULE hmod = GetModuleHandleA(pszCalleeModName);
	m_pfnOrig = GetProcAddressRaw(hmod, m_pszFuncName);
	DebugLog("ԭ������ַ��%x���º�����ַ��%x", m_pfnOrig, pfnHook);

	// ������������ڣ����˳����������������ģ��δ�����ؽ���
	if (m_pfnOrig == NULL)
	{
		wchar_t szPathname[MAX_PATH];
		GetModuleFileNameW(NULL, szPathname, _countof(szPathname));
		wchar_t sz[1024];
		StringCchPrintfW(sz, _countof(sz),
			TEXT("[%4u - %s] �Ҳ��� %S\r\n"),
			GetCurrentProcessId(), szPathname, pszFuncName);
		OutputDebugString(sz);
		return;
	}

	// ���ص�ǰ�����������ģ����ָ���ĺ���
	ReplaceIATEntryInAllMods(m_pszCalleeModName, m_pfnOrig, m_pfnHook);
}

CAPIHook::~CAPIHook()
{
	// ȡ������
	// ����ReplaceIATEntryInAllMods��ÿ��ģ��ĵ�����еĵ�ַ����Ϊԭ���ĵ�ַ
	ReplaceIATEntryInAllMods(m_pszCalleeModName, m_pfnHook, m_pfnOrig);

	// ɾ������
	CAPIHook* p = sm_pHead;
	if (p == this)  // ɾ��ͷ���
	{
		sm_pHead = p->m_pNext;
	}
	else
	{
		BOOL bFound = FALSE;

		//��������,��ɾ����ǰ���
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

// ���ذ���ָ���ڴ��ַ��ģ����
static HMODULE ModuleFromAddress(PVOID pv)
{
	MEMORY_BASIC_INFORMATION mbi;
	return ((VirtualQuery(pv, &mbi, sizeof(mbi)) != 0) ? (HMODULE)mbi.AllocationBase : NULL);
}

// �쳣���˺���
LONG WINAPI InvalidReadExceptionFilter(PEXCEPTION_POINTERS pep)
{
	// �������з�Ԥ�ڵ��쳣������������²������κ�ģ��
	// ע�⣺pep->ExceptionRecord->ExceptionCode��ֵ����Ϊ0xC0000005
	LONG lDisposition = EXCEPTION_EXECUTE_HANDLER;
	// ����EXCEPTION_EXECUTE_HANDLER��ʾ���Խ���__exceptģ�鴦����
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
	//������ImageDirectorEntryToData��������һ��һ����Ч��ģ����ʱ�������һ��0xC0000005���쳣��
	// �磺���̵���һ���߳̿��ٶ�̬�����жװDLL����ʱ���ܵ��¸ú����쳣��
	// �磺hmodCaller����Ч������ᴥ��0xC0000005���쳣
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = NULL;
	__try {
		// ��ȡָ��ģ��hmodCaller�����ַ��(IAT)���׸�����ģ���ַ
		pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
			hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
	}
	__except (InvalidReadExceptionFilter(GetExceptionInformation())) {
		//����ʲôҲ����������hmodCallerΪ��Ч���ʱ����ʱ���Ա�֤�߳������������У���pImportDescΪNULL��
	}

	if (pImportDesc == NULL)
		return; // ģ��û�е�����δ�����ء�

	// ���������
	// ������������ǰ��IMAGE_IMPORT_DESCRIPTOR�ṹ(Name�ֶ�)ָ��ΪNULL
	for (; pImportDesc->Name; pImportDesc++)
	{
		// ��ȡ�������DLL������
		// ģ�鵼���������ַ���������ANSI��ʽ����ģ����Բ�����UNICODE
		PSTR pszModName = (PSTR)((PBYTE)hmodCaller + pImportDesc->Name);
		if (lstrcmpiA(pszModName, pszCalleeModName) == 0) // �����ָ��Ҫ���صĺ������ڵ�DLL
		{
			// ��õ����ߵ������Ҫ�����غ����ĵ�ַ
			PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hmodCaller + pImportDesc->FirstThunk);
			// ����IAT��
			for (; pThunk->u1.Function; pThunk++)
			{
				// ��ú�����ַ�ĵ�ַ
				PROC* ppfn = (PROC*)&pThunk->u1.Function;
				// �жϴ˺����ǲ�������Ҫ�ҵĺ���
				BOOL bFound = (*ppfn == pfnCurrent);
				if (bFound)
				{
					DebugLog("�ҵ�Ҫ���ص�ģ��ͺ�������ʼ����...");
					// ���µĺ�����ַ�滻��ǰ�ĺ�����ַ
					if (!WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL)
						&& (ERROR_NOACCESS == GetLastError()))
					{
						// ���Ȩ�޲���������ڴ�ҳ��������
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
		// ��ȡָ��ģ��hmod������(EAT)�ĵ�ַ
		pExportDir = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryEntryToData(
			hmod, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &ulSize);
	}
	__except (InvalidReadExceptionFilter(GetExceptionInformation()))
	{
		// ����ʲôҲ��������hmodΪ��Ч���ʱ����ʱ���Ա�֤�߳�����
		// �������У���pExportDirΪNULL��
	}

	if (pExportDir == NULL)
		return; // ģ��û�е������δ�����ء�

	// ��ȡ�����������
	PDWORD pdwNamesRvas = (PDWORD)((PBYTE)hmod + pExportDir->AddressOfNames);
	PWORD pwNameOrdinals = (PWORD)((PBYTE)hmod + pExportDir->AddressOfNameOrdinals);
	PDWORD pdwFunctionAddresses = (PDWORD)((PBYTE)hmod + pExportDir->AddressOfFunctions);

	// ����������ĵ�������������, ������ƥ��
	for (DWORD n = 0; n < pExportDir->NumberOfNames; n++)
	{
		// ģ�鵼����������ַ���������ANSI��ʽ�����
		PSTR pszFuncName = (PSTR)((PBYTE)hmod + pdwNamesRvas[n]);
		if (lstrcmpiA(pszFuncName, pszFuncName) != 0)
			continue;
		// �ҵ�ָ���ĺ���
		WORD ordinal = pwNameOrdinals[n];
		// ��ȡ�����ĵ�ַ
		PROC* ppfn = (PROC*)&pdwFunctionAddresses[ordinal];
		// ���µĵ�ַд��RVA
		pfnNew = (PROC)((PBYTE)pfnNew - (PBYTE)hmod);
		// �������غ����滻Ϊ�滻����
		if (!WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL) && (ERROR_NOACCESS == GetLastError()))
		{
			// ���ʧ��������ڴ�ҳ��������
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
	// �Ƿ�����DLL�����ģ��Ҳ�滻��Ĭ���Ǳ��ų��ģ�������DLL���滻��
	HMODULE hmodThisMod = ExcludeAPIHookMod ? ModuleFromAddress(ReplaceIATEntryInAllMods) : NULL;
	// ��ȡ����ģ���б�
	CToolHelp th(TH32CS_SNAPMODULE, GetCurrentProcessId());

	MODULEENTRY32 me = { sizeof(me) };
	for (BOOL bOk = th.ModuleFirst(&me); bOk; bOk = th.ModuleNext(&me))
	{
		//ע�⣺���滻�Լ�ģ���еĺ���
		if (me.hModule != hmodThisMod)
		{
			ReplaceIATEntryInOneMod(pszCalleeModName, pfnCurrent, pfnNew, me.hModule);
		}
	}
}

void WINAPI CAPIHook::FixupNewlyLoadedModuled(HMODULE hmod, DWORD dwFlags)
{
	DebugLog("���µ�ģ�鱻����");
	// ���һ��ģ����������أ�������Ӧ��API
	if ((hmod != NULL) &&
		(hmod != ModuleFromAddress(FixupNewlyLoadedModuled)) && // ��Ҫ�����Լ���ģ��
		((dwFlags & LOAD_LIBRARY_AS_DATAFILE) == 0) &&
		((dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) == 0) &&
		((dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) == 0)
		)
	{
		for (CAPIHook* p = sm_pHead; p != NULL; p = p->m_pNext)
		{
			if (p->m_pfnOrig != NULL)
			{
				// ����ֻ�����������ص�DLL�޸���Щ�����صĺ�����ַ���ɣ�
				// ���������DLL�������̬���ӣ�ע�⣬���Ƕ�̬��������DLL������������DLLҲ��
				// ����Ҫ�����صĺ���ʱ���Ͳ������������������Կ��Լ򵥵ص����滻����ģ����������
				ReplaceIATEntryInAllMods(p->m_pszCalleeModName, p->m_pfnOrig, p->m_pfnHook);
			}
			else
			{
#ifdef _DEBUG
				wchar_t szPathname[MAX_PATH];
				GetModuleFileNameW(NULL, szPathname, _countof(szPathname));
				wchar_t sz[1024];
				StringCchPrintfW(sz, _countof(sz),
					TEXT("[%4u - %s]�Ҳ��� %S\r\n"),
					GetCurrentProcessId(), szPathname, p->m_pszCalleeModName);
				OutputDebugString(sz);
#endif
			}
		}
	}
}

HMODULE WINAPI CAPIHook::LoadLibraryA(PCSTR pszModulePath)
{
	DebugLog("���ñ����ص�LoadLibraryA���أ�%s", pszModulePath);
	HMODULE hmod = ::LoadLibraryA(pszModulePath);
	FixupNewlyLoadedModuled(hmod, 0);

	// �����ⲿ��Ϊ��̬����ָ������ʾ��
	// ��ͨ��LoadLibraryA����dynamicLinkDLL.dllʱ�����䵼����dynamicLinkDLLFun�����滻��������Ҫ�ĺ���
	std::string dllPath = std::string(pszModulePath);
	if (dllPath.find("dynamicLinkDLL.dll") != std::string::npos)
	{
		ReplaceEATEntryInOneMod(hmod, "dynamicLinkDLLFun", (PROC)CAPIProvider::newDynamicLinkDLLFun);
	}

	return (hmod);
}

HMODULE WINAPI CAPIHook::LoadLibraryW(PCWSTR pszModulePath)
{
	DebugLog("���ñ����ص�LoadLibraryW���أ�%s", pszModulePath);
	HMODULE hmod = ::LoadLibraryW(pszModulePath);
	FixupNewlyLoadedModuled(hmod, 0);
	return hmod;
}
HMODULE WINAPI CAPIHook::LoadLibraryExA(PCSTR pszModulePath, HANDLE hFile, DWORD dwFlags)
{
	DebugLog("�����ص�LoadLibraryExA");
	HMODULE hmod = ::LoadLibraryExA(pszModulePath, hFile, dwFlags);
	FixupNewlyLoadedModuled(hmod, dwFlags);
	return hmod;
}
HMODULE WINAPI CAPIHook::LoadLibraryExW(PCWSTR pszModulePath, HANDLE hFile, DWORD dwFlags)
{
	DebugLog("�����ص�LoadLibraryExW");
	HMODULE hmod = ::LoadLibraryExW(pszModulePath, hFile, dwFlags);
	FixupNewlyLoadedModuled(hmod, dwFlags);
	return hmod;
}

// ���GetProcAddess��������ʵ��ַ
FARPROC CAPIHook::GetProcAddressRaw(HMODULE hmod, PCSTR pszProcName)
{
	DebugLog("����ԭ�ȵ�GetProcAddress��ȡ%s�ĺ�����ַ", pszProcName);
	return (::GetProcAddress(hmod, pszProcName)); // ����ȫ�ֵ�API
}

// ����ͨ����GetProcAddress���ö�ֱ�ӻ��Ҫ�����غ�����ַ��
// ��ȡ��Hook������������������ʵ��ַ
FARPROC WINAPI CAPIHook::GetProcAddress(HMODULE hmod, PCSTR pszProcName)
{
	DebugLog("���ñ����ص�GetProcAddress��ȡ%s�ĺ�����ַ", pszProcName);
	// ��ȡ��������ʵ��ַ
	FARPROC pfn = GetProcAddressRaw(hmod, pszProcName);
	DebugLog("ԭ�ȵĺ�����ַ��%x", pfn);
	// �жϺ����Ƿ񼺱�hook��
	for (CAPIHook* p = sm_pHead; (pfn != NULL) && (p != NULL); p = p->m_pNext)
	{
		if (pfn == p->m_pfnOrig) // ������ַ��ȣ�˵���Ѿ����ع�
		{
			pfn = p->m_pfnHook;
			DebugLog("�˺��������ع���������ַ��%x", pfn);
			break;
		}
	}

	return pfn;
}

//����LoadLibrary*��GetProcAddress����
CAPIHook CAPIHook::sm_LoadLibraryA("Kernel32.dll", "LoadLibraryA", (PROC)CAPIHook::LoadLibraryA);
CAPIHook CAPIHook::sm_LoadLibrarayW("Kernel32.dll", "LoadLibraryW", (PROC)CAPIHook::LoadLibraryW);
CAPIHook CAPIHook::sm_LoadLibraryExA("Kernel32.dll", "LoadLibraryExA", (PROC)CAPIHook::LoadLibraryExA);
CAPIHook CAPIHook::sm_LoadLibrarayExW("Kernel32.dll", "LoadLibraryExW", (PROC)CAPIHook::LoadLibraryExW);
CAPIHook CAPIHook::sm_GetProcAddress("Kernel32.dll", "GetProcAddress", (PROC)CAPIHook::GetProcAddress);