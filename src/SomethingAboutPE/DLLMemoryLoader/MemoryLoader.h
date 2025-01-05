#ifndef _MM_LOAD_DLL_H_
#define _MM_LOAD_DLL_H_

#include <Windows.h>

typedef BOOL(WINAPI* DLLMAIN)(HMODULE, DWORD, LPVOID);

void ShowError(char* lpszText);

// ģ��LoadLibrary�����ڴ�DLL�ļ���������
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// dwSize: �ڴ�DLL�ļ����ڴ��С
// ����ֵ: �ڴ�DLL���ص����̵ļ��ػ�ַ
LPVOID MmLoadLibrary(LPVOID lpData, DWORD dwSize);

// �ж��Ƿ�Ϸ���DLL�ļ�
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// ����ֵ: ��DLL����TRUE�����򷵻�FALSE
BOOL IsValidDLL(LPVOID lpData);

// �ڽ����п���һ���ɶ�����д����ִ�е��ڴ��
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// ����ֵ��������ڴ��ַ
LPVOID AllocVirtualMemory(LPVOID lpData);

// ���ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ���
// lpData: �ڴ�DLL�ļ����ݵĻ�ַ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL MmMapFile(LPVOID lpData, LPVOID lpBaseAddress);

// �޸�PE�ļ��ض�λ����Ϣ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL DoRelocationTable(LPVOID lpBaseAddress);

// ��дPE�ļ��������Ϣ
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL DoImportTable(LPVOID lpBaseAddress);

// �޸�PE�ļ����ػ�ַIMAGE_NT_HEADERS.OptionalHeader.ImageBase
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL SetImageBase(LPVOID lpBaseAddress);

// ����DLL����ں���DllMain,������ַ��ΪPE�ļ�����ڵ�IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL CallDllMain(LPVOID lpBaseAddress, DWORD ul_reason_for_call);

// ģ��GetProcAddress��ȡ�ڴ�DLL�ĵ�������
// lpBaseAddress: �ڴ�DLL�ļ����ص������еļ��ػ�ַ
// lpszFuncName: ��������������
// ����ֵ: ���ص��������ĵĵ�ַ
LPVOID MmGetProcAddress(LPVOID lpBaseAddress, PCHAR lpszFuncName);

// �ͷŴ��ڴ���ص�DLL�������ڴ�Ŀռ�
// lpBaseAddress: �ڴ�DLL���ݰ�SectionAlignment��С����ӳ�䵽�����ڴ��е��ڴ��ַ
// ����ֵ: �ɹ�����TRUE�����򷵻�FALSE
BOOL MmFreeLibrary(LPVOID lpBaseAddress);
#endif