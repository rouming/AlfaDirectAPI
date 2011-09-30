#ifndef ADDYNALOADER_H
#define ADDYNALOADER_H

#include <string>

class ADDynaLoader
{
public:
    ADDynaLoader ( const std::string& libName );
    ~ADDynaLoader ();

    bool isLoaded () const;
    bool load ();
    void unload ();

    void* resolve ( const std::string& funcName );

private:
    ADDynaLoader ( const ADDynaLoader& );
    ADDynaLoader& operator= ( const ADDynaLoader& );

    struct LoaderData* m_data;
    std::string m_libName;
};

#endif //ADDYNALOADER_H
