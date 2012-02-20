#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>

#include <QCoreApplication>
#include <QProcess>
#include <QDir>

#include "ADRemoteLibrary.h"
#include "ADBootstrap.h"
#include "ADCryptoAPI.h"
#include "ADRPC.h"

#ifndef _LIN_
#error Unsupported platform
#endif

/******************************************************************************/

class WineProcess : public QProcess
{
public:
    WineProcess ( int fd ) :
        m_fd(fd)
    {
#ifdef REDIRECT_STDOUT_STDERR
            setStandardErrorFile( QCoreApplication::applicationDirPath() + "/adapi_server_err" );
            setStandardOutputFile( QCoreApplication::applicationDirPath() + "/adapi_server_out" );
#endif
        }

protected:
    void setupChildProcess ()
    {
            ::dup2(m_fd, STDIN_FILENO);
    }

private:
    int m_fd;
};

/******************************************************************************/

struct RemoteData
{
    RemoteData () :
        wineProcess(0)
    {
        socks[0] = socks[1] = -1;
    }

    void closeSocks ()
    {
        rpc.set_fd(-1);

        if ( socks[0] != -1 ) {
            ::close(socks[0]);
            socks[0] = -1;
        }
        if ( socks[1] != -1 ) {
            ::close(socks[1]);
            socks[1] = -1;
        }
    }

    ADRPC rpc;
    int socks[2];
    WineProcess* wineProcess;
};

/******************************************************************************/

ADRemoteLibrary::ADRemoteLibrary () :
    m_data( new RemoteData )
{}

ADRemoteLibrary::~ADRemoteLibrary ()
{
    unload();
    delete m_data;
}

bool ADRemoteLibrary::load ()
{
    unload();

    int fd = -1;
    int res = -1;
    int flags = 0;
    QString serverBinPath;
    QString bootstrapDir = QString::fromLatin1( ADBootstrap::bootstrapDir() );
    QDir cp( QCoreApplication::applicationDirPath() );

    if ( cp.exists(bootstrapDir + "/ADAPIServer.exe") )
        serverBinPath = cp.absoluteFilePath(bootstrapDir + "/ADAPIServer.exe");
    else
        goto error;

    // It's a good idea to disable SIGPIPE signals; if client closes his end
    // of the pipe/socket, we'd rather see a failure to send a response than
    // get killed by the OS.
    signal(SIGPIPE, SIG_IGN);

    // Create socket
    res = ::socketpair( AF_UNIX, SOCK_STREAM, 0, m_data->socks );
    if ( res < 0 ) {
        goto error;
    }

    // Set close on exec, i.e. prevent inheritance

    fd = m_data->socks[0];
    flags = ::fcntl( fd, F_GETFD, 0 );
    if ( flags < 0 ) {
        goto error;
    }
    else if ( flags & FD_CLOEXEC ) {
    }
    else if ( ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0 ) {
        goto error;
    }

    m_data->wineProcess = new WineProcess( m_data->socks[1] );
    m_data->wineProcess->start( serverBinPath );
    if ( ! m_data->wineProcess->waitForStarted() )
        goto error;

    // Close fd on this side
    ::close( m_data->socks[1] );

    // Set fd
    m_data->rpc.set_fd(fd);

    return true;

 error:
    unload();
    return false;
}

void ADRemoteLibrary::unload ()
{
    m_data->closeSocks();

    if ( m_data->wineProcess ) {
        m_data->wineProcess->terminate();
        m_data->wineProcess->waitForFinished();
        delete m_data->wineProcess;
        m_data->wineProcess = 0;
    }
}

bool ADRemoteLibrary::isLoaded () const
{
    return m_data->wineProcess != 0;
}

