#include <stdio.h>

#include "ADAPIServer.h"
#include "ADLocalLibrary.h"

/*****************************************************************************/

class ADAPIServer : public ADAPIServerBase
{
    ADLocalLibrary& m_lib;

public:
    ADAPIServer ( int fd, ADLocalLibrary& lib ) :
        ADAPIServerBase(fd),
        m_lib(lib)
    {}

    virtual bool encode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize )
    {
        return m_lib.encode(pEncodingType, cEncodingTypeSize, pKey, pData, cDataSize, pResultData, pcResultSize);
    }

    virtual bool decode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize, unsigned int* pcParsedSize )
    {
        return m_lib.decode(pEncodingType, cEncodingTypeSize, pKey, pData, cDataSize, pResultData, pcResultSize, pcParsedSize);
    }

    virtual bool loadCertificate ( const char* pCertData, int cCertDataSize, void** ppCertContext )
    {
        return m_lib.loadCertificate(pCertData, cCertDataSize, ppCertContext);
    }

    virtual bool unloadCertificate ( const void* pCertContext )
    {
        return m_lib.unloadCertificate(pCertContext);
    }

    virtual bool loadContext ( const void* pCertContext, void** provContext )
    {
        return m_lib.loadContext(pCertContext, provContext);
    }

    virtual bool unloadContext ( const void* provContext )
    {
        return m_lib.unloadContext(provContext);
    }

    virtual bool makeSignature ( const void* provContext, const void* pCertContext, const char* szData, unsigned int cDataSize, char** pszResultData, unsigned int* pcResultSize )
    {
        return m_lib.makeSignature(provContext, pCertContext, szData, cDataSize, pszResultData, pcResultSize);
    }

    virtual bool getProtocolVersion ( char* szData, int* pcSize )
    {
        return m_lib.getProtocolVersion(szData, pcSize);
    }

    virtual bool getConnectionType ( char* szData, int* pcSize )
    {
        return m_lib.getConnectionType(szData, pcSize);
    }

    virtual bool freeMemory ( char* pData )
    {
        return m_lib.freeMemory(pData);
    }

};

/*****************************************************************************/

int main (int, char**)
{
    ADLocalLibrary lib;
    bool loaded = lib.load();
    if ( ! loaded ) {
        printf("Can't load ADAPI lib!\n");
        return -1;
    }

    ADAPIServer server(STDIN_FILENO, lib);
    return server.startServer();
}

/*****************************************************************************/
