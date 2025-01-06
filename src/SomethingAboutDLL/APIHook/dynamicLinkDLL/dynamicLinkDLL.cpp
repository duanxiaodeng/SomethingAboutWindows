// dynamicLinkDLL.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "dynamicLinkDLL.h"
#include <iostream>

void dynamicLinkDLLFun()
{
	std::cout << "这是原本的dynamicLinkDLLFun函数" << std::endl;
}