// noDelayLoadDLL.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "noDelayLoadDLL.h"
#include <iostream>

// 这是导出变量的一个示例
NODELAYLOADDLL_API int nnoDelayLoadDLL = 100;

void noDelayLoadDLL()
{
	std::cout << "这是noDelayLoadDLL函数" << std::endl;
}