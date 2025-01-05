#ifndef _MM_LOAD_DLL_H_
#define _MM_LOAD_DLL_H_

#include <Windows.h>

typedef BOOL(WINAPI* DLLMAIN)(HMODULE, DWORD, LPVOID);

void ShowError(char* lpszText);

// 模拟LoadLibrary加载内存DLL文件到进程中
// lpData: 内存DLL文件数据的基址
// dwSize: 内存DLL文件的内存大小
// 返回值: 内存DLL加载到进程的加载基址
LPVOID MmLoadLibrary(LPVOID lpData, DWORD dwSize);

// 判断是否合法的DLL文件
// lpData: 内存DLL文件数据的基址
// 返回值: 是DLL返回TRUE，否则返回FALSE
BOOL IsValidDLL(LPVOID lpData);

// 在进程中开辟一个可读、可写、可执行的内存块
// lpData: 内存DLL文件数据的基址
// 返回值：分配的内存基址
LPVOID AllocVirtualMemory(LPVOID lpData);

// 将内存DLL数据按SectionAlignment大小对齐映射到进程内存中
// lpData: 内存DLL文件数据的基址
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL MmMapFile(LPVOID lpData, LPVOID lpBaseAddress);

// 修改PE文件重定位表信息
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL DoRelocationTable(LPVOID lpBaseAddress);

// 填写PE文件导入表信息
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL DoImportTable(LPVOID lpBaseAddress);

// 修改PE文件加载基址IMAGE_NT_HEADERS.OptionalHeader.ImageBase
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL SetImageBase(LPVOID lpBaseAddress);

// 调用DLL的入口函数DllMain,函数地址即为PE文件的入口点IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL CallDllMain(LPVOID lpBaseAddress, DWORD ul_reason_for_call);

// 模拟GetProcAddress获取内存DLL的导出函数
// lpBaseAddress: 内存DLL文件加载到进程中的加载基址
// lpszFuncName: 导出函数的名字
// 返回值: 返回导出函数的的地址
LPVOID MmGetProcAddress(LPVOID lpBaseAddress, PCHAR lpszFuncName);

// 释放从内存加载的DLL到进程内存的空间
// lpBaseAddress: 内存DLL数据按SectionAlignment大小对齐映射到进程内存中的内存基址
// 返回值: 成功返回TRUE，否则返回FALSE
BOOL MmFreeLibrary(LPVOID lpBaseAddress);
#endif