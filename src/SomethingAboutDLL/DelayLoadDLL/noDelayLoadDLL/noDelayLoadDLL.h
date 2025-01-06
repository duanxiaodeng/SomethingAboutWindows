#ifdef NODELAYLOADDLL_EXPORTS
#define NODELAYLOADDLL_API __declspec(dllexport)
#else
#define NODELAYLOADDLL_API __declspec(dllimport)
#endif

extern "C"
{
	NODELAYLOADDLL_API void noDelayLoadDLL();

	extern NODELAYLOADDLL_API int nnoDelayLoadDLL;
}

