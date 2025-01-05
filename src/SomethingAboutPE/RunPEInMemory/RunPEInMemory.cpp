#include "RunPEInMemory.h"
#include <stdio.h>
#include <iostream>

CRunPE::CRunPE(std::string exePath)
	: m_exePath(exePath)
{
}

CRunPE::~CRunPE()
{

}

bool CRunPE::runPE()
{
	bool res = false;

	do
	{
		if (!readPEFile(m_exePath, m_peData, m_peSize))
			break;

		if (!isValidPE(m_peData))
			break;

		LPVOID preferAddr = 0;
		LPVOID pImageBase = allocVirtualMemory(m_peData);
		if (pImageBase == NULL)
			return false;

		preferAddr = (LPVOID)m_pNtHeaderStatic->OptionalHeader.ImageBase;
		

		if (FALSE == mapFile(m_peData, pImageBase))
		{
			return NULL;
		}
		// ��ȡDosͷ
		m_pDosHeaderDynamic = (PIMAGE_DOS_HEADER)pImageBase;
		// ��ȡNTͷ
		m_pNtHeaderDynamic = (PIMAGE_NT_HEADERS)((LPBYTE)m_pDosHeaderDynamic + m_pDosHeaderDynamic->e_lfanew);
		if (FALSE == setImageBase(pImageBase))
		{
			return NULL;
		}

		// 5. ��дPE�ļ��������Ϣ
		if (FALSE == doImportTable(pImageBase))
		{
			return false;
		}


		if (pImageBase != preferAddr)
			doRelocationTable(pImageBase);
		size_t retAddr = (size_t)(pImageBase) + m_pNtHeaderDynamic->OptionalHeader.AddressOfEntryPoint;

		((void(*)())retAddr)();

	} while (0);

	return res;
}

bool CRunPE::readPEFile(std::string exePath, BYTE*& bufPtr, int64_t& length)
{
	if (FILE* fp = fopen(exePath.c_str(), "rb"))
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		bufPtr = new BYTE[length + 1];
		fseek(fp, 0, SEEK_SET);
		fread(bufPtr, sizeof(BYTE), length, fp);
		return true;
	}

	return false;
}

bool CRunPE::isValidPE(LPVOID lpData)
{
	if (lpData == NULL)
		return false;

	// ��ȡDosͷ
	m_pDosHeaderStatic = (PIMAGE_DOS_HEADER)lpData;
	// ��ȡNTͷ
	m_pNtHeaderStatic = (PIMAGE_NT_HEADERS)((LPBYTE)m_pDosHeaderStatic + m_pDosHeaderStatic->e_lfanew);

#ifdef _WIN64
	if (m_pNtHeaderStatic->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		return false;
#elif _WIN32
	if (m_pNtHeaderStatic->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		return false;
#endif

	if (m_pDosHeaderStatic->e_magic != IMAGE_DOS_SIGNATURE || m_pNtHeaderStatic->Signature != IMAGE_NT_SIGNATURE)
		return false;

	return true;
}

LPVOID CRunPE::allocVirtualMemory(LPVOID lpData)
{
	// ��ȡ�ڴ�ӳ���С
	DWORD dwSizeOfImage = m_pNtHeaderStatic->OptionalHeader.SizeOfImage;
	// �ڽ����п���һ���ɶ�����д����ִ�е��ڴ��
	LPVOID lpBaseAddress = VirtualAlloc((LPVOID)m_pNtHeaderStatic->OptionalHeader.ImageBase, dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!lpBaseAddress)
	{
		lpBaseAddress = (BYTE*)VirtualAlloc(NULL, dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!lpBaseAddress)
		{
			// ShowError("Allocate Memory For Image Base Failure.");
			return NULL;
		}
	}

	// ������Ŀռ������ȫ����0
	RtlZeroMemory(lpBaseAddress, dwSizeOfImage);
	return lpBaseAddress;
}

bool CRunPE::mapFile(LPVOID lpData, LPVOID lpBaseAddress)
{
	// ��ȡSizeOfHeaders��ֵ: DOSͷ+NTͷ+Sectionͷ�Ĵ�С
	DWORD dwSizeOfHeaders = m_pNtHeaderStatic->OptionalHeader.SizeOfHeaders;
	// ��ȡsection������
	WORD wNumberOfSections = m_pNtHeaderStatic->FileHeader.NumberOfSections;
	// ��ȡ��һ��sectionͷ�ĵ�ַ
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNtHeaderStatic);
	// ������ͷ��DOSͷ+NTͷ+Sectionͷ��ֱ�ӿ������ڴ�
	RtlMoveMemory(lpBaseAddress, lpData, dwSizeOfHeaders);
	// ����SectionAlignmentѭ������section
	for (WORD i = 0; i < wNumberOfSections; i++)
	{
		// ������Чsection
		if ((0 == pSectionHeader->VirtualAddress) ||
			(0 == pSectionHeader->SizeOfRawData))
		{
			pSectionHeader++;
			continue;
		}
		// ��ȡsection���ļ��е�λ��
		LPVOID lpSrcMem = (LPVOID)((LPBYTE)lpData + pSectionHeader->PointerToRawData);
		// ��ȡsectionӳ�䵽�ڴ��е�λ��
		LPVOID lpDestMem = (LPVOID)((LPBYTE)lpBaseAddress + pSectionHeader->VirtualAddress);
		// ��ȡsection���ļ��еĴ�С
		DWORD dwSizeOfRawData = pSectionHeader->SizeOfRawData;
		// ��section�������ڴ���
		RtlMoveMemory(lpDestMem, lpSrcMem, dwSizeOfRawData);
		pSectionHeader++;
	}
	return true;
}

bool CRunPE::doRelocationTable(LPVOID lpBaseAddress)
{
	// ��ȡ�ض�λ��ĵ�ַ
	PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)m_pDosHeaderDynamic + m_pNtHeaderDynamic->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	// �ж������ض�λ��
	if ((PVOID)pReloc == (PVOID)m_pDosHeaderDynamic)
	{
		return TRUE;
	}

	// ��ʼɨ���ض�λ��
	// ע�⣺�ض�λ���λ�ÿ��ܺ��ļ��ڴ����е�ƫ�Ƶ�ַ��ͬ��Ӧ��ʹ�ü��غ�ĵ�ַ
	while (pReloc->VirtualAddress != 0 && pReloc->SizeOfBlock != 0)
	{
		WORD* pLocData = (WORD*)((PBYTE)pReloc + sizeof(IMAGE_BASE_RELOCATION));
		//���㱾������Ҫ�������ض�λ���������ÿ���ض�λ��������4KB��С��������ض�λ��Ϣ��
		int nNumberOfReloc = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		for (int i = 0; i < nNumberOfReloc; i++)
		{
			if ((DWORD)(pLocData[i] & 0xF000) == 0x3000) //����һ����Ҫ�����ĵ�ַ
			{
				// ��ȡ��Ҫ�������ݵĵ�ַ
				PDWORD pAddress = (PDWORD)((LPBYTE)lpBaseAddress + pReloc->VirtualAddress + (pLocData[i] & 0x0FFF));
				// ����ƫ������ʵ�ʼ��ص�ַ - �ڴ��ַ
#ifdef _WIN64
				DWORD64 dwDelta = (DWORD64)((LPBYTE)lpBaseAddress - m_pNtHeaderDynamic->OptionalHeader.ImageBase);
#elif _WIN32
				DWORD32 dwDelta = (DWORD32)((LPBYTE)lpBaseAddress - m_pNtHeaderDynamic->OptionalHeader.ImageBase);
#endif
				// �޸�
				*pAddress += dwDelta;
			}
		}

		// ��һ���ض�λ��
		pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)pReloc + pReloc->SizeOfBlock);
	}
	return TRUE;
}

