#pragma once
#ifdef IMPLICITLINKDLL_EXPORTS
#define IMPLICITLINKDLL_API __declspec(dllexport)
#else
#define IMPLICITLINKDLL_API __declspec(dllimport)
#endif

extern "C"
{
	IMPLICITLINKDLL_API void implicitLinkDLLFun();
}
