#ifndef _RUNPEINMEMORY_H_
#define _RUNPEINMEMORY_H_

#include <string>
#include <Windows.h>

class CRunPE
{
public:
	CRunPE(std::string exePath);
	~CRunPE();
	bool runPE();

private:
	bool readPEFile(std::string exePath, BYTE*& bufPtr, int64_t& length);
	bool isValidPE(LPVOID lpData);
	LPVOID allocVirtualMemory(LPVOID lpData);
	bool mapFile(LPVOID lpData, LPVOID lpBaseAddress);
	bool doRelocationTable(LPVOID lpBaseAddress);
	bool doImportTable(LPVOID lpBaseAddress);
	bool setImageBase(LPVOID lpBaseAddress);

private:
	std::string m_exePath;

	BYTE* m_peData = NULL; // PE文件内容
	int64_t m_peSize = 0; // PE文件长度
	PIMAGE_DOS_HEADER m_pDosHeaderStatic = NULL; // DOS头，PE文件静态存储时
	PIMAGE_NT_HEADERS m_pNtHeaderStatic = NULL; // NT头，PE文件静态存储时

	PIMAGE_DOS_HEADER m_pDosHeaderDynamic = NULL; // DOS头，PE文件动态加载到内存中后
	PIMAGE_NT_HEADERS m_pNtHeaderDynamic = NULL; // NT头，PE文件动态加载到内存中后
};

#endif