#include <cassert>
#include <signal.h>

#include "ADAPIServer.h"

/******************************************************************************/

// Params:  bytestring,bytestring,bytestring
// Retvals: int,bytestring
void ADRPC_Encode(void* param,
                  const std::vector<ADRPC::value*>& params,
                  std::vector<ADRPC::value*>& retval)
{
    assert(params.size() == 3);
    ADAPIServerBase* server = (ADAPIServerBase*)param;
    const unsigned char* encType;
    unsigned int encSize;
    params[0]->to_bytearray_ptr(encType, encSize);

    const unsigned char* key;
    unsigned int keySize;
    params[1]->to_bytearray_ptr(key, keySize);

    const unsigned char* data;
    unsigned int dataSize;
    params[2]->to_bytearray_ptr(data, dataSize);

    char* pResultData = 0;
    unsigned int cResultSize = 0;

    bool res = server->encode(
        (encSize > 0 ? (const char*)encType : 0),
        encSize,
        (keySize > 0 ? (const char*)key : 0),
        (dataSize > 0 ? (const char*)data : 0),
        dataSize,
        &pResultData, &cResultSize);

    std::vector<unsigned char> resData;
    if (pResultData && cResultSize)
        resData = std::vector<unsigned char>(pResultData, pResultData + cResultSize);

    server->freeMemory(pResultData);

    // Create retval vector
    retval.push_back(ADRPC::value::create_value((uint32_t)res));
    retval.push_back(ADRPC::value::create_value(resData));
}

// Params:  bytestring,bytestring,bytestring
// Retvals: int,bytestring,int
void ADRPC_Decode(void* param,
                  const std::vector<ADRPC::value*>& params,
                  std::vector<ADRPC::value*>& retval)
{
    assert(params.size() == 3);
    ADAPIServerBase* server = (ADAPIServerBase*)param;
    const unsigned char* encType;
    unsigned int encSize;
    params[0]->to_bytearray_ptr(encType, encSize);

    const unsigned char* key;
    unsigned int keySize;
    params[1]->to_bytearray_ptr(key, keySize);

    const unsigned char* data;
    unsigned int dataSize;
    params[2]->to_bytearray_ptr(data, dataSize);

    char* pResultData = 0;
    unsigned int cResultSize = 0;
    unsigned int cParsedSize = 0;

    bool res = server->decode(
        (encSize > 0 ? (const char*)encType : 0),
        encSize,
        (keySize > 0 ? (const char*)key : 0),
        (dataSize > 0 ? (const char*)data : 0),
        dataSize,
        &pResultData, &cResultSize, &cParsedSize);

    std::vector<unsigned char> resData;
    if (pResultData && cResultSize)
        resData = std::vector<unsigned char>(pResultData, pResultData + cResultSize);

    server->freeMemory(pResultData);

    // Create retval vector
    retval.push_back(ADRPC::value::create_value((uint32_t)res));
    retval.push_back(ADRPC::value::create_value(resData));
    retval.push_back(ADRPC::value::create_value((uint32_t)cParsedSize));
}

// Params:  void
// Retvals: int,bytestring
void ADRPC_GetProtocolVersion(void* param,
                              const std::vector<ADRPC::value*>& params,
                              std::vector<ADRPC::value*>& retval)
{
    assert(params.size() == 0);
    ADAPIServerBase* server = (ADAPIServerBase*)param;
    char protoVer[ 256 ] = {0};
    int size = sizeof(protoVer);

    bool res = server->getProtocolVersion( protoVer, &size );

    std::vector<unsigned char> data;
    if ( res && size )
        data = std::vector<unsigned char>((unsigned char*)protoVer,
                                          (unsigned char*)protoVer + size);

    // Create retval vector
    retval.push_back(ADRPC::value::create_value((uint32_t)res));
    retval.push_back(ADRPC::value::create_value(data));
}

// Params:  void
// Retvals: int,bytestring
void ADRPC_GetConnectionType(void* param,
                             const std::vector<ADRPC::value*>& params,
                             std::vector<ADRPC::value*>& retval)
{
    assert(params.size() == 0);
    ADAPIServerBase* server = (ADAPIServerBase*)param;
    char connType[ 256 ] = {0};
    int size = sizeof(connType);

    bool res = server->getConnectionType( connType, &size );

    std::vector<unsigned char> data;
    if ( res && size )
        data = std::vector<unsigned char>((unsigned char*)connType,
                                          (unsigned char*)connType + size);

    // Create retval vector
    retval.push_back(ADRPC::value::create_value((uint32_t)res));
    retval.push_back(ADRPC::value::create_value(data));
}

/******************************************************************************/

ADAPIServerBase::ADAPIServerBase ( int fd )
{
    m_rpc.set_fd(fd);
}

ADAPIServerBase::~ADAPIServerBase ()
{}

int ADAPIServerBase::startServer ()
{
    // It's a good idea to disable SIGPIPE signals; if client closes his end
    // of the pipe/socket, we'd rather see a failure to send a response than
    // get killed by the OS.
    signal(SIGPIPE, SIG_IGN);

    // Register rpc calls
    m_rpc.register_call("adapi.encode", ADRPC_Encode, this);
    m_rpc.register_call("adapi.decode", ADRPC_Decode, this);
    m_rpc.register_call("adapi.getProtocolVersion", ADRPC_GetProtocolVersion, this);
    m_rpc.register_call("adapi.getConnectionType", ADRPC_GetConnectionType, this);

    // Enters loop
    m_rpc.exec_loop();

    return 0;
}

/******************************************************************************/
