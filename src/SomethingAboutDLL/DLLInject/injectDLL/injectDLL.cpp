// injectDLL.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "injectDLL.h"


// ���ǵ���������һ��ʾ��
INJECTDLL_API int ninjectDLL=0;

// ���ǵ���������һ��ʾ����
INJECTDLL_API int fninjectDLL(void)
{
    return 42;
}

// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� injectDLL.h
CinjectDLL::CinjectDLL()
{
    return;
}
