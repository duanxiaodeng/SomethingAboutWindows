//-------------------------------------------------------------------
// Copyright (C) Microsoft.  All rights reserved.
// Example of verifying the embedded signature of a PE file by using 
// the WinVerifyTrust function.

#define _UNICODE 1
#define UNICODE 1

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#pragma comment (lib, "wintrust") // wintrust.lib

// 验证嵌入式签名
BOOL VerifyEmbeddedSignature(LPCWSTR pwszSourceFile)
{
    // 1. 初始化WINTRUST_FILE_INFO结构
    WINTRUST_FILE_INFO FileData;
    memset(&FileData, 0, sizeof(FileData));
    FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    FileData.pcwszFilePath = pwszSourceFile; // 要验证的文件全路径
    FileData.hFile = NULL;
    FileData.pgKnownSubject = NULL;
    
    
    // 2. 初始化WINTRUST_DATA结构
    WINTRUST_DATA WinTrustData;
    memset(&WinTrustData, 0, sizeof(WinTrustData));
    WinTrustData.cbStruct = sizeof(WinTrustData);
    WinTrustData.pPolicyCallbackData = NULL;              // Use default code signing EKU.
    WinTrustData.pSIPClientData = NULL;                   // 没有数据传递给SIP
    WinTrustData.dwUIChoice = WTD_UI_NONE;                // 禁用UI，即：签名验证时不提示用户
    WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;   // No revocation checking.
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;         // 指定要使用的union成员是pFile
    WinTrustData.pFile = &FileData;                       // 设置union中的pFile
    WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;  // 验签dwUnionChoice成员指定的对象
    WinTrustData.hWVTStateData = NULL;                    // Verification sets this value.
    WinTrustData.pwszURLReference = NULL;                 // 保留参数，设置为NULL
    WinTrustData.dwUIContext = WTD_UICONTEXT_EXECUTE;     // 表示为要运行的文件调用WinVerifyTrust时
    
    // 3. 指定使用WINTRUST_ACTION_GENERIC_VERIFY_V2，即：验证文件或对象
    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2; 

    // 4. 调用WinVerifyTrust验证签名
    LONG lStatus = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
    DWORD dwLastError;
    switch (lStatus)
    {
    case ERROR_SUCCESS: // 验证成功
        /*
        Signed file:
            - Hash that represents the subject is trusted.
            - Trusted publisher without any verification errors.
            - UI was disabled in dwUIChoice. No publisher or time stamp chain errors.
            - UI was enabled in dwUIChoice and the user clicked "Yes" when asked to install and run the signed subject.
        */
        wprintf_s(L"The file \"%s\" is signed and the signature was verified.\n", pwszSourceFile);
        break;

    case TRUST_E_NOSIGNATURE: // 此文件未被签名或签名无效
        dwLastError = GetLastError();
        if (TRUST_E_NOSIGNATURE == dwLastError || TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError || TRUST_E_PROVIDER_UNKNOWN == dwLastError)
            wprintf_s(L"The file \"%s\" is not signed.\n", pwszSourceFile); // 文件未签名
        else
            wprintf_s(L"An unknown error occurred trying to verify the signature of the \"%s\" file.\n", pwszSourceFile); // 签名无效或打开文件有未知错误
        break;

    case TRUST_E_EXPLICIT_DISTRUST: // 传入文件经过有效签名和身份验证，但签名者或当前用户已禁用该签名，因此无效
        wprintf_s(L"The signature is present, but specifically "
            L"disallowed.\n");
        break;

    case TRUST_E_SUBJECT_NOT_TRUSTED: // 当有用户交互时，用户选择了No
        wprintf_s(L"The signature is present, but not trusted.\n");
        break;

    case CRYPT_E_SECURITY_SETTINGS: // 签名证书已被管理员禁用，指纹计算结果与当前传入文件不匹配，或者时间戳异常
        wprintf_s(L"CRYPT_E_SECURITY_SETTINGS - The hash "
            L"representing the subject or the publisher wasn't "
            L"explicitly trusted by the admin and admin policy "
            L"has disabled user trust. No signature, publisher "
            L"or timestamp errors.\n");
        break;

    default:
        // The UI was disabled in dwUIChoice or the admin policy 
        // has disabled user trust. lStatus contains the 
        // publisher or time stamp chain error.
        wprintf_s(L"Error is: 0x%x.\n", lStatus);
        break;
    }

    // 使用WTD_STATEACTION_CLOSE选项释放之前使用WTD_STATEACTION_VERIFY选项分配的hWVTStateData成员
    WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    lStatus = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

    return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc > 1)
        VerifyEmbeddedSignature(argv[1]);

    return 0;
}


