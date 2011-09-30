#ifndef ADLIBRARY_H
#define ADLIBRARY_H

#include "ADAPIInterface.h"

class ADLibrary : public ADAPIInterface
{
public:
    virtual ~ADLibrary () {}

    virtual bool load () = 0;
    virtual void unload () = 0;
    virtual bool isLoaded () const = 0;
};

#endif //ADLIBRARY_H
