#include "ToolHelp.h"
#include <strsafe.h>

CToolHelp::CToolHelp(DWORD dwFlags, DWORD dwProcessID) 
{
	m_hSnapshot = INVALID_HANDLE_VALUE;
	CreateSnapshot(dwFlags, dwProcessID);
}

CToolHelp::~CToolHelp() 
{
	CloseHandle(m_hSnapshot);
}

BOOL CToolHelp::CreateSnapshot(DWORD dwFlags, DWORD dwProcessID) 
{
	if (dwFlags == 0) 
	{
		m_hSnapshot = INVALID_HANDLE_VALUE;
	}
	else 
	{
		m_hSnapshot = CreateToolhelp32Snapshot(dwFlags, dwProcessID);
	}

	return m_hSnapshot != INVALID_HANDLE_VALUE;
}

BOOL CToolHelp::EnableDebugPrivilege(BOOL fEnable) 
{
	BOOL fOK = FALSE;
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) 
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		fOK = (GetLastError() == ERROR_SUCCESS);
		CloseHandle(hToken);
	}

	return fOK;
}

BOOL CToolHelp::ReadProcessMemory(DWORD dwProcessID, LPCVOID lpBaseAddress, LPVOID pvBuffer, DWORD cbSize, PDWORD pdwNumberOfBytesRead) 
{
	return Toolhelp32ReadProcessMemory(dwProcessID, lpBaseAddress, pvBuffer, cbSize, (SIZE_T*)pdwNumberOfBytesRead);
}

BOOL CToolHelp::ProcessFirst(LPPROCESSENTRY32 ppe)const 
{
	BOOL fOK = Process32First(m_hSnapshot, ppe);
	if (fOK && (ppe->th32ProcessID == 0))
		fOK = ProcessNext(ppe);
	return fOK;
}

BOOL CToolHelp::ProcessNext(LPPROCESSENTRY32 ppe)const 
{
	BOOL fOK = Process32Next(m_hSnapshot, ppe);
	if (fOK && (ppe->th32ProcessID == 0))
		fOK = ProcessNext(ppe);
	return fOK;
}

BOOL CToolHelp::ProcessFind(DWORD dwProcessID, LPPROCESSENTRY32 ppe)const 
{
	BOOL bFound = FALSE;
	for (BOOL fOK = ProcessFirst(ppe); fOK; fOK = ProcessNext(ppe)) 
	{
		bFound = (ppe->th32ProcessID == dwProcessID);
		if (bFound)
			break;
	}
	return   bFound;
}

BOOL CToolHelp::ModuleFirst(PMODULEENTRY32 pme)const 
{
	return   Module32First(m_hSnapshot, pme);
}

BOOL CToolHelp::ModuleNext(PMODULEENTRY32 pme)const 
{
	return   Module32Next(m_hSnapshot, pme);
}

BOOL CToolHelp::ModuleFind(PVOID pvBaseAddr, PMODULEENTRY32 pme) const 
{
	BOOL bFound = FALSE;
	for (BOOL fOK = ModuleFirst(pme); fOK; fOK = ModuleNext(pme)) 
	{
		bFound = (pme->modBaseAddr == pvBaseAddr);
		if (bFound)
			break;
	}
	return   bFound;
}

BOOL CToolHelp::ModuleFind(PTSTR pszModName, PMODULEENTRY32 pme)const
{
	BOOL bFound = FALSE;
	for (BOOL fOK = ModuleFirst(pme); fOK; fOK = ModuleNext(pme))
	{
		bFound = (lstrcmpi(pme->szModule, pszModName) == 0) 
			|| (lstrcmpi(pme->szExePath, pszModName) == 0);
		if (bFound)
			break;
	}
	return   bFound;
}

BOOL CToolHelp::ThreadFirst(PTHREADENTRY32 pte)const 
{
	return Thread32First(m_hSnapshot, pte);
}

BOOL CToolHelp::ThreadNext(PTHREADENTRY32 pte)const 
{
	return Thread32Next(m_hSnapshot, pte);
}

int CToolHelp::HowManyHeaps() const 
{
	int   nHowManyHeaps = 0;
	HEAPLIST32   hl = { sizeof(hl) };
	for (BOOL fOK = HeapListFirst(&hl); fOK; fOK = HeapListNext(&hl))
		++nHowManyHeaps;
	return   nHowManyHeaps;
}

int CToolHelp::HowManyBlocksInHeap(DWORD dwProcessID, DWORD dwHeapID) const
{
	int   nHowManyBlocksInHeap = 0;
	HEAPENTRY32   he = { sizeof(he) };
	for (BOOL fOK = HeapFirst(&he, dwProcessID, dwHeapID); fOK; fOK = HeapNext(&he))
		++nHowManyBlocksInHeap;
	return   nHowManyBlocksInHeap;
}

BOOL CToolHelp::HeapListFirst(PHEAPLIST32 phl) const 
{
	return Heap32ListFirst(m_hSnapshot, phl);
}

BOOL CToolHelp::HeapListNext(PHEAPLIST32 phl) const 
{
	return Heap32ListNext(m_hSnapshot, phl);
}

BOOL CToolHelp::HeapFirst(PHEAPENTRY32 phe, DWORD dwProcessID, DWORD dwHeapID) const 
{
	return Heap32First(phe, dwProcessID, dwHeapID);
}

BOOL CToolHelp::HeapNext(PHEAPENTRY32 phe)const 
{
	return Heap32Next(phe);
}

BOOL CToolHelp::IsAHeap(HANDLE hProcess, PVOID pvBlock, PDWORD pdwFlags)const 
{
	HEAPLIST32	hl = { sizeof(hl) };
	for (BOOL fOK = HeapListFirst(&hl); fOK; HeapListNext(&hl)) 
	{
		HEAPENTRY32	he = { sizeof(he) };
		for (BOOL   fOk = HeapFirst(&he, hl.th32ProcessID, hl.th32HeapID); fOk; fOk = HeapNext(&he)) 
		{
			MEMORY_BASIC_INFORMATION	mbi;
			VirtualQueryEx(hProcess, (PVOID)he.dwAddress, &mbi, sizeof(mbi));
			if (PBYTE(pvBlock) >= PBYTE(mbi.AllocationBase) && PBYTE(pvBlock) <= (PBYTE(mbi.AllocationBase) + mbi.RegionSize)) 
			{
				*pdwFlags = hl.dwFlags;
			}
		}
		return   FALSE;
	}
}

PVOID CToolHelp::GetModulePreferredBaseAddr(DWORD dwProcessID, PVOID pvModuleRemote) 
{
	PVOID pvModulePreferredBaseAddr = NULL;
	IMAGE_DOS_HEADER idh;
	IMAGE_NT_HEADERS inth;
	Toolhelp32ReadProcessMemory(dwProcessID, pvModuleRemote, &idh, sizeof(idh), NULL);
	if (idh.e_magic == IMAGE_DOS_SIGNATURE) 
	{
		Toolhelp32ReadProcessMemory(dwProcessID, (PBYTE)pvModuleRemote + idh.e_lfanew, &inth, sizeof(inth), NULL);
		if (inth.Signature == IMAGE_NT_SIGNATURE) 
		{
			pvModulePreferredBaseAddr = PVOID(inth.OptionalHeader.ImageBase);
		}
	}
	return   pvModulePreferredBaseAddr;
}
