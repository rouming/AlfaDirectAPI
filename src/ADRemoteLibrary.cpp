#ifndef _WIN_
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client.hpp>
#include <xmlrpc-c/client_transport.hpp>

#include <QCoreApplication>
#include <QProcess>
#include <QDir>

#include "ADRemoteLibrary.h"
#include "ADBootstrap.h"

/******************************************************************************/

#ifndef _WIN_ // Unix

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

#endif

/******************************************************************************/

struct RemoteData
{
    RemoteData () :
        transport(0),
        client(0),
        wineProcess(0)
    {
        socks[0] = socks[1] = -1;
    }

    xmlrpc_c::clientXmlTransport_pstream* transport;
    xmlrpc_c::client_xml* client;

#ifndef _WIN_
    void closeSocks ()
    {
        if ( socks[0] != -1 ) {
            ::close(socks[0]);
            socks[0] = -1;
        }
        if ( socks[1] != -1 ) {
            ::close(socks[1]);
            socks[1] = -1;
        }
    }

    int socks[2];
    WineProcess* wineProcess;
#endif
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

#ifndef _WIN_ //Unix
    int res = -1;
    int flags = 0;
    QString serverBinPath;
    QString bootstrapDir = QString::fromLatin1( ADBootstrap::bootstrapDir() );
    QDir cp( QCoreApplication::applicationDirPath() );

    if ( cp.exists(bootstrapDir + "/ADAPIServer.exe") )
        serverBinPath = cp.absoluteFilePath(bootstrapDir + "/ADAPIServer.exe");
    else
        goto error;

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

#else
#error Unsupported platform
#endif

    try {
        // Set XML size to max
        xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, UINT_MAX);

        m_data->transport = new xmlrpc_c::clientXmlTransport_pstream(
            xmlrpc_c::clientXmlTransport_pstream::constrOpt().fd(fd));

        m_data->client = new xmlrpc_c::client_xml(m_data->transport);

        return true;
    }
    catch ( ... ) {}

 error:
    unload();
    return false;
}

void ADRemoteLibrary::unload ()
{
    delete m_data->client;
    delete m_data->transport;

    m_data->client = 0;
    m_data->transport = 0;

#ifndef _WIN_
    m_data->closeSocks();

    if ( m_data->wineProcess ) {
        m_data->wineProcess->terminate();
        m_data->wineProcess->waitForFinished();
        delete m_data->wineProcess;
        m_data->wineProcess = 0;
    }
#endif
}

bool ADRemoteLibrary::isLoaded () const
{
    return m_data->client != 0;
}

