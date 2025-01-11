#include <iostream>
#include "MyTestServiceBase.h"
#include "MyTestServiceInstaller.h"
#include "MyTestService.h"

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		// 安装卸载服务
		if (lstrcmpiA(argv[1], "-install") == 0)
		{
			CMyTestServiceInstaller::installService();
		}
		else if (lstrcmpiA(argv[1], "-uninstall") == 0)
		{
			CMyTestServiceInstaller::uninstallService();
		}
	}

	// 服务运行
	CMyTestServiceBase* pSvc = CMyTestService::getInstance();
	pSvc->run(argc, argv);

	return 0;
}