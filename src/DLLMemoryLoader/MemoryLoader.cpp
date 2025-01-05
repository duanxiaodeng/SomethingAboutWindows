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

// 模拟LoadLibrary加载内存DLL文件到进程中
// lpData: 内存DLL文件数据的基址
// dwSize: 内存DLL文件的内存大小
// 返回值: 内存DLL加载到进程的加载基址
LPVOID MmLoadLibrary(LPVOID lpData, DWORD dwSize)
{
	// 1. 判断是否标准DLL文件
	if (!IsValidDLL(lpData))
	{
		ShowError("Not a DLL.");
		return NULL;
	}

	// 2. 在进程虚拟内存空间中分配内存
	LPVOID lpBaseAddress = AllocVirtualMemory(lpData);
	if (lpBaseAddress == NULL)
		return NULL;

	// 3. 将内存中的DLL数据按SectionAlignment大小对齐映射到进程内存中
	if (FALSE == MmMapFile(lpData, lpBaseAddress))
	{
		ShowError("MmMapFile");
		return NULL;
	}

	// 4. 修改PE文件重定位表信息
	if (FALSE == DoRelocationTable(lpBaseAddress))
	{
		ShowError("DoRelocationTable");
		return NULL;
	}

	// 5. 填写PE文件导入表信息
	if (FALSE == DoImportTable(lpBaseAddress))
	{
		ShowError("DoImportTable");
		return NULL;
	}

	/*******************************************************************
	// 修改PE文件加载基址IMAGE_NT_HEADERS.OptionalHeader.ImageBase
	// 针对VirtualAlloc申请内存时，如果第一个参数传NULL，则需要通过SetImageBase设置PE文件加载基址.
	// 但是如果传NULL来申请内存，则内存加载64位dll时，调用DLLMain时会崩溃，尚不清楚为什么
	if (FALSE == SetImageBase(lpBaseAddress))
	{
	ShowError("SetImageBase");
	return NULL;
	}
	*******************************************************************/

	// 6. 调用DLL的入口函数DllMain，函数地址即为PE文件的入口点IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
	if (FALSE == CallDllMain(lpBaseAddress, DLL_PROCESS_ATTACH))
	{
		ShowError("CallDllMain");
		return NULL;
	}

	return lpBaseAddress;
}

