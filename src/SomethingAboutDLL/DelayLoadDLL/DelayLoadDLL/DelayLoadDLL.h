#pragma once

#include <string>

#ifdef DELAYLOADDLL_EXPORTS
#define DELAYLOADDLL_API __declspec(dllexport)
#else
#define DELAYLOADDLL_API __declspec(dllimport)
#endif

extern "C"
{
	DELAYLOADDLL_API void delayFun1(int num);
	DELAYLOADDLL_API void delayFun2(std::string msg);
}