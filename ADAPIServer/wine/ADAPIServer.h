#ifndef ADAPISERVERBASE_H
#define ADAPISERVERBASE_H

#include "ADAPIInterface.h"
#include "ADRPC.h"

class ADAPIServerBase : public ADAPIInterface
{
public:
    ADAPIServerBase ( int fd );
    virtual ~ADAPIServerBase ();

    int startServer ();

private:
    ADRPC m_rpc;
};

#endif // ADAPISERVERBASE_H