bool ADRemoteLibrary::encode ( const char* pEncodingType,
                               unsigned int cEncodingTypeSize,
                               const char* pKey, const char* pData,
                               unsigned int cDataSize, char** pResultData,
                               unsigned int* pcResultSize )
{
    if (!isLoaded())
        return false;

    if (pResultData == 0 || pcResultSize == 0)
        return false;

    std::vector<unsigned char> encType;
    if (pEncodingType && cEncodingTypeSize)
        encType = std::vector<unsigned char>(pEncodingType,
                                             pEncodingType + cEncodingTypeSize);

    std::vector<unsigned char> data;
    if (pData && cDataSize)
        data = std::vector<unsigned char>(pData, pData + cDataSize);

    std::vector<unsigned char> key;
    if (pKey)
        key = std::vector<unsigned char>(pKey, pKey + 16);

    uint32_t resOut = 0;
    std::vector<ADRPC::value*> params;
    params.push_back(ADRPC::value::create_value(encType));
    params.push_back(ADRPC::value::create_value(key));
    params.push_back(ADRPC::value::create_value(data));

    std::vector<ADRPC::value*> retvals;
    if (!m_data->rpc.call("adapi.encode", params, retvals)) {
        qWarning("Call 'adapi.encode' failed");
        goto exit;
    }

    if (retvals.size() != 2) {
        qWarning("Wrong retval size for 'adapi.encode' call");
        goto exit;
    }

    if (!retvals[0]->to_uint32(resOut)) {
        qWarning("Can't parse result for 'adapi.encode' call");
        goto exit;
    }

    if (!retvals[1]->to_bytearray(data)) {
        qWarning("Can't parse data for 'adapi.encode' call");
        goto exit;
    }

    if (data.size() > 0) {
        *pcResultSize = data.size();
        *pResultData = reinterpret_cast<char*>(::malloc(*pcResultSize));
        if (*pResultData == 0)
            return false;
        std::copy(data.begin(), data.end(), *pResultData);
    }

exit:
    ADRPC::free(params);
    ADRPC::free(retvals);
    return !!resOut;
}

bool ADRemoteLibrary::decode ( const char* pEncodingType,
                               unsigned int cEncodingTypeSize,
                               const char* pKey, const char* pData,
                               unsigned int cDataSize, char** pResultData,
                               unsigned int* pcResultSize,
                               unsigned int* pcParsedSize )
{
    if (!isLoaded())
        return false;

    if (pResultData == 0 || pcResultSize == 0 || pcParsedSize == 0)
        return false;

    if (pData == 0 || cDataSize == 0)
        return false;

    std::vector<unsigned char> encType;
    if (pEncodingType && cEncodingTypeSize)
        encType = std::vector<unsigned char>(pEncodingType,
                                             pEncodingType + cEncodingTypeSize);

    std::vector<unsigned char> data;
    if ( pData && cDataSize )
        data = std::vector<unsigned char>(pData, pData + cDataSize);

    std::vector<unsigned char> key;
    if ( pKey )
        key = std::vector<unsigned char>(pKey, pKey + 16);

    uint32_t resOut = 0;
    uint32_t parsedSize = 0;
    std::vector<ADRPC::value*> params;
    params.push_back(ADRPC::value::create_value(encType));
    params.push_back(ADRPC::value::create_value(key));
    params.push_back(ADRPC::value::create_value(data));

    std::vector<ADRPC::value*> retvals;
    if (!m_data->rpc.call("adapi.decode", params, retvals)) {
        qWarning("Call 'adapi.decode' failed");
        goto exit;
    }

    if (retvals.size() != 3) {
        qWarning("Wrong retval size for 'adapi.decode' call");
        goto exit;
    }

    if (!retvals[0]->to_uint32(resOut)) {
        qWarning("Can't parse result for 'adapi.decode' call");
        goto exit;
    }

    if (!retvals[1]->to_bytearray(data)) {
        qWarning("Can't parse data for 'adapi.decode' call");
        goto exit;
    }

    if (!retvals[2]->to_uint32(parsedSize)) {
        qWarning("Can't parse data for 'adapi.decode' call");
        goto exit;
    }

    if (data.size() > 0) {
        *pcResultSize = data.size();
        *pResultData = reinterpret_cast<char*>(::malloc(*pcResultSize));
        if (*pResultData == 0)
            return false;
        std::copy(data.begin(), data.end(), *pResultData);
    }
    *pcParsedSize = parsedSize;

exit:
    ADRPC::free(params);
    ADRPC::free(retvals);
    return !!resOut;
}