bool ADRemoteLibrary::encode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize )
{
    if ( ! isLoaded() )
        return false;

    if ( pResultData == 0 || pcResultSize == 0 )
        return false;

    try {
        const std::string methodName("adapi.encode");

        std::vector<unsigned char> encType;
        if ( pEncodingType && cEncodingTypeSize )
            encType = std::vector<unsigned char>(pEncodingType, pEncodingType + cEncodingTypeSize);

        std::vector<unsigned char> data;
        if ( pData && cDataSize )
            data = std::vector<unsigned char>(pData, pData + cDataSize);

        std::vector<unsigned char> key;
        if ( pKey )
            key = std::vector<unsigned char>(pKey, pKey + 16);

        xmlrpc_c::paramList parms;
        parms.add(xmlrpc_c::value_bytestring(encType));
        parms.add(xmlrpc_c::value_bytestring(key));
        parms.add(xmlrpc_c::value_bytestring(data));

        xmlrpc_c::rpcPtr rpc(methodName, parms);
        xmlrpc_c::carriageParm_pstream carriageParm;

        rpc->call(m_data->client, &carriageParm);
        assert(rpc->isFinished());

        std::map<std::string, xmlrpc_c::value> res = xmlrpc_c::value_struct( rpc->getResult() );

        std::map<std::string, xmlrpc_c::value>::iterator it;

        bool resOut = false;

        if ( (it = res.find("result")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BOOLEAN )
            resOut = xmlrpc_c::value_boolean(it->second);
        else
            return false;

        if ( (it = res.find("data")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BYTESTRING )
            data = xmlrpc_c::value_bytestring(it->second).vectorUcharValue();
        else
            return false;

        if ( data.size() > 0 ) {
            *pcResultSize = data.size();
            *pResultData = reinterpret_cast<char*>(::malloc(*pcResultSize));
            if ( *pResultData == 0 )
                return false;
            std::copy( data.begin(), data.end(), *pResultData );
        }

        return resOut;
    }
    catch ( std::exception& e ) {
        qWarning("Encode exception: %s", e.what());
        return false;
    }
    catch ( ... ) {
        qWarning("Encode generic exception!");
        return false;
    }
}

bool ADRemoteLibrary::decode ( const char* pEncodingType, unsigned int cEncodingTypeSize, const char* pKey, const char* pData, unsigned int cDataSize, char** pResultData, unsigned int* pcResultSize, unsigned int* pcParsedSize )
{
    if ( ! isLoaded() )
        return false;

    if ( pResultData == 0 || pcResultSize == 0 || pcParsedSize == 0 )
        return false;

    if ( pData == 0 || cDataSize == 0 )
        return false;

    try {
        const std::string methodName("adapi.decode");

        std::vector<unsigned char> encType;
        if ( pEncodingType && cEncodingTypeSize )
            encType = std::vector<unsigned char>(pEncodingType, pEncodingType + cEncodingTypeSize);

        std::vector<unsigned char> key;
        if ( pKey )
            key = std::vector<unsigned char>(pKey, pKey + 16);

        unsigned int parsedSize = 0;
        std::vector<unsigned char> data = std::vector<unsigned char>(pData, pData + cDataSize);

        xmlrpc_c::paramList parms;
        parms.add(xmlrpc_c::value_bytestring(encType));
        parms.add(xmlrpc_c::value_bytestring(key));
        parms.add(xmlrpc_c::value_bytestring(data));

        xmlrpc_c::rpcPtr rpc(methodName, parms);
        xmlrpc_c::carriageParm_pstream carriageParm;

        rpc->call(m_data->client, &carriageParm);
        assert(rpc->isFinished());

        std::map<std::string, xmlrpc_c::value> res = xmlrpc_c::value_struct( rpc->getResult() );
        std::map<std::string, xmlrpc_c::value>::iterator it;

        bool resOut = false;

        if ( (it = res.find("result")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BOOLEAN )
            resOut = xmlrpc_c::value_boolean(it->second);
        else
            return false;

        if ( ! resOut )
            return false;

        if ( (it = res.find("data")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BYTESTRING )
            data = xmlrpc_c::value_bytestring(it->second).vectorUcharValue();
        else
            return false;

        if ( (it = res.find("parsed_size")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_INT )
            parsedSize = xmlrpc_c::value_int(it->second);
        else
            return false;

        //XXX
        if ( data.size() >= XMLRPC_XML_SIZE_LIMIT_DEFAULT )
            printf("!!!!!!!!!!!! >>>>>>>>  enc=%d, dec=%lu, parsed=%d\n", cDataSize, data.size(), parsedSize);

        //XXX
        if ( 0 )
        {
            printf("parsed=%d, key=", parsedSize);
            for ( unsigned int i = 0; i < 16; ++i ) {
                printf("%02x", pKey[i] & 0xff);
            }
            printf(", encrypted=");
            for ( unsigned int i = 0; i < parsedSize; ++i ) {
                printf("%02x", pData[i] & 0xff);
            }
            printf(", decrypted=");
            for ( unsigned int i = 0; i < data.size(); ++i ) {
                printf("%c", data[i] & 0xff);
            }
            printf("\n");

        }

        if ( data.size() > 0 ) {
            *pcResultSize = data.size();
            *pResultData = reinterpret_cast<char*>(::malloc(*pcResultSize));
            if ( *pResultData == 0 )
                return false;
            std::copy( data.begin(), data.end(), *pResultData );
        }
        *pcParsedSize = parsedSize;

        return true;
    }
    catch ( std::exception& e ) {
        qWarning("Decode exception: %s", e.what());
        return false;
    }
    catch ( ... ) {
        qWarning("Decode generic exception!");
        return false;
    }
}

bool ADRemoteLibrary::loadCertificate ( const char* pCertData, int cCertDataSize, void** ppCertContext )
{
    (void)pCertData;
    (void)cCertDataSize;
    if ( ppCertContext )
        *reinterpret_cast<char**>(ppCertContext) = (char*)0xDEADBEAF;
    return true;
}

bool ADRemoteLibrary::unloadCertificate ( const void* pCertContext )
{
    (void)pCertContext;
    return true;
}

bool ADRemoteLibrary::loadContext ( const void* pCertContext, unsigned int* provContext )
{
    (void)pCertContext;
    if ( provContext )
        *provContext = 0xDEADBEAF;
    return true;
}

bool ADRemoteLibrary::unloadContext ( unsigned int provContext )
{
    (void)provContext;
    return true;
}

bool ADRemoteLibrary::makeSignature ( unsigned int provContext, const void* pCertContext, const char* szData, unsigned int cDataSize, char** pszResultData, unsigned int* pcResultSize )
{
    (void)provContext;
    (void)pCertContext;
    (void)szData;
    (void)cDataSize;
    (void)pszResultData;
    (void)pcResultSize;
    return false;
}

bool ADRemoteLibrary::getProtocolVersion ( char* szData, int* pcSize )
{
    if ( ! isLoaded() )
        return false;

    if ( szData == 0 || pcSize == 0 )
        return false;

    try {
        const std::string methodName("adapi.getProtocolVersion");

        xmlrpc_c::paramList parms;
        xmlrpc_c::rpcPtr rpc(methodName, parms);
        xmlrpc_c::carriageParm_pstream carriageParm;

        rpc->call(m_data->client, &carriageParm);
        assert(rpc->isFinished());

        std::map<std::string, xmlrpc_c::value> res = xmlrpc_c::value_struct( rpc->getResult() );

        std::map<std::string, xmlrpc_c::value>::iterator it;

        bool resOut = false;
        std::vector<unsigned char> data;

        if ( (it = res.find("result")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BOOLEAN )
            resOut = xmlrpc_c::value_boolean(it->second);
        else
            return false;

        if ( (it = res.find("data")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BYTESTRING )
            data = xmlrpc_c::value_bytestring(it->second).vectorUcharValue();
        else
            return false;

        if ( data.size() > 0 ) {
            *pcSize = data.size();
            int sz = ((unsigned int)*pcSize > data.size() ? data.size() : *pcSize);
            std::copy( data.begin(), data.begin() + sz, szData );
        }

        return resOut;
    }
    catch ( std::exception& e ) {
        qWarning("getProtocolVersion exception: %s", e.what());
        return false;
    }
    catch ( ... ) {
        qWarning("getProtocolVersion generic exception!");
        return false;
    }
}

bool ADRemoteLibrary::getConnectionType ( char* szData, int* pcSize )
{
    if ( ! isLoaded() )
        return false;

    if ( szData == 0 || pcSize == 0 )
        return false;

    try {
        const std::string methodName("adapi.getConnectionType");

        xmlrpc_c::paramList parms;
        xmlrpc_c::rpcPtr rpc(methodName, parms);
        xmlrpc_c::carriageParm_pstream carriageParm;

        rpc->call(m_data->client, &carriageParm);
        assert(rpc->isFinished());

        std::map<std::string, xmlrpc_c::value> res = xmlrpc_c::value_struct( rpc->getResult() );

        std::map<std::string, xmlrpc_c::value>::iterator it;

        bool resOut = false;
        std::vector<unsigned char> data;

        if ( (it = res.find("result")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BOOLEAN )
            resOut = xmlrpc_c::value_boolean(it->second);
        else
            return false;

        if ( (it = res.find("data")) != res.end() &&
             it->second.type() == xmlrpc_c::value::TYPE_BYTESTRING )
            data = xmlrpc_c::value_bytestring(it->second).vectorUcharValue();
        else
            return false;

        if ( data.size() > 0 ) {
            *pcSize = data.size();
            int sz = ((unsigned int)*pcSize > data.size() ? data.size() : *pcSize);
            std::copy( data.begin(), data.begin() + sz, szData );
        }

        return resOut;
    }
    catch ( std::exception& e ) {
        qWarning("getConnectionType exception: %s", e.what());
        return false;
    }
    catch ( ... ) {
        qWarning("getConnectionType generic exception!");
        return false;
    }
}

bool ADRemoteLibrary::freeMemory ( char* pData )
{
    ::free(pData);
    return true;
}

/******************************************************************************/
