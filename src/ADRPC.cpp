#include "ADRPC.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

/******************************************************************************/

bool ADRPC::value::to_bytearray(std::vector<unsigned char>& out) const
{
    if (type != BYTEARRAY)
        return false;
    out = std::vector<unsigned char>((const unsigned char*)val,
                                     (const unsigned char*)val + size);
    return true;
}

bool ADRPC::value::to_bytearray_ptr(const unsigned char*& ptr, uint32_t& sz) const
{
    if (type != BYTEARRAY)
        return false;
    ptr = (const unsigned char*)val;
    sz = size;
    return true;
}

bool ADRPC::value::to_uint32(uint32_t& out) const
{
    if (type != UINT32 || size != 4)
        return false;
    uint32_t* ptr = (uint32_t*)val;
    out = *ptr;
    out = ntohl(out);
    return true;
}

ADRPC::value* ADRPC::value::create_value(const std::vector<unsigned char>& in)
{
    // Allocate mem for header and for string
    value* val = (value*)::malloc(sizeof(ADRPC::value) + in.size() - 2);
    if (!val)
        return 0;
    val->type = BYTEARRAY;
    val->size = in.size();
    if (val->size)
        ::memcpy(val->val, in.data(), in.size());

    return val;
}

ADRPC::value* ADRPC::value::create_value(uint32_t in)
{
    // Allocate mem for header and for string
    value* val = (value*)::malloc(sizeof(ADRPC::value) + sizeof(in) - 2);
    if (!val)
        return 0;
    val->type = UINT32;
    val->size = sizeof(in);
    in = htonl(in);
    ::memcpy(val->val, &in, sizeof(in));

    return val;
}

/******************************************************************************/

ADRPC::ADRPC() :
    m_sock(-1)
{}

void ADRPC::set_fd(int sock)
{
    m_sock = sock;
}

bool ADRPC::register_call(const std::string& method,
                          ADRPC::adrpc_call cb,
                          void* param)
{
    if (cb == 0 || m_calls.find(method) != m_calls.end())
        return false;
    m_calls.insert(std::pair<std::string, std::pair<adrpc_call, void*> >(
                       method, std::pair<adrpc_call, void*>(cb, param)));
    return true;
}

bool ADRPC::read(void* b, uint32_t size)
{
    char* buff = (char*)b;
    ssize_t rb  = 0;
again:
    rb = ::read(m_sock, buff, size);
    if (rb == 0) {
        // gracefull shutdown
        return false;
    }
    else if (rb < 0 && errno == EINTR) {
        // Read has been interrupted
        goto again;
    }
    else if (rb < 0) {
        // Read failed
        return false;
    }

    // Iterate
    buff += rb;
    size -= rb;

    if (size != 0)
        goto again;

    return true;
}

bool ADRPC::read(std::string& method_str,
                 std::vector<ADRPC::value*>& vals)
{
    ADRPC::rpc_header header;
    char* method = 0;
    bool res = false;

    // Read header
    if (!read(&header, sizeof(header)))
        goto exit;;
    assert(header.method_size != 0);

    // Read method name
    method = (char*)::malloc(header.method_size);
    if (!method)
        goto exit;
    if (!read(method, header.method_size))
        goto exit;

    // Set out method string
    method_str = std::string(method, header.method_size);

    // Read params in loop
    for (uint32_t i = 0; i < header.values_num; ++i) {
        ADRPC::value val_header;

        // Read value header
        if (!read(&val_header, sizeof(val_header) - 2))
            goto exit;

        // Allocate value
        ADRPC::value* val = (ADRPC::value*)::malloc(sizeof(ADRPC::value) +
                                                    val_header.size - 2);
        if (!val)
            goto exit;

        // Copy real header
        ::memcpy(val, &val_header, sizeof(ADRPC::value) - 2);

        // Add to params vector
        vals.push_back(val);

        // Read value data
        if (val->size && !read(val->val, val->size))
            goto exit;
    }

    // Success
    res = true;

exit:
    ::free(method);
    if (!res)
        free(vals);
    return res;
}

bool ADRPC::send(const void* b, uint32_t size)
{
    const char* buff = (const char*)b;
    ssize_t wb  = 0;
again:
    wb = ::write(m_sock, buff, size);
    if (wb == 0) {
        // Peer was closed
        return false;
    }
    else if (wb < 0 && errno == EINTR) {
        // Write has been interrupted
        goto again;
    }
    else if (wb < 0) {
        // Write failed
        return false;
    }

    // Iterate
    buff += wb;
    size -= wb;

    if (size != 0)
        goto again;

    return true;
}

bool ADRPC::send(const std::string& method,
                 const std::vector<ADRPC::value*>& vals)
{
    if (method.length() == 0)
        return false;

    bool res = false;
    ADRPC::rpc_header header = {method.length(), vals.size()};

    // Send header
    if (!send(&header, sizeof(header)))
        goto exit;

    // Send method
    if (!send(method.data(), method.length()))
        goto exit;

    // Send params in loop
    for (std::vector<ADRPC::value*>::const_iterator it = vals.begin();
         it != vals.end(); ++it) {
        const ADRPC::value* val = *it;
        if (!send((void*)val, sizeof(*val) + val->size - 2))
            goto exit;
    }

    // Success
    res = true;

exit:
    return res;
}

bool ADRPC::exec_loop()
{
    while (1) {
        // Vector for in/out values
        std::vector<ADRPC::value*> params;
        std::vector<ADRPC::value*> retvals;
        std::map<std::string, std::pair<adrpc_call, void*> >::iterator it;

        // Read call with params
        std::string method;
        if (!read(method, params))
            goto exit;

        // Check if method was registered
        if ((it = m_calls.find(method)) == m_calls.end())
            goto exit;

        // Do call
        it->second.first(it->second.second, params, retvals);

        // Send retvals
        if (!send(method, retvals))
            goto exit;

        // Free values vector
        free(params);
        free(retvals);

        continue;

    exit:
        // Free vectors
        free(params);
        free(retvals);
        return false;
    }
}

bool ADRPC::call(const std::string& method,
                 const std::vector<ADRPC::value*>& params,
                 std::vector<ADRPC::value*>& retvals)
{
    // Send request
    if (!send(method, params))
        return false;
    // Read retvals
    std::string method_ret;
    if (!read(method_ret, retvals))
        return false;
    assert(method == method_ret);
    return true;
}

void ADRPC::free(std::vector<ADRPC::value*>& vec)
{
    std::vector<ADRPC::value*>::iterator it = vec.begin();
    while (it != vec.end()) {
        ::free(*it);
        it = vec.erase(it);
    }
}

/******************************************************************************/
