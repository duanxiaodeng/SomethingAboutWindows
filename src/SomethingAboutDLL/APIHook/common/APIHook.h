#pragma once
#include <windows.h>

class CAPIHook
{
public:
	// ���ؽ���ע��Ľ�������ģ����ָ����API
	CAPIHook(PSTR pszCalleeModName, // �����غ������ڵ�ģ�飨DLL��
		PSTR pszFuncName,		// Ҫ�����غ���������
		PROC pfnHook);			// ���غ�����Ҳ�������������ַ

	// �ӱ�ע��Ľ�������ģ����ж��һ����������:���غ�����
	~CAPIHook();

	// ��������ġ������غ����ĵ�ַ
	// �������ϣ���ڵ������غ����Ժ��ٵ���ԭ�ȵĺ���������Ҫʹ����
	operator PROC() { return (m_pfnOrig); }

	// �Ƿ�����ǰ����CAPIHook��ģ��(����DLL����ҲҪ���أ�
	// ���������ReplaceIATEntryInAllMods�л�ʹ�õ������ﱻ����Ϊstatic
	static BOOL ExcludeAPIHookMod;

public:
	// ����������GetProcAddress
	static FARPROC WINAPI GetProcAddressRaw(HMODULE hmod, PCSTR pszProcName);
	static void DebugLog(const char *fmt, ...);

public:
	// ��һ��ģ��ĵ�������滻ָ�������ĵ�ַ��
	// ����hmodCallerģ�鵼����pszCalleeModeNameģ�����pfnOrg������
	// ����Ҫ��hmodCaller������е�pfnOrg�����滻ΪpfnHook������
	static void WINAPI ReplaceIATEntryInOneMod(
		PCSTR pszCalleeModName,		// ������ģ�������(DLL)
		PROC pfnOrig,				// �����غ����ĵ�ַ��pszCalleeModeNameģ����
		PROC pfnHook,				// ���غ����������滻�������ĵ�ַ
		HMODULE hmodCaller);		// ����ģ�飬����Ҫ�޸ĵ���ε�ģ��

	// �滻��ǰ�����У�����ģ��ĵ������ָ��������ַΪ�����غ����ĵ�ַ
	static void WINAPI ReplaceIATEntryInAllMods(PCSTR pszCalleeModName, PROC pfnOrig, PROC pfnHook);

	// ��һ��ģ��ĵ��������滻ָ�������ĵ�ַ
	static void WINAPI ReplaceEATEntryInOneMod(HMODULE hmod, PCSTR pszFunctionName, PROC pfnNew);

private:
	// ��DLL���ڱ������Ժ���ؽ���ʱ��ʹ�������������
	static void WINAPI FixupNewlyLoadedModuled(HMODULE hmod, DWORD dwFlags);

	// ��DLL������ʱ��̬�����ؽ���ʱʹ��
	static HMODULE WINAPI LoadLibraryA(PCSTR pszModulePath);
	static HMODULE WINAPI LoadLibraryW(PCWSTR pszModulePath);
	static HMODULE WINAPI LoadLibraryExA(PCSTR pszModulePath, HANDLE hFile, DWORD dwFlags);
	static HMODULE WINAPI LoadLibraryExW(PCWSTR pszModulePath, HANDLE hFile, DWORD dwFlags);
	// ��������Ѿ������ع����򷵻ر��滻��������ʵ��ַ�������������У�
	static FARPROC WINAPI GetProcAddress(HMODULE hmod, PCSTR pszProcName);

private:
	static CAPIHook* sm_pHead;	// �����У���һ�������غ���
	CAPIHook* m_pNext;			// ��һ�������غ���

	PCSTR m_pszCalleeModName;	// �������غ�����ģ�����ƣ�ANSI��
	PCSTR m_pszFuncName;		// �����صĺ������ƣ�ANSI��
	PROC m_pfnOrig;				// �ڱ���������ԭʼ�ĺ���,Ҳ���Ǳ����صĺ���
	PROC m_pfnHook;				// ���غ������������m_pfnOrig

private:
	// ʵ����������Щ����
	static CAPIHook  sm_LoadLibraryA;
	static CAPIHook  sm_LoadLibrarayW;
	static CAPIHook  sm_LoadLibraryExA;
	static CAPIHook  sm_LoadLibrarayExW;
	static CAPIHook  sm_GetProcAddress;
};