#include <windows.h>
#include <assert.h>

#include "ADLocalLibrary.h"

#ifndef _WINE_ // Pure WIN
  #include <QString>
  #include "ADBootstrap.h"
#endif

/****************************************************************************/

struct ADLibrarySyms
{
    typedef BOOL (WINAPI *Encode) (const BYTE* pEncodingType, const SIZE_T cEncodingTypeSize, const BYTE* pKey, const BYTE* pData, const SIZE_T cDataSize, BYTE** pResultData, SIZE_T* pcResultSize);
    typedef BOOL (WINAPI *Decode) (const BYTE* pEncodingType, const SIZE_T cEncodingTypeSize, const BYTE* pKey, BYTE* pData, const SIZE_T cDataSize, BYTE** pResultData, SIZE_T* pcResultSize, SIZE_T* pcParsedSize);
    typedef BOOL (WINAPI *LoadCertificate) (const BYTE* pCertData, const int cCertDataSize, void** ppCertContext);
    typedef BOOL (WINAPI *UnloadCertificate) (const void* pCertContext);
    typedef BOOL (WINAPI *LoadContext) (const void* pCertContext, unsigned int* provContext);
    typedef BOOL (WINAPI *UnloadContext) (unsigned int provContext);
    typedef BOOL (WINAPI *MakeSignature) (unsigned int provContext, const void* pCertContext, const BYTE* szData, const SIZE_T cDataSize, BYTE** pszResultData, SIZE_T* pcResultSize);
    typedef BOOL (WINAPI *GetProtocolVersion) (BYTE* szData, int* pcSize);
    typedef BOOL (WINAPI *GetConnectionType) (BYTE* szData, int* pcSize);
    typedef BOOL (WINAPI *FreeMemory) (BYTE* pData);

    Encode encode;
    Decode decode;
    LoadCertificate loadCertificate;
    UnloadCertificate unloadCertificate;
    LoadContext loadContext;
    UnloadContext unloadContext;
    MakeSignature makeSignature;
    GetProtocolVersion getProtocolVersion;
    GetConnectionType getConnectionType;
    FreeMemory freeMemory;
};

/****************************************************************************/

ADLocalLibrary::ADLocalLibrary () :
#ifndef _WINE_ // Pure WIN
    m_adLib( QString(QString::fromLatin1(ADBootstrap::bootstrapDir()) + "/ADAPI").toStdString() ),
#else // Wine
    m_adLib("ADAPI"),
#endif
    m_syms(new ADLibrarySyms)
{
    ::memset(m_syms, 0, sizeof(*m_syms));
}

ADLocalLibrary::~ADLocalLibrary ()
{
    unload();
    delete m_syms;
}

bool ADLocalLibrary::load ()
{
    if ( m_adLib.isLoaded() )
        return true;
    bool res = m_adLib.load();
    if ( ! res )
        return res;

    void* encodeSym = 0;
    void* decodeSym = 0;
    void* loadCertificateSym = 0;
    void* unloadCertificateSym = 0;
    void* loadContextSym = 0;
    void* unloadContextSym = 0;
    void* makeSignatureSym = 0;
    void* getProtocolVersionSym = 0;
    void* getConnectionTypeSym = 0;
    void* freeMemorySym = 0;

    //
    // Resolve symbols
    //

    encodeSym = m_adLib.resolve( "Encode" );
    if ( encodeSym == 0 )
        goto error;

    decodeSym= m_adLib.resolve( "Decode" );
    if ( decodeSym == 0 )
        goto error;

    loadCertificateSym = m_adLib.resolve( "LoadCertificate" );
    if ( loadCertificateSym == 0 )
        goto error;

    unloadCertificateSym = m_adLib.resolve( "UnloadCertificate" );
    if ( unloadCertificateSym == 0 )
        goto error;

    loadContextSym = m_adLib.resolve( "LoadContext" );
    if ( loadContextSym == 0 )
        goto error;

    unloadContextSym = m_adLib.resolve( "UnloadContext" );
    if ( unloadContextSym == 0 )
        goto error;

    makeSignatureSym = m_adLib.resolve( "MakeSignature" );
    if ( makeSignatureSym == 0 )
        goto error;

    getProtocolVersionSym = m_adLib.resolve( "GetProtocolVersion" );
    if ( getProtocolVersionSym == 0 )
        goto error;

    getConnectionTypeSym = m_adLib.resolve( "GetConnectionType" );
    if ( getConnectionTypeSym == 0 )
        goto error;

    freeMemorySym = m_adLib.resolve( "FreeMemory" );
    if ( freeMemorySym == 0 )
        goto error;

    //
    // Setup symbols
    //

    m_syms->encode =
        reinterpret_cast<ADLibrarySyms::Encode>(encodeSym);
    m_syms->decode =
        reinterpret_cast<ADLibrarySyms::Decode>(decodeSym);
    m_syms->loadCertificate =
        reinterpret_cast<ADLibrarySyms::LoadCertificate>(loadCertificateSym);
    m_syms->unloadCertificate =
        reinterpret_cast<ADLibrarySyms::UnloadCertificate>(unloadCertificateSym);
    m_syms->loadContext =
        reinterpret_cast<ADLibrarySyms::LoadContext>(loadContextSym);
    m_syms->unloadContext =
        reinterpret_cast<ADLibrarySyms::UnloadContext>(unloadContextSym);
    m_syms->makeSignature =
        reinterpret_cast<ADLibrarySyms::MakeSignature>(makeSignatureSym);
    m_syms->getProtocolVersion =
        reinterpret_cast<ADLibrarySyms::GetProtocolVersion>(getProtocolVersionSym);
    m_syms->getConnectionType =
        reinterpret_cast<ADLibrarySyms::GetConnectionType>(getConnectionTypeSym);
    m_syms->freeMemory =
        reinterpret_cast<ADLibrarySyms::FreeMemory>(freeMemorySym);

    return true;

 error:
    m_adLib.unload();
    return false;
}

