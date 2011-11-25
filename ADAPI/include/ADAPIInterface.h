#ifndef ADAPIINTERFACE_H
#define ADAPIINTERFACE_H

class ADAPIInterface
{
public:
    virtual bool encode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize ) = 0;
    virtual bool decode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize, unsigned int* pcParsedSize ) = 0;
    virtual bool loadCertificate ( const char* pCertData, int cCertDataSize, void** ppCertContext ) = 0;
    virtual bool unloadCertificate ( const void* pCertContext ) = 0;
    virtual bool loadContext ( const void* pCertContext, void** provContext ) = 0;
    virtual bool unloadContext ( const void* provContext ) = 0;
    virtual bool makeSignature ( const void* provContext, const void* pCertContext, const char* szData, unsigned int cDataSize, char** pszResultData, unsigned int* pcResultSize ) = 0;
    virtual bool getProtocolVersion ( char* szData, int* pcSize ) = 0;
    virtual bool getConnectionType ( char* szData, int* pcSize ) = 0;
    virtual bool freeMemory ( char* pData ) = 0;
};

#endif //ADAPIINTERFACE_H