bool CRunPE::doImportTable(LPVOID lpBaseAddress)
{
	// ��ȡ������ַ 
	PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)m_pDosHeaderDynamic +
		m_pNtHeaderDynamic->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// ѭ������DLL������е�����DLL����ȡ������еĺ�����ַ
	while (pImportTable->Name)
	{
		// ��ȡ�������DLL������
		char* lpDllName = (char*)((LPBYTE)lpBaseAddress + pImportTable->Name);
		// ����DLLģ���ȡģ����
		HMODULE hModule = GetModuleHandleA(lpDllName);
		if (hModule == NULL)
		{
			hModule = LoadLibraryA(lpDllName);
			if (hModule == NULL)
			{
				pImportTable++;
				continue;
			}
		}

		// ��ȡINT���IAT��
		PIMAGE_THUNK_DATA pINT = (PIMAGE_THUNK_DATA)((LPBYTE)lpBaseAddress + pImportTable->OriginalFirstThunk);
		PIMAGE_THUNK_DATA pIAT = (PIMAGE_THUNK_DATA)((LPBYTE)lpBaseAddress + pImportTable->FirstThunk);
		while (pINT->u1.Ordinal)
		{
#ifdef _WIN64
			DWORD64 lpFuncAddress;
#elif _WIN32
			DWORD32 lpFuncAddress;
#endif
			// �жϵ�����������ŵ������Ǻ������Ƶ���
			if (pINT->u1.Ordinal & IMAGE_ORDINAL_FLAG) // ���λΪ1��������ŵ���
			{
				// ��IMAGE_THUNK_DATAֵ�����λΪ1ʱ����ʾ��������ŷ�ʽ���룬��ʱ��λ��������һ���������
#ifdef _WIN64
				lpFuncAddress = (DWORD64)GetProcAddress(hModule, (LPCSTR)(pINT->u1.Ordinal & 0x7FFFFFFF));
#elif _WIN32
				lpFuncAddress = (DWORD32)GetProcAddress(hModule, (LPCSTR)(pINT->u1.Ordinal & 0x7FFFFFFF));
#endif	
			}
			else // �������Ƶ���
			{
				// ��ȡIMAGE_IMPORT_BY_NAME�ṹ
				PIMAGE_IMPORT_BY_NAME lpImportByName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)m_pDosHeaderDynamic + pINT->u1.AddressOfData);
#ifdef _WIN64
				lpFuncAddress = (DWORD64)GetProcAddress(hModule, (LPCSTR)lpImportByName->Name);
#elif _WIN32
				lpFuncAddress = (DWORD32)GetProcAddress(hModule, (LPCSTR)lpImportByName->Name);
#endif	
			}

			// ��������ַ���뵽IAT����
			// ע��˴��ĺ�����ַ��ĸ�ֵ��Ҫ����PE��ʽ����װ�أ���Ҫ�����ˣ�����
			pIAT->u1.Function = lpFuncAddress;
			pINT++;
			pIAT++;
		}
		pImportTable++;
	}
	return TRUE;
}

bool CRunPE::setImageBase(LPVOID lpBaseAddress)
{
#ifdef _WIN64
	m_pNtHeaderDynamic->OptionalHeader.ImageBase = (ULONGLONG)lpBaseAddress;
#elif _WIN32
	m_pNtHeaderDynamic->OptionalHeader.ImageBase = (DWORD)lpBaseAddress;
#endif

	return true;
}