// 判断是否合法的DLL文件
// lpData: 内存DLL文件数据的基址
// 返回值: 是DLL返回TRUE，否则返回FALSE
BOOL IsValidDLL(LPVOID lpData)
{
	if (lpData == NULL)
		return FALSE;

	// 获取Dos头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// 获取NT头
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

// 在进程中开辟一个可读、可写、可执行的内存块
// lpData: 内存DLL文件数据的基址
// 返回值：分配的内存基址
LPVOID AllocVirtualMemory(LPVOID lpData)
{
	// 获取DOS头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// 获取NT头
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// 获取内存映像大小
	DWORD dwSizeOfImage = pNtHeader->OptionalHeader.SizeOfImage;
	// 在进程中开辟一个可读、可写、可执行的内存块
	LPVOID lpBaseAddress = VirtualAlloc((LPVOID)pNtHeader->OptionalHeader.ImageBase, dwSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (NULL == lpBaseAddress)
	{
		ShowError("VirtualAlloc");
		return NULL;
	}
	// 将申请的空间的数据全部置0
	RtlZeroMemory(lpBaseAddress, dwSizeOfImage);
	return lpBaseAddress;
}

// 将内存DLL数据按SectionAlignment大小对齐映射到进程内存中
// lpData: 内存DLL文件数据的基址
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL MmMapFile(LPVOID lpData, LPVOID lpBaseAddress)
{
	// 获取Dos头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpData;
	// 获取NT头
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// 获取SizeOfHeaders的值: DOS头+NT头+Section头的大小
	DWORD dwSizeOfHeaders = pNtHeader->OptionalHeader.SizeOfHeaders;
	// 获取section的数量
	WORD wNumberOfSections = pNtHeader->FileHeader.NumberOfSections;
	// 获取第一个section头的地址
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeader);
	// 将所有头（DOS头+NT头+Section头）拷贝到内存
	RtlMoveMemory(lpBaseAddress, lpData, dwSizeOfHeaders);
	// 对齐SectionAlignment循环加载section
	for (WORD i = 0; i < wNumberOfSections; i++)
	{
		// 过滤无效section
		if ((0 == pSectionHeader->VirtualAddress) ||
			(0 == pSectionHeader->SizeOfRawData))
		{
			pSectionHeader++;
			continue;
		}
		// 获取section在文件中的位置
		LPVOID lpSrcMem = (LPVOID)((LPBYTE)lpData + pSectionHeader->PointerToRawData);
		// 获取section映射到内存中的位置
		LPVOID lpDestMem = (LPVOID)((LPBYTE)lpBaseAddress + pSectionHeader->VirtualAddress);
		// 获取section在文件中的大小
		DWORD dwSizeOfRawData = pSectionHeader->SizeOfRawData;
		// 将section拷贝至内存中
		RtlMoveMemory(lpDestMem, lpSrcMem, dwSizeOfRawData);
		pSectionHeader++;
	}
	return TRUE;
}


// 修改PE文件重定位表信息
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL DoRelocationTable(LPVOID lpBaseAddress)
{
	// 获取Dos头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// 获取NT头
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// 获取重定位表的地址
	PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)pDosHeader + pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	// 判断是有重定位表
	if ((PVOID)pReloc == (PVOID)pDosHeader)
	{
		return TRUE;
	}

	// 开始扫描重定位表
	// 注意：重定位表的位置可能和文件在磁盘中的偏移地址不同，应该使用加载后的地址
	while (pReloc->VirtualAddress != 0 && pReloc->SizeOfBlock != 0)
	{
		WORD* pLocData = (WORD*)((PBYTE)pReloc + sizeof(IMAGE_BASE_RELOCATION));
		//计算本区域需要修正的重定位项的数量（每个重定位表描述了4KB大小的区域的重定位信息）
		int nNumberOfReloc = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		for (int i = 0; i < nNumberOfReloc; i++)
		{
			if ((DWORD)(pLocData[i] & 0xF000) == 0x3000) //这是一个需要修正的地址
			{
				// 获取需要修正数据的地址
				PDWORD pAddress = (PDWORD)((LPBYTE)lpBaseAddress + pReloc->VirtualAddress + (pLocData[i] & 0x0FFF));
				// 计算偏移量：实际加载地址 - 内存基址
#ifdef _WIN64
				DWORD64 dwDelta = (DWORD64)((LPBYTE)lpBaseAddress - pNtHeader->OptionalHeader.ImageBase);
#elif _WIN32
				DWORD32 dwDelta = (DWORD32)((LPBYTE)lpBaseAddress - pNtHeader->OptionalHeader.ImageBase);
#endif
				// 修改
				*pAddress += dwDelta;
			}
		}

		// 下一个重定位块
		pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)pReloc + pReloc->SizeOfBlock);
	}
	return TRUE;
}


