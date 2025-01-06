#pragma   once 
#include <Windows.h>  
#include <TlHelp32.h>

#pragma   warning(disable:4244) 
#pragma   warning(disable:4312) 

class CToolHelp
{
public:
	CToolHelp(DWORD dwFlags = 0, DWORD dwProcessID = 0);
	~CToolHelp();
	BOOL CreateSnapshot(DWORD dwFlags, DWORD dwProcessID = 0);
	BOOL ProcessFirst(LPPROCESSENTRY32 ppe) const;
	BOOL ProcessNext(LPPROCESSENTRY32 ppe) const;
	BOOL ProcessFind(DWORD dwProcessID, LPPROCESSENTRY32 ppe) const;
	BOOL ModuleFirst(PMODULEENTRY32 pme) const;
	BOOL ModuleNext(PMODULEENTRY32 pme) const;
	BOOL ModuleFind(PVOID pvBaseAddr, PMODULEENTRY32 pme) const;
	BOOL ModuleFind(PTSTR pszModName, PMODULEENTRY32 pme) const;
	BOOL ThreadFirst(PTHREADENTRY32 pte) const;
	BOOL ThreadNext(PTHREADENTRY32 pte) const;
	BOOL HeapListFirst(PHEAPLIST32 phl) const;
	BOOL HeapListNext(PHEAPLIST32 phl) const;
	int HowManyHeaps() const;
	BOOL HeapFirst(PHEAPENTRY32	phe, DWORD dwProcessID, DWORD dwHeapID) const;
	BOOL HeapNext(PHEAPENTRY32 phe) const;
	int	HowManyBlocksInHeap(DWORD dwProcessID, DWORD dwHeapID) const;
	BOOL IsAHeap(HANDLE hProcess, PVOID pvBlock, PDWORD pdwFlags) const;

public:
	static BOOL EnableDebugPrivilege(BOOL fEnable = TRUE);
	static BOOL ReadProcessMemory(DWORD dwProcessID, LPCVOID lpBaseAddress, LPVOID pvBuffer, DWORD cbSize, PDWORD pdwNumberOfBytesRead = NULL);
	static PVOID GetModulePreferredBaseAddr(DWORD dwProcessID, PVOID pvModuleRemote);

private:
	HANDLE	m_hSnapshot;
};