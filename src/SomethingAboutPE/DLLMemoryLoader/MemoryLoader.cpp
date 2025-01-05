#include "MemoryLoader.h"
#include <windows.h>
#include <stdio.h>
#include <iostream>

void ShowError(char* lpszText)
{
	char szErr[MAX_PATH] = { 0 };
	wsprintf(szErr, "%s Error, Error Code is: %d\n", lpszText, GetLastError());
	MessageBox(NULL, szErr, "ERROR", MB_OK | MB_ICONERROR);
}

// ģ��LoadLibrary�����ڴ�DLL�ļ���������
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// dwSize: �ڴ�DLL�ļ����ڴ��С
// ����ֵ: �ڴ�DLL���ص����̵ļ��ػ�ַ
LPVOID MmLoadLibrary(LPVOID lpData, DWORD dwSize)
{
	// 1. �ж��Ƿ��׼DLL�ļ�
	if (!IsValidDLL(lpData))
	{
		ShowError("Not a DLL.");
		return NULL;
	}

	// 2. �ڽ��������ڴ�ռ��з����ڴ�
	LPVOID lpBaseAddress = AllocVirtualMemory(lpData);
	if (lpBaseAddress == NULL)
		return NULL;

	// 3. ���ڴ��е�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ���
	if (FALSE == MmMapFile(lpData, lpBaseAddress))
	{
		ShowError("MmMapFile");
		return NULL;
	}

	// 4. �޸�PE�ļ��ض�λ����Ϣ
	if (FALSE == DoRelocationTable(lpBaseAddress))
	{
		ShowError("DoRelocationTable");
		return NULL;
	}

	// 5. ��дPE�ļ��������Ϣ
	if (FALSE == DoImportTable(lpBaseAddress))
	{
		ShowError("DoImportTable");
		return NULL;
	}

	/*******************************************************************
	// �޸�PE�ļ����ػ�ַIMAGE_NT_HEADERS.OptionalHeader.ImageBase
	// ���VirtualAlloc�����ڴ�ʱ�������һ��������NULL������Ҫͨ��SetImageBase����PE�ļ����ػ�ַ.
	// ���������NULL�������ڴ棬���ڴ����64λdllʱ������DLLMainʱ��������в����Ϊʲô
	if (FALSE == SetImageBase(lpBaseAddress))
	{
	ShowError("SetImageBase");
	return NULL;
	}
	*******************************************************************/

	// 6. ����DLL����ں���DllMain��������ַ��ΪPE�ļ�����ڵ�IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
	if (FALSE == CallDllMain(lpBaseAddress, DLL_PROCESS_ATTACH))
	{
		ShowError("CallDllMain");
		return NULL;
	}

	return lpBaseAddress;
}

// �ж��Ƿ�Ϸ���DLL�ļ�
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// ����ֵ: ��DLL����TRUE�����򷵻�FALSE
BOOL IsValidDLL(LPVOID lpData)
{
	if (lpData == NULL)
		return FALSE;

	// ��ȡDosͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);

#ifdef _WIN64
	if (pNtHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		return FALSE;
#elif _WIN32
	if (pNtHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		return FALSE;
#endif

	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || pNtHeader->Signature != IMAGE_NT_SIGNATURE)
		return FALSE;
	if (!(pNtHeader->FileHeader.Characteristics & IMAGE_FILE_DLL))
		return FALSE;

	return TRUE;

}

// �ڽ����п���һ���ɶ�����д����ִ�е��ڴ��
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// ����ֵ��������ڴ��ַ
LPVOID AllocVirtualMemory(LPVOID lpData)
{
	// ��ȡDOSͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// ��ȡ�ڴ�ӳ���С
	DWORD dwSizeOfImage = pNtHeader->OptionalHeader.SizeOfImage;
	// �ڽ����п���һ���ɶ�����д����ִ�е��ڴ��
	LPVOID lpBaseAddress = VirtualAlloc((LPVOID)pNtHeader->OptionalHeader.ImageBase, dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (NULL == lpBaseAddress)
	{
		ShowError("VirtualAlloc");
		return NULL;
	}
	// ������Ŀռ������ȫ����0
	RtlZeroMemory(lpBaseAddress, dwSizeOfImage);
	return lpBaseAddress;
}

// ���ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ���
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL MmMapFile(LPVOID lpData, LPVOID lpBaseAddress)
{
	// ��ȡDosͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// ��ȡSizeOfHeaders��ֵ: DOSͷ+NTͷ+Sectionͷ�Ĵ�С
	DWORD dwSizeOfHeaders = pNtHeader->OptionalHeader.SizeOfHeaders;
	// ��ȡsection������
	WORD wNumberOfSections = pNtHeader->FileHeader.NumberOfSections;
	// ��ȡ��һ��sectionͷ�ĵ�ַ
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeader);
	// ������ͷ��DOSͷ+NTͷ+Sectionͷ���������ڴ�
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
	return TRUE;
}


// �޸�PE�ļ��ض�λ����Ϣ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL DoRelocationTable(LPVOID lpBaseAddress)
{
	// ��ȡDosͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// ��ȡ�ض�λ��ĵ�ַ
	PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)pDosHeader + pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	// �ж������ض�λ��
	if ((PVOID)pReloc == (PVOID)pDosHeader)
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
				DWORD64 dwDelta = (DWORD64)((LPBYTE)lpBaseAddress - pNtHeader->OptionalHeader.ImageBase);
#elif _WIN32
				DWORD32 dwDelta = (DWORD32)((LPBYTE)lpBaseAddress - pNtHeader->OptionalHeader.ImageBase);
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


// ��дPE�ļ��������Ϣ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL DoImportTable(LPVOID lpBaseAddress)
{
	// ��ȡDOSͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// ��ȡ������ַ 
	PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)pDosHeader +
		pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

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
				PIMAGE_IMPORT_BY_NAME lpImportByName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)pDosHeader + pINT->u1.AddressOfData);
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


