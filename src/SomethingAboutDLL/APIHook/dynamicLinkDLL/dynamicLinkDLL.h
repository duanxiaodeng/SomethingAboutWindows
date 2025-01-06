#pragma once
#ifdef DYNAMICLINKDLL_EXPORTS
#define DYNAMICLINKDLL_API __declspec(dllexport)
#else
#define DYNAMICLINKDLL_API __declspec(dllimport)
#endif

extern "C"
{
	DYNAMICLINKDLL_API void dynamicLinkDLLFun();
}