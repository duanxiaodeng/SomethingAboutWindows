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

	BYTE* m_peData = NULL; // PE�ļ�����
	int64_t m_peSize = 0; // PE�ļ�����
	PIMAGE_DOS_HEADER m_pDosHeaderStatic = NULL; // DOSͷ��PE�ļ���̬�洢ʱ
	PIMAGE_NT_HEADERS m_pNtHeaderStatic = NULL; // NTͷ��PE�ļ���̬�洢ʱ

	PIMAGE_DOS_HEADER m_pDosHeaderDynamic = NULL; // DOSͷ��PE�ļ���̬���ص��ڴ��к�
	PIMAGE_NT_HEADERS m_pNtHeaderDynamic = NULL; // NTͷ��PE�ļ���̬���ص��ڴ��к�
};

#endif