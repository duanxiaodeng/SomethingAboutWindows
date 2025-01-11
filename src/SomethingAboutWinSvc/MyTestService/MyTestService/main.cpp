#include <iostream>
#include "MyTestServiceBase.h"
#include "MyTestServiceController.h"
#include "MyTestService.h"

#define MYTESTSERIVCE_CONTROL(COMMAND, COMMANDINPUT)						\
if (lstrcmpiA(COMMANDINPUT, (#COMMAND)) == 0)							\
{																		\
	char str[256];														\
	if (!CMyTestServiceController::##COMMAND##Service())					\
	{																	\
		sprintf(str, "Failed to %s MyTestService\n",  #COMMAND);			\
		OutputDebugStringA(str);										\
		return -1;														\
	}																	\
	else																\
	{																	\
		sprintf(str, "Success to %s MyTestService\n",  #COMMAND);			\
		OutputDebugStringA(str);										\
		return 0;														\
	}																	\
}																		\

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		MYTESTSERIVCE_CONTROL(install, argv[1])
		MYTESTSERIVCE_CONTROL(uninstall, argv[1])
	}

	CMyTestServiceBase* pSvc = CMyTestService::getInstance();
	pSvc->run(argc, argv);

	return 0;
}