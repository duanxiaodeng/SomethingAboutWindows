// noDelayLoadDLL.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "noDelayLoadDLL.h"
#include <iostream>

// ���ǵ���������һ��ʾ��
NODELAYLOADDLL_API int nnoDelayLoadDLL = 100;

void noDelayLoadDLL()
{
	std::cout << "����noDelayLoadDLL����" << std::endl;
}