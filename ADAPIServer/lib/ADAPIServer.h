#ifndef ADAPISERVERBASE_H
#define ADAPISERVERBASE_H

#include "ADAPIInterface.h"

class ADAPIServerBase : public ADAPIInterface
{
public:
    ADAPIServerBase ( int fd );
    virtual ~ADAPIServerBase ();

    int startServer ();

private:
    int m_fd;
};

#endif // ADAPISERVERBASE_H
