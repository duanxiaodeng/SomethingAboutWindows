// DelayLoadDLL.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "DelayLoadDLL.h"
#include <iostream>

void delayFun1(int num)
{
	std::cout << "����delayFun1��" << num << std::endl;
}

void delayFun2(std::string msg)
{
	std::cout << "����delayFun2��" << msg << std::endl;
}