// 填写PE文件导入表信息
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL DoImportTable(LPVOID lpBaseAddress)
{
	// 获取DOS头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// 获取NT头
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// 获取导入表地址 
	PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)pDosHeader +
		pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// 循环遍历DLL导入表中的依赖DLL及获取导入表中的函数地址
	while (pImportTable->Name)
	{
		// 获取导入表中DLL的名称
		char* lpDllName = (char*)((LPBYTE)lpBaseAddress + pImportTable->Name);
		// 检索DLL模块获取模块句柄
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

		// 获取INT表和IAT表
		PIMAGE_THUNK_DATA pINT = (PIMAGE_THUNK_DATA)((LPBYTE)lpBaseAddress + pImportTable->OriginalFirstThunk);
		PIMAGE_THUNK_DATA pIAT = (PIMAGE_THUNK_DATA)((LPBYTE)lpBaseAddress + pImportTable->FirstThunk);
		while (pINT->u1.Ordinal)
		{
#ifdef _WIN64
			DWORD64 lpFuncAddress;
#elif _WIN32
			DWORD32 lpFuncAddress;
#endif
			// 判断导出函数是序号导出还是函数名称导出
			if (pINT->u1.Ordinal & IMAGE_ORDINAL_FLAG) // 最高位为1，则是序号导入
			{
				// 当IMAGE_THUNK_DATA值的最高位为1时，表示函数以序号方式导入，这时低位被看做是一个函数序号
#ifdef _WIN64
				lpFuncAddress = (DWORD64)GetProcAddress(hModule, (LPCSTR)(pINT->u1.Ordinal & 0x7FFFFFFF));
#elif _WIN32
				lpFuncAddress = (DWORD32)GetProcAddress(hModule, (LPCSTR)(pINT->u1.Ordinal & 0x7FFFFFFF));
#endif	
			}
			else // 否则，名称导入
			{
				// 获取IMAGE_IMPORT_BY_NAME结构
				PIMAGE_IMPORT_BY_NAME lpImportByName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)pDosHeader + pINT->u1.AddressOfData);
#ifdef _WIN64
				lpFuncAddress = (DWORD64)GetProcAddress(hModule, (LPCSTR)lpImportByName->Name);
#elif _WIN32
				lpFuncAddress = (DWORD32)GetProcAddress(hModule, (LPCSTR)lpImportByName->Name);
#endif	
			}

			// 将函数地址填入到IAT表中
			// 注意此处的函数地址表的赋值，要对照PE格式进行装载，不要理解错了！！！
			pIAT->u1.Function = lpFuncAddress;
			pINT++;
			pIAT++;
		}
		pImportTable++;
	}
	return TRUE;
}


// 修改PE文件加载基址IMAGE_NT_HEADERS.OptionalHeader.ImageBase
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
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


// 调用DLL的入口函数DllMain,函数地址即为PE文件的入口点IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL CallDllMain(LPVOID lpBaseAddress, DWORD ul_reason_for_call)
{
	// 获取DOS头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	// 获取NT头
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	// 获取DLL入口函数指针
	DLLMAIN DllMain = (DLLMAIN)((LPBYTE)pDosHeader + pNtHeader->OptionalHeader.AddressOfEntryPoint);
	// 调用入口函数
	BOOL bRet = DllMain((HMODULE)lpBaseAddress, ul_reason_for_call, NULL);
	if (bRet == FALSE)
	{
		ShowError("DllMain");
	}

	return bRet;
}


// 模拟GetProcAddress获取内存DLL的导出函数
// lpBaseAddress: 内存DLL文件加载到进程中的加载基址
// lpszFuncName: 导出函数的名字
// 返回值: 返回导出函数的的地址
LPVOID MmGetProcAddress(LPVOID lpBaseAddress, PCHAR lpszFuncName)
{
	LPVOID lpFunc = NULL;
	// 获取导出表
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY pExportTable = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)pDosHeader +
		pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	// 获取导出表的数据
	PDWORD lpAddressOfNamesArray = (PDWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfNames);
	PWORD lpAddressOfNameOrdinalsArray = (PWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfNameOrdinals);
	PDWORD lpAddressOfFunctionsArray = (PDWORD)((LPBYTE)pDosHeader + pExportTable->AddressOfFunctions);

	// 遍历导出表的导出函数的名称, 并进行匹配
	for (DWORD i = 0; i < pExportTable->NumberOfNames; i++)
	{
		PCHAR lpFuncName = (PCHAR)((LPBYTE)pDosHeader + lpAddressOfNamesArray[i]);
		if (0 == ::lstrcmpi(lpFuncName, lpszFuncName))
		{
			// 获取导出函数地址
			WORD wHint = lpAddressOfNameOrdinalsArray[i];
			lpFunc = (LPVOID)((LPBYTE)pDosHeader + lpAddressOfFunctionsArray[wHint]);
			break;
		}
	}

	return lpFunc;
}


// 释放从内存加载的DLL到进程内存的空间
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
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