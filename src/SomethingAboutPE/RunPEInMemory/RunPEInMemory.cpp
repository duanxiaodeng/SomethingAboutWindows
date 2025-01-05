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
		// 获取Dos头
		m_pDosHeaderDynamic = (PIMAGE_DOS_HEADER)pImageBase;
		// 获取NT头
		m_pNtHeaderDynamic = (PIMAGE_NT_HEADERS)((LPBYTE)m_pDosHeaderDynamic + m_pDosHeaderDynamic->e_lfanew);
		if (FALSE == setImageBase(pImageBase))
		{
			return NULL;
		}

		// 5. 填写PE文件导入表信息
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

	// 获取Dos头
	m_pDosHeaderStatic = (PIMAGE_DOS_HEADER)lpData;
	// 获取NT头
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
	// 获取内存映像大小
	DWORD dwSizeOfImage = m_pNtHeaderStatic->OptionalHeader.SizeOfImage;
	// 在进程中开辟一个可读、可写、可执行的内存块
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

	// 将申请的空间的数据全部置0
	RtlZeroMemory(lpBaseAddress, dwSizeOfImage);
	return lpBaseAddress;
}

bool CRunPE::mapFile(LPVOID lpData, LPVOID lpBaseAddress)
{
	// 获取SizeOfHeaders的值: DOS头+NT头+Section头的大小
	DWORD dwSizeOfHeaders = m_pNtHeaderStatic->OptionalHeader.SizeOfHeaders;
	// 获取section的数量
	WORD wNumberOfSections = m_pNtHeaderStatic->FileHeader.NumberOfSections;
	// 获取第一个section头的地址
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNtHeaderStatic);
	// 将所有头（DOS头+NT头+Section头）直接拷贝到内存
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
	return true;
}

bool CRunPE::doRelocationTable(LPVOID lpBaseAddress)
{
	// 获取重定位表的地址
	PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)m_pDosHeaderDynamic + m_pNtHeaderDynamic->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	// 判断是有重定位表
	if ((PVOID)pReloc == (PVOID)m_pDosHeaderDynamic)
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
				DWORD64 dwDelta = (DWORD64)((LPBYTE)lpBaseAddress - m_pNtHeaderDynamic->OptionalHeader.ImageBase);
#elif _WIN32
				DWORD32 dwDelta = (DWORD32)((LPBYTE)lpBaseAddress - m_pNtHeaderDynamic->OptionalHeader.ImageBase);
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

bool CRunPE::doImportTable(LPVOID lpBaseAddress)
{
	// 获取导入表地址 
	PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)m_pDosHeaderDynamic +
		m_pNtHeaderDynamic->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

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
				PIMAGE_IMPORT_BY_NAME lpImportByName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)m_pDosHeaderDynamic + pINT->u1.AddressOfData);
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

bool CRunPE::setImageBase(LPVOID lpBaseAddress)
{
#ifdef _WIN64
	m_pNtHeaderDynamic->OptionalHeader.ImageBase = (ULONGLONG)lpBaseAddress;
#elif _WIN32
	m_pNtHeaderDynamic->OptionalHeader.ImageBase = (DWORD)lpBaseAddress;
#endif

	return true;
}
