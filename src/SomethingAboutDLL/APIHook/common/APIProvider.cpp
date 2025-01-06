#include "APIProvider.h"
#include <iostream>

void CAPIProvider::newImplicitLinkDLLFun()
{
	std::cout << "这是新的implicitLinkDLLFun函数" << std::endl;
}

void CAPIProvider::newDynamicLinkDLLFun()
{
	std::cout << "这是新的dynamicLinkDLLFun函数" << std::endl;
}
