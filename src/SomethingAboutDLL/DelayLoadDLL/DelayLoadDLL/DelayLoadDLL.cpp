// DelayLoadDLL.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "DelayLoadDLL.h"
#include <iostream>

void delayFun1(int num)
{
	std::cout << "这是delayFun1：" << num << std::endl;
}

void delayFun2(std::string msg)
{
	std::cout << "这是delayFun2：" << msg << std::endl;
}
