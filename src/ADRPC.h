#ifndef _ADRPC_
#define _ADRPC_

#include <unistd.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <string>

class ADRPC
{
public:
    struct value
    {
    public:
        bool to_bytearray(std::vector<unsigned char>&) const;
        bool to_bytearray_ptr(const unsigned char*&, uint32_t&) const;
        bool to_uint32(uint32_t&) const;

        static value* create_value(const std::vector<unsigned char>&);
        static value* create_value(uint32_t);

    private:
        friend class ADRPC;

        // Supported types of values
        enum type
        {
            BYTEARRAY = 0,
            UINT32    = 1
        };
        type type;
        uint16_t size;
        unsigned char val[2];
    };

    typedef void (*adrpc_call) (void* param,
                                const std::vector<ADRPC::value*>&,
                                std::vector<ADRPC::value*>& retvals);

    // Default ctr
    ADRPC();

    // Set sock descriptor
    void set_fd(int sock);

    // Register callback by method name
    bool register_call(const std::string& method, adrpc_call cb, void* param);

    // Enters main server loop
    bool exec_loop();

    // Calls method
    bool call(const std::string& method,
              const std::vector<ADRPC::value*>& params,
              std::vector<ADRPC::value*>& retvals);

    // Frees vector of values
    static void free(std::vector<ADRPC::value*>&);

private:
    bool read(void* b, uint32_t size);
    bool read(std::string& method_str,
              std::vector<ADRPC::value*>& vals);
    bool send(const void* b, uint32_t size);
    bool send(const std::string& method,
              const std::vector<ADRPC::value*>& vals);

private:
    struct rpc_header
    {
        uint16_t method_size;
        uint16_t values_num;
        // method name will follow
        // values will follow
    };

    std::map<std::string, std::pair<adrpc_call, void*> > m_calls;
    int m_sock;
};

#endif //_ADRPC_