// �޸�PE�ļ����ػ�ַIMAGE_NT_HEADERS.OptionalHeader.ImageBase
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL SetImageBase(LPVOID lpBaseAddress)
{
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);

#ifdef _WIN64
	pNtHeader->OptionalHeader.ImageBase = (ULONGLONG)lpBaseAddress;
#elif _WIN32
	pNtHeader->OptionalHeader.ImageBase = (DWORD)lpBaseAddress;
#endif

	return TRUE;
}


// ����DLL����ں���DllMain,������ַ��ΪPE�ļ�����ڵ�IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL CallDllMain(LPVOID lpBaseAddress, DWORD ul_reason_for_call)
{
	// ��ȡDOSͷ
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// ��ȡNTͷ
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// ��ȡDLL��ں���ָ��
	DLLMAIN DllMain = (DLLMAIN)((LPBYTE)pDosHeader + pNtHeader->OptionalHeader.AddressOfEntryPoint);
	// ������ں���
	BOOL bRet = DllMain((HMODULE)lpBaseAddress, ul_reason_for_call, NULL);
	if (bRet == FALSE)
	{
		ShowError("DllMain");
	}

	return bRet;
}


// ģ��GetProcAddress��ȡ�ڴ�DLL�ĵ�������
// lpBaseAddress: �ڴ�DLL�ļ����ص������еļ��ػ�ַ
// lpszFuncName: ��������������
// ����ֵ: ���ص��������ĵĵ�ַ
LPVOID MmGetProcAddress(LPVOID lpBaseAddress, PCHAR lpszFuncName)
{
	LPVOID lpFunc = NULL;
	// ��ȡ������
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY pExportTable = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)pDosHeader +
		pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	// ��ȡ�����������
	PDWORD lpAddressOfNamesArray = (PDWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfNames);
	PWORD lpAddressOfNameOrdinalsArray = (PWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfNameOrdinals);
	PDWORD lpAddressOfFunctionsArray = (PDWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfFunctions);

	// ����������ĵ�������������, ������ƥ��
	for (DWORD i = 0; i < pExportTable->NumberOfNames; i++)
	{
		PCHAR lpFuncName = (PCHAR)((LPBYTE)pDosHeader + lpAddressOfNamesArray[i]);
		if (0 == ::lstrcmpi(lpFuncName, lpszFuncName))
		{
			// ��ȡ����������ַ
			WORD wHint = lpAddressOfNameOrdinalsArray[i];
			lpFunc = (LPVOID)((LPBYTE)pDosHeader + lpAddressOfFunctionsArray[wHint]);
			break;
		}
	}

	return lpFunc;
}


// �ͷŴ��ڴ���ص�DLL�������ڴ�Ŀռ�
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL MmFreeLibrary(LPVOID lpBaseAddress)
{
	BOOL bRet = FALSE;

	if (FALSE == CallDllMain(lpBaseAddress, DLL_PROCESS_DETACH))
	{
		ShowError("CallDllMain");
		return bRet;
	}

	if (NULL == lpBaseAddress)
	{
		return bRet;
	}

	bRet = VirtualFree(lpBaseAddress, 0, MEM_RELEASE);
	lpBaseAddress = NULL;

	return bRet;
}