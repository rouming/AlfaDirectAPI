#ifndef ATOMICOPS_H
#define ATOMICOPS_H

/*
 * GCC
 */
#if defined(__GNUC__)
#undef __inline
#define __inline static __always_inline
#define __stdcall

typedef unsigned int atomic32_t;
typedef unsigned long long atomic64_t;

__inline atomic64_t __stdcall atomic_read64 ( const volatile atomic64_t* src )
{
    return __sync_fetch_and_add((volatile atomic64_t*)src, 0);
}

__inline void __stdcall atomic_write64 ( volatile atomic64_t* dst, atomic64_t val )
{
    __sync_lock_test_and_set(dst, val);
}

__inline atomic64_t __stdcall atomic_add64 ( volatile atomic64_t* dst, atomic64_t val )
{
    return __sync_fetch_and_add(dst, val);
}

__inline atomic64_t __stdcall atomic_inc64 ( volatile atomic64_t* dst )
{
    return __sync_fetch_and_add(dst, 1);
}

__inline atomic64_t __stdcall atomic_dec64 ( volatile atomic64_t* dst )
{
    return __sync_fetch_and_sub(dst, 1);
}

__inline atomic64_t __stdcall atomic_swap64 ( volatile atomic64_t* dst, atomic64_t val )
{
    return  __sync_lock_test_and_set(dst, val);
}

__inline atomic32_t __stdcall atomic_read32 ( const volatile atomic32_t* src )
{
    return __sync_fetch_and_add((volatile atomic32_t*)src, 0);
}

__inline void __stdcall atomic_write32 ( volatile atomic32_t* dst, atomic32_t val )
{
    __sync_lock_test_and_set(dst, val);
}

__inline atomic32_t __stdcall atomic_add32 ( volatile atomic32_t* dst, atomic32_t val )
{
    return __sync_fetch_and_add(dst, val);
}

__inline atomic32_t __stdcall atomic_inc32 ( volatile atomic32_t* dst )
{
    return __sync_fetch_and_add(dst, 1);
}

__inline atomic32_t __stdcall atomic_dec32 ( volatile atomic32_t* dst )
{
    return __sync_fetch_and_sub(dst, 1);
}

__inline atomic32_t __stdcall atomic_swap32 ( volatile atomic32_t* dst, atomic32_t val )
{
    return  __sync_lock_test_and_set(dst, val);
}

#undef __inline

/*
 * Windows
 */
#elif defined(_MSC_VER)

#ifndef WINBASEAPI
#include <intrin.h>
#endif

#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedIncrement64)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedDecrement64)

typedef unsigned int atomic32_t;
typedef unsigned __int64 atomic64_t;

__inline atomic64_t __stdcall atomic_read64 ( const volatile atomic64_t* src )
{
    return (atomic64_t)_InterlockedCompareExchange64((volatile atomic64_t*)src, 0, 0);
}

__inline void __stdcall atomic_write64 ( volatile atomic64_t* dst, atomic64_t val )
{
    _InterlockedExchange64(dst, write);
}

__inline atomic64_t __stdcall atomic_add64 ( volatile atomic64_t* dst, atomic64_t val )
{
    return (atomic64_t)_InterlockedExchangeAdd64(dst, val);
}

__inline atomic64_t __stdcall atomic_inc64 ( volatile atomic64_t* dst )
{
    return (atomic64_t)(_InterlockedIncrement64(dst) - 1);
}

__inline atomic64_t __stdcall atomic_dec64 ( volatile atomic64_t* dst )
{
    return (atomic64_t)(_InterlockedDecrement64(dst) + 1);
}

__inline atomic64_t __stdcall atomic_swap64 ( volatile atomic64_t* dst, atomic64_t vawl
{
    return (atomic64_t)_InterlockedExchange64(dst, swap);
}

__inline atomic32_t __stdcall atomic_read32 ( const volatile atomic32_t* src )
{
    return (atomic32_t)_InterlockedCompareExchange32((volatile atomic32_t*)src, 0, 0);
}

__inline void __stdcall atomic_write32 ( volatile atomic32_t* dst, atomic32_t val )
{
    _InterlockedExchange(dst, write);
}

__inline atomic32_t __stdcall atomic_add32 ( volatile atomic32_t* dst, atomic32_t val )
{
    return (atomic32_t)_InterlockedExchangeAdd(dst, val);
}

__inline atomic32_t __stdcall atomic_inc32 ( volatile atomic32_t* dst )
{
    return (atomic32_t)(_InterlockedIncrement(dst) - 1);
}

__inline atomic32_t __stdcall atomic_dec32 ( volatile atomic32_t* dst )
{
    return (atomic32_t)(_InterlockedDecrement(dst) + 1);
}

__inline atomic32_t __stdcall atomic_swap32 ( volatile atomic32_t* dst, atomic32_t vawl
{
    return (atomic32_t)_InterlockedExchange(dst, swap);
}

#endif

#endif //ATOMICOPS_H