bool ADRemoteLibrary::loadCertificate ( const char* pCertData,
                                        int cCertDataSize,
                                        void** ppCertContext )
{
    return ADCryptoAPI::loadCertificate(pCertData, cCertDataSize, ppCertContext);
}

bool ADRemoteLibrary::unloadCertificate ( const void* pCertContext )
{
    return ADCryptoAPI::unloadCertificate(pCertContext);
}

bool ADRemoteLibrary::loadContext ( const void* pCertContext, void** provContext )
{
    return ADCryptoAPI::loadContext(pCertContext, provContext);
}

bool ADRemoteLibrary::unloadContext ( const void* provContext )
{
    return ADCryptoAPI::unloadContext(provContext);
}

bool ADRemoteLibrary::makeSignature ( const void* provContext,
                                      const void* pCertContext,
                                      const char* szData,
                                      unsigned int cDataSize,
                                      char** pszResultData,
                                      unsigned int* pcResultSize )
{
    return ADCryptoAPI::makeSignature(provContext, pCertContext, szData,
                                      cDataSize, pszResultData, pcResultSize);
}

bool ADRemoteLibrary::getProtocolVersion ( char* szData, int* pcSize )
{
    if (!isLoaded())
        return false;

    if (szData == 0 || pcSize == 0)
        return false;

    uint32_t resOut = 0;
    std::vector<unsigned char> data;
    std::vector<ADRPC::value*> params;
    std::vector<ADRPC::value*> retvals;
    if (!m_data->rpc.call("adapi.getProtocolVersion", params, retvals)) {
        qWarning("Call 'adapi.getProtocolVersion' failed");
        goto exit;
    }

    if (retvals.size() != 2) {
        qWarning("Wrong retval size for 'adapi.getProtocolVersion' call");
        goto exit;
    }

    if (!retvals[0]->to_uint32(resOut)) {
        qWarning("Can't parse result for 'adapi.getProtocolVersion' call");
        goto exit;
    }

    if (!retvals[1]->to_bytearray(data)) {
        qWarning("Can't parse data for 'adapi.getProtocolVersion' call");
        goto exit;
    }

    if (data.size() > 0) {
        *pcSize = data.size();
        int sz = ((unsigned int)*pcSize > data.size() ? data.size() : *pcSize);
        std::copy(data.begin(), data.begin() + sz, szData);
    }

exit:
    ADRPC::free(params);
    ADRPC::free(retvals);
    return !!resOut;
}

bool ADRemoteLibrary::getConnectionType ( char* szData, int* pcSize )
{
    if (!isLoaded())
        return false;

    if (szData == 0 || pcSize == 0)
        return false;

    uint32_t resOut = 0;
    std::vector<unsigned char> data;
    std::vector<ADRPC::value*> params;
    std::vector<ADRPC::value*> retvals;
    if (!m_data->rpc.call("adapi.getConnectionType", params, retvals)) {
        qWarning("Call 'adapi.getConnectionType' failed");
        goto exit;
    }

    if (retvals.size() != 2) {
        qWarning("Wrong retval size for 'adapi.getConnectionType' call");
        goto exit;
    }

    if (!retvals[0]->to_uint32(resOut)) {
        qWarning("Can't parse result for 'adapi.getConnectionType' call");
        goto exit;
    }

    if (!retvals[1]->to_bytearray(data)) {
        qWarning("Can't parse data for 'adapi.getConnectionType' call");
        goto exit;
    }

    if (data.size() > 0) {
        *pcSize = data.size();
        int sz = ((unsigned int)*pcSize > data.size() ? data.size() : *pcSize);
        std::copy(data.begin(), data.begin() + sz, szData);
    }

exit:
    ADRPC::free(params);
    ADRPC::free(retvals);
    return !!resOut;
}

bool ADRemoteLibrary::freeMemory ( char* pData )
{
    ::free(pData);
    return true;
}

/******************************************************************************/
