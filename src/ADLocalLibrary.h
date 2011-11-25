#ifndef ADLOCALLIBRARY_H
#define ADLOCALLIBRARY_H

#include "ADLibrary.h"
#include "ADDynaLoader.h"

class ADLocalLibrary : public ADLibrary
{
public:
    ADLocalLibrary ();
    virtual ~ADLocalLibrary ();

    virtual bool load ();
    virtual void unload ();
    virtual bool isLoaded () const;

    virtual bool encode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize );
    virtual bool decode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize, unsigned int* pcParsedSize );
    virtual bool loadCertificate ( const char* pCertData, int cCertDataSize, void** ppCertContext );
    virtual bool unloadCertificate ( const void* pCertContext );
    virtual bool loadContext ( const void* pCertContext, void** provContext );
    virtual bool unloadContext ( const void* provContext );
    virtual bool makeSignature ( const void* provContext, const void* pCertContext, const char* szData, unsigned int cDataSize, char** pszResultData, unsigned int* pcResultSize );
    virtual bool getProtocolVersion ( char* szData, int* pcSize );
    virtual bool getConnectionType ( char* szData, int* pcSize );
    virtual bool freeMemory ( char* pData );

private:
    ADDynaLoader m_adLib;
    struct ADLibrarySyms* m_syms;
};

#endif //ADLOCALLIBRARY_H
