#include <Windows.h>
#include <string>
#include "RunPEInMemory.h"

int main(int argc, char** argv)
{
	std::string exePath = "G:\\main.exe";
	CRunPE runPE(exePath);
	runPE.runPE();
	return 0;
}