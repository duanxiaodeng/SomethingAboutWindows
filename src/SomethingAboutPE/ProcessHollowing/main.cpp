#include <stdio.h>
#include <iostream>
#include <windows.h>
#pragma warning (disable : 4996)

/*
  说明：
  ① 本文只能用于32位的PE文件，因为后续代码中的CONTEXT使用的x86的CONTEXT结构
*/

std::string getModuleDir()
{
	std::string path;
	char modulePath[MAX_PATH];
	if (GetModuleFileNameA(NULL, modulePath, MAX_PATH))
	{
		path = modulePath;
	}

	std::string directory;
	std::string filename = path;
	const size_t last_slash_idx = filename.rfind('\\');
	if (std::string::npos != last_slash_idx)
	{
		directory = filename.substr(0, last_slash_idx);
		return directory;
	}

	return "";
}

BYTE* MapFileToMemory(LPCSTR filename, LONGLONG& filelen)
{
	FILE* fileptr;
	BYTE* buffer;

	fileptr = fopen(filename, "rb");  // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
	filelen = ftell(fileptr);             // Get the current byte offset in the file
	rewind(fileptr);                      // Jump back to the beginning of the file

	buffer = (BYTE*)malloc((filelen + 1) * sizeof(char)); // Enough memory for file + \0
	fread(buffer, filelen, 1, fileptr); // Read in the entire file
	fclose(fileptr); // Close the file

	return buffer;
}

void RunPortableExecutable(const char* path, void* Image)
{
	PROCESS_INFORMATION PI = {};
	STARTUPINFOA SI = {};

	void* pImageBase; // Pointer to the image base
	IMAGE_NT_HEADERS* NtHeader = PIMAGE_NT_HEADERS((size_t)Image + PIMAGE_DOS_HEADER(Image)->e_lfanew); // NT头
	IMAGE_SECTION_HEADER* SectionHeader = PIMAGE_SECTION_HEADER((size_t)NtHeader + sizeof(*NtHeader)); // Section header

	// 创建指定的进程，但是处于挂起(suspended)状态，新进程的主线程尚未执行
	if (CreateProcessA(path, 0, 0, 0, false, CREATE_SUSPENDED, 0, 0, &SI, &PI))
	{
		// Allocate memory for the context.
		CONTEXT* threadContext = LPCONTEXT(VirtualAlloc(NULL, sizeof(threadContext), MEM_COMMIT, PAGE_READWRITE));
		threadContext->ContextFlags = CONTEXT_FULL; // Context

		if (GetThreadContext(PI.hThread, LPCONTEXT(threadContext))) // 获取新进程中主线程的CONTEXT结构
		{
			pImageBase = VirtualAllocEx(PI.hProcess, LPVOID(NtHeader->OptionalHeader.ImageBase),
				NtHeader->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

			// PE文件动态映射
			WriteProcessMemory(PI.hProcess, pImageBase, Image, NtHeader->OptionalHeader.SizeOfHeaders, NULL);
			for (int i = 0; i < NtHeader->FileHeader.NumberOfSections; i++)
				WriteProcessMemory
				(
					PI.hProcess,
					LPVOID((size_t)pImageBase + SectionHeader[i].VirtualAddress),
					LPVOID((size_t)Image + SectionHeader[i].PointerToRawData),
					SectionHeader[i].SizeOfRawData,
					0
				);

			// Ebx：基址寄存器
			WriteProcessMemory(PI.hProcess, LPVOID(threadContext->Ebx + 8), LPVOID(&pImageBase), sizeof(pImageBase), 0);
			// Eax：累加寄存器
			threadContext->Eax = DWORD(pImageBase) + NtHeader->OptionalHeader.AddressOfEntryPoint;
			SetThreadContext(PI.hThread, LPCONTEXT(threadContext));
			ResumeThread(PI.hThread);
		}
	}
}

int main()
{
	// 要注入执行的代码内容
	char CurrentFilePath[MAX_PATH];
	GetModuleFileNameA(0, CurrentFilePath, MAX_PATH);
	if (strstr(CurrentFilePath, "testExe.exe"))
	{
		MessageBoxA(NULL, "This is my process", "Inject", MB_OK);
		return 0;
	}

	// 创建进程并注入
	std::string exePath = getModuleDir() + "\\testExe.exe";
	LONGLONG len = -1;
	RunPortableExecutable(exePath.c_str(), MapFileToMemory(CurrentFilePath, len));
	system("pause");
	return 0;
}