#if defined _WIN32 || defined WIN32
#include <windows.h>
typedef HMODULE LibraryHandle;
typedef FARPROC ProcAddress;
#define dl_open(filename) (LibraryHandle)LoadLibraryA(filename)
#define dl_sym(handler, funcname) (void*)GetProcAddress(handler, funcname)
#define dl_close(handler) FreeLibrary(handler)
#else
#include <dlfcn.h>
typedef void* LibraryHandle;
typedef void* ProcAddress;
#define dl_open(filename) dlopen(filename, RTLD_NOW | RTLD_GLOBAL)
#define dl_sym(handler, funcname) dlsym(handler, funcname)
#define dl_close(handler) dlclose(handler)
#endif

#include "ADDynaLoader.h"

/******************************************************************************/

struct LoaderData
{
    LibraryHandle handle;
    LoaderData () : handle(0) {}
};

/******************************************************************************/

ADDynaLoader::ADDynaLoader ( const std::string& libName ) :
    m_data( new LoaderData ),
    m_libName(libName)
{}

ADDynaLoader::~ADDynaLoader ()
{
    unload();
    delete m_data;
}

bool ADDynaLoader::isLoaded () const
{
    return m_data->handle != 0;
}

bool ADDynaLoader::load ()
{
    unload();

#ifdef _WIN_
    std::string libExtension = ".dll";
#else
    std::string libExtension = ".so";
#endif

    std::string lib = m_libName + libExtension;

    m_data->handle = dl_open(lib.c_str());

    return isLoaded();
}

void ADDynaLoader::unload ()
{
    if ( isLoaded() )
        dl_close(m_data->handle);
}

void* ADDynaLoader::resolve ( const std::string& funcName )
{
    if ( ! isLoaded() )
        return 0;
    else
        return dl_sym(m_data->handle, funcName.c_str());
}

/******************************************************************************/
