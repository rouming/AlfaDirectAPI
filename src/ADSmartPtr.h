#ifndef SMARTPTR_H
#define SMARTPTR_H

#include <new>

#include "ADAtomicOps.h"

template <class T>
class ADSmartPtr
{
public:
    enum StoragePolicy
    {
        Array,        // Free with 'delete []'
        DoNotRelease  // Do not release
    };

    typedef T* PointerType;   // type returned by operator->
    typedef T& ReferenceType; // type returned by operator*

private:
    typedef void (*FreeFunc) ( PointerType ); // free function

    static FreeFunc selectFreeFunc ( ADSmartPtr::StoragePolicy storagePolicy )
    {
        switch ( storagePolicy )
        {
        case ADSmartPtr::Array:
            return deletePointedArray;

        case ADSmartPtr::DoNotRelease:
        default:
            return 0;
        }
    }

    static void deletePointer ( PointerType pointer )
    {
        delete pointer;
    }

    static void deletePointedArray ( PointerType pointer )
    {
        delete[] pointer;
    }

    template <class P>
    class Storage
    {
    public:
        Storage ( PointerType pointer, FreeFunc free ) :
            m_ref(1),
            m_pointer(pointer),
            m_free(free)
        {
        }

        ~Storage ()
        {
            if ( m_free )
                m_free( m_pointer );
        }

        void incRef ()
        {
            atomic_inc32( &m_ref );
        }

        unsigned int decRef ()
        {
            return atomic_dec32( &m_ref ) - 1;
        }

        unsigned int countRefs () const
        {
            return m_ref;
        }

        PointerType pointer () const
        {
            return m_pointer;
        }

    private:
        Storage ( const Storage & );
        Storage& operator= ( const Storage & );

    private:
        volatile atomic32_t m_ref;
        PointerType m_pointer;
        FreeFunc m_free;
    };

public:
    ADSmartPtr () :
        m_storage(0)
    {}

    explicit ADSmartPtr ( PointerType pointer ) :
        m_storage(allocStorage(pointer, deletePointer))
    {}

    ADSmartPtr ( PointerType pointer,
                 ADSmartPtr::StoragePolicy storagePolicy ) :
        m_storage(allocStorage(pointer,
                               selectFreeFunc(storagePolicy)))
    {}

    template <typename R, typename A>
    ADSmartPtr ( PointerType pointer, R(*free)(A) ) :
        m_storage(allocStorage(pointer,
                               reinterpret_cast<FreeFunc>(free)))
    {}

    ADSmartPtr ( const ADSmartPtr<T>& copy ) :
        m_storage(copy.m_storage)
    {
        if ( m_storage )
            m_storage->incRef();
    }

    ~ADSmartPtr ()
    {
        freeStorage( m_storage );
    }

    const ADSmartPtr& operator= ( const ADSmartPtr<T>& copy )
    {
        Storage<T>* copyStorage = copy.m_storage;
        if ( copyStorage == m_storage )
            return *this;
        if ( copyStorage )
            copyStorage->incRef();

        freeStorage( m_storage );
        m_storage = copyStorage;

        return *this;
    }

    bool operator == ( const ADSmartPtr<T>& ptr ) const
    {
        return m_storage == ptr.m_storage;
    }

    bool operator != ( const ADSmartPtr<T>& ptr ) const
    {
        return m_storage != ptr.m_storage;
    }

    PointerType getImpl () const
    {
        return m_storage != 0 ? m_storage->pointer(): 0;
    }

    PointerType operator-> () const
    {
        return getImpl();
    }

    ReferenceType operator* () const
    {
        return *getImpl();
    }

    unsigned int countRefs () const
    {
        return m_storage != 0 ? m_storage->countRefs(): 0;
    }

    bool isValid () const
    {
        return m_storage && m_storage->pointer();
    }

    operator bool () const
    {
        return isValid();
    }

private:
    static Storage<T>* allocStorage ( const PointerType pointer,
                                      const FreeFunc freeFunc )
    {
        try { return new Storage<T>( pointer, freeFunc ); }
        catch ( const std::bad_alloc & )
        {
            if ( freeFunc )
                freeFunc( pointer );
        }
        return 0;
    }

    static void freeStorage ( Storage<T>* storage )
    {
        if ( storage && 0 == storage->decRef() )
            delete storage;
    }

    Storage<T>* m_storage;
};

#endif //SMARTPTR_H
