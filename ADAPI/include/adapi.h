// ****************************************************************************
// adapi.h
// ****************************************************************************

#pragma once

#ifndef ADSDK_PREFIX
#if __GNUC__
  #define ADSDK_PREFIX extern "C"
#else
  #define ADSDK_PREFIX extern "C" __declspec(dllimport)
#endif
#endif

#define ADSDK_ERROR_FLAG 0xE0000000
#define ADSDK_ALLOC_UNKNOWN (ADSDK_ERROR_FLAG | 1)
#define ADSDK_FREE_UNKNOWN (ADSDK_ERROR_FLAG | 2)
#define ADSDK_COPY_UNKNOWN (ADSDK_ERROR_FLAG | 3)
#define ADSDK_DECODE_ERROR (ADSDK_ERROR_FLAG | 4)
#define ADSDK_INNER_ERROR (ADSDK_ERROR_FLAG | 5)
#define ADSDK_CERT_EXTENTION (ADSDK_ERROR_FLAG | 6)
#define ADSDK_SIGN_UNKNOWN (ADSDK_ERROR_FLAG | 7)
#define ADSDK_FATAL_ERROR (ADSDK_ERROR_FLAG | 8)

ADSDK_PREFIX BOOL WINAPI Encode(const BYTE* pEncodingType, const SIZE_T cEncodingTypeSize, const BYTE* pKey, const BYTE* pData, const SIZE_T cDataSize, BYTE** pResultData, SIZE_T* pcResultSize);
ADSDK_PREFIX BOOL WINAPI Decode(const BYTE* pEncodingType, const SIZE_T cEncodingTypeSize, const BYTE* pKey, BYTE* pData, const SIZE_T cDataSize, BYTE** pResultData, SIZE_T* pcResultSize, SIZE_T* pcParsedSize);
ADSDK_PREFIX BOOL WINAPI LoadCertificate(const BYTE* pCertData, const int cCertDataSize, void** ppCertContext);
ADSDK_PREFIX BOOL WINAPI UnloadCertificate(const void* pCertContext);
ADSDK_PREFIX BOOL WINAPI LoadContext(const void* pCertContext, unsigned int* provContext);
ADSDK_PREFIX BOOL WINAPI UnloadContext(unsigned int provContext);
ADSDK_PREFIX BOOL WINAPI MakeSignature(unsigned int provContext, const void* pCertContext, const BYTE* szData, const SIZE_T cDataSize, BYTE** pszResultData, SIZE_T* pcResultSize);
ADSDK_PREFIX BOOL WINAPI GetProtocolVersion(BYTE* szData, int* pcSize);
ADSDK_PREFIX BOOL WINAPI GetConnectionType(BYTE* szData, int* pcSize);
ADSDK_PREFIX BOOL WINAPI FreeMemory(BYTE* pData);
