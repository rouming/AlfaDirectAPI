#include <unistd.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/server_pstream.hpp>

#include <stdio.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <signal.h>

#include "ADAPIServer.h"

/******************************************************************************/

class EncodeMethod : public xmlrpc_c::method
{
private:
    ADAPIServerBase* m_server;

public:
    EncodeMethod ( ADAPIServerBase* server ) :
        m_server(server)
    {
        // S{b:result, 6:data}:666
        _signature = "S:666";
    }

    void execute ( xmlrpc_c::paramList const& paramList,
                   xmlrpc_c::value *   const  retvalP )
    {
        std::vector<unsigned char> encType = paramList.getBytestring(0);
        unsigned int encSize = encType.size();
        std::vector<unsigned char> key = paramList.getBytestring(1);
        std::vector<unsigned char> data = paramList.getBytestring(2);
        unsigned int dataSize = data.size();
        char* pResultData = 0;
        unsigned int cResultSize = 0;
        paramList.verifyEnd(3);

        bool res = m_server->encode( (encType.size() > 0 ? reinterpret_cast<const char*>(&encType[0]) : 0),
                                     encSize,
                                     (key.size() > 0 ? reinterpret_cast<const char*>(&key[0]) : 0),
                                     (data.size() > 0 ? reinterpret_cast<const char*>(&data[0]) : 0),
                                     dataSize,
                                     &pResultData, &cResultSize);

        if ( pResultData && cResultSize )
            data = std::vector<unsigned char>(pResultData, pResultData + cResultSize);
        else
            data.clear();

        m_server->freeMemory( pResultData );

        std::map<std::string, xmlrpc_c::value> retStruct;
        retStruct["result"] = xmlrpc_c::value_boolean(res);
        retStruct["data"] = xmlrpc_c::value_bytestring(data);

        *retvalP = xmlrpc_c::value_struct(retStruct);
    }
};

/******************************************************************************/

class DecodeMethod : public xmlrpc_c::method
{
private:
    ADAPIServerBase* m_server;

public:
    DecodeMethod ( ADAPIServerBase* server ) :
        m_server(server)
    {
        // S{b:result, 6:data, i:parsed_size}:666
        _signature = "S:666";
    }

    void execute ( xmlrpc_c::paramList const& paramList,
                   xmlrpc_c::value *   const  retvalP )
    {
        std::vector<unsigned char> encType = paramList.getBytestring(0);
        unsigned int encSize = encType.size();
        std::vector<unsigned char> key = paramList.getBytestring(1);
        std::vector<unsigned char> data = paramList.getBytestring(2);
        unsigned int dataSize = data.size();
        char* pResultData = 0;
        unsigned int cResultSize = 0;
        unsigned int cParsedSize = 0;
        paramList.verifyEnd(3);

        bool res = m_server->decode( (encType.size() > 0 ? reinterpret_cast<const char*>(&encType[0]) : 0),
                                     encSize,
                                     (key.size() > 0 ? reinterpret_cast<const char*>(&key[0]) : 0),
                                     (data.size() > 0 ? reinterpret_cast<const char*>(&data[0]) : 0),
                                     dataSize,
                                     &pResultData, &cResultSize, &cParsedSize);

        if ( pResultData && cResultSize )
            data = std::vector<unsigned char>(pResultData, pResultData + cResultSize);
        else
            data.clear();

        m_server->freeMemory( pResultData );

        std::map<std::string, xmlrpc_c::value> retStruct;
        retStruct["result"] = xmlrpc_c::value_boolean(res);
        retStruct["data"] = xmlrpc_c::value_bytestring(data);
        retStruct["parsed_size"] = xmlrpc_c::value_int(cParsedSize);

        *retvalP = xmlrpc_c::value_struct(retStruct);
    }
};

/******************************************************************************/

class GetProtocolVersionMethod : public xmlrpc_c::method
{
private:
    ADAPIServerBase* m_server;

public:
    GetProtocolVersionMethod ( ADAPIServerBase* server ) :
        m_server(server)
    {
        // S{b:result, 6:data}:
        _signature = "S:";
    }

    void execute ( xmlrpc_c::paramList const&,
                   xmlrpc_c::value *   const  retvalP )
    {
        char protoVer[ 256 ] = {0};
        int size = sizeof(protoVer);

        bool res = m_server->getProtocolVersion( protoVer, &size );

        std::vector<unsigned char> data;
        if ( res && size )
            data = std::vector<unsigned char>( reinterpret_cast<unsigned char*>(protoVer),
                                               reinterpret_cast<unsigned char*>(protoVer + size) );

        std::map<std::string, xmlrpc_c::value> retStruct;
        retStruct["result"] = xmlrpc_c::value_boolean(res);
        retStruct["data"] = xmlrpc_c::value_bytestring(data);

        *retvalP = xmlrpc_c::value_struct(retStruct);
    }
};

/******************************************************************************/

class GetConnectionTypeMethod : public xmlrpc_c::method
{
private:
    ADAPIServerBase* m_server;

public:
    GetConnectionTypeMethod ( ADAPIServerBase* server ) :
        m_server(server)
    {
        // S{b:result, 6:data}:
        _signature = "S:";
    }

    void execute ( xmlrpc_c::paramList const&,
                   xmlrpc_c::value *   const  retvalP )
    {
        char connType[ 256 ] = {0};
        int size = sizeof(connType);

        bool res = m_server->getConnectionType( connType, &size );

        std::vector<unsigned char> data;
        if ( res && size )
            data = std::vector<unsigned char>( reinterpret_cast<unsigned char*>(connType),
                                               reinterpret_cast<unsigned char*>(connType + size) );


        std::map<std::string, xmlrpc_c::value> retStruct;
        retStruct["result"] = xmlrpc_c::value_boolean(res);
        retStruct["data"] = xmlrpc_c::value_bytestring(data);

        *retvalP = xmlrpc_c::value_struct(retStruct);
    }
};

/******************************************************************************/

ADAPIServerBase::ADAPIServerBase ( int fd ) :
    m_fd(fd)
{}

ADAPIServerBase::~ADAPIServerBase ()
{}

int ADAPIServerBase::startServer ()
{
    // It's a good idea to disable SIGPIPE signals; if client closes his end
    // of the pipe/socket, we'd rather see a failure to send a response than
    // get killed by the OS.
    signal(SIGPIPE, SIG_IGN);

    try {
        // Set XML size to max
        xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, UINT_MAX);

        xmlrpc_c::registry myRegistry;

        xmlrpc_c::methodPtr const encodeMethodP(new EncodeMethod(this));
        xmlrpc_c::methodPtr const decodeMethodP(new DecodeMethod(this));
        xmlrpc_c::methodPtr const getProtoVersionMethodP(new GetProtocolVersionMethod(this));
        xmlrpc_c::methodPtr const getConnTypeMethodP(new GetConnectionTypeMethod(this));

        myRegistry.addMethod("adapi.encode", encodeMethodP);
        myRegistry.addMethod("adapi.decode", decodeMethodP);
        myRegistry.addMethod("adapi.getProtocolVersion", getProtoVersionMethodP);
        myRegistry.addMethod("adapi.getConnectionType", getConnTypeMethodP);

        xmlrpc_c::serverPstreamConn server(
            xmlrpc_c::serverPstreamConn::constrOpt()
            .socketFd(m_fd)
            .registryP(&myRegistry));

        while ( 1 ) {
            bool eofP;
            server.runOnce(&eofP);
        }

    } catch (std::exception const& e) {
        std::cerr << "Something threw an error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

/******************************************************************************/