void ADLocalLibrary::unload ()
{
    if ( m_adLib.isLoaded() )
        m_adLib.unload();
}

bool ADLocalLibrary::isLoaded () const
{
    return m_adLib.isLoaded();
}

bool ADLocalLibrary::encode ( const char* encType,
                         unsigned int encTypeSz,
                         const char* key,
                         const char* data,
                         unsigned int dataSz,
                         char** resData,
                         unsigned int* resSz )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->encode);
    return !!m_syms->encode( reinterpret_cast<const BYTE*>(encType),
                             encTypeSz,
                             reinterpret_cast<const BYTE*>(key),
                             reinterpret_cast<const BYTE*>(data),
                             dataSz,
                             reinterpret_cast<BYTE**>(resData),
                             reinterpret_cast<SIZE_T*>(resSz) );
}

bool ADLocalLibrary::decode ( const char* encType,
                         unsigned int encTypeSz,
                         const char* key,
                         const char* data,
                         unsigned int dataSz,
                         char** resData,
                         unsigned int* resSz,
                         unsigned int* parsedSz )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    //WTF?
    char* encData = const_cast<char*>(data);

    assert(m_syms->decode);
    return !!m_syms->decode( reinterpret_cast<const BYTE*>(encType),
                             encTypeSz,
                             reinterpret_cast<const BYTE*>(key),
                             reinterpret_cast<BYTE*>(encData),
                             dataSz,
                             reinterpret_cast<BYTE**>(resData),
                             reinterpret_cast<SIZE_T*>(resSz),
                             reinterpret_cast<SIZE_T*>(parsedSz) );
}

bool ADLocalLibrary::loadCertificate ( const char* certData,
                                  int certDataSz,
                                  void** certCtx )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->loadCertificate);
    return !!m_syms->loadCertificate( reinterpret_cast<const BYTE*>(certData),
                                      certDataSz, certCtx );
}

bool ADLocalLibrary::unloadCertificate ( const void* certCtx )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->unloadCertificate);
    return !!m_syms->unloadCertificate( certCtx );
}

bool ADLocalLibrary::loadContext ( const void* certCtx, unsigned int* provCtx )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->loadContext);
    return !!m_syms->loadContext( certCtx, provCtx );
}

bool ADLocalLibrary::unloadContext ( unsigned int provCtx )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->unloadContext);
    return !!m_syms->unloadContext( provCtx );
}

bool ADLocalLibrary::makeSignature ( unsigned int provCtx,
                                const void* certCtx,
                                const char* data,
                                unsigned int dataSz,
                                char** resData,
                                unsigned int* resSize )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->makeSignature);
    return !!m_syms->makeSignature( provCtx, certCtx,
                                    reinterpret_cast<const BYTE*>(data),
                                    dataSz,
                                    reinterpret_cast<BYTE**>(resData),
                                    reinterpret_cast<SIZE_T*>(resSize) );
}

bool ADLocalLibrary::getProtocolVersion ( char* data, int* size )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->getProtocolVersion);
    return !!m_syms->getProtocolVersion( reinterpret_cast<BYTE*>(data), size );
}

bool ADLocalLibrary::getConnectionType ( char* data, int* size )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->getConnectionType);
    return !!m_syms->getConnectionType( reinterpret_cast<BYTE*>(data), size );
}

bool ADLocalLibrary::freeMemory ( char* data )
{
    if ( ! m_adLib.isLoaded() )
        return false;

    assert(m_syms->freeMemory);
    return !!m_syms->freeMemory( reinterpret_cast<BYTE*>(data) );
}

/****************************************************************************/
