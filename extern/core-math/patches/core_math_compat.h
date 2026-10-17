// SPDX-FileCopyrightText: 2026 Rowan Claude
// SPDX-License-Identifier: MIT

// Force-included into the vendored CORE-MATH files on MSVC cl, which has neither the GCC
// builtins nor unsigned __int128. clang-cl, gcc, clang and MinGW compile the files as
// upstream ships them and never see this header.
//
// tools/revendor-core-math.sh --prove force-includes it into a gcc build with
// B3_CORE_MATH_COMPAT_FORCE=1, so the exact fallback paths cl compiles are proven equal,
// output for output, to unpatched CORE-MATH built by gcc.

#pragma once

#if defined( _MSC_VER ) && !defined( __clang__ )
#define B3_CORE_MATH_COMPAT 1
#elif defined( B3_CORE_MATH_COMPAT_FORCE )
#define B3_CORE_MATH_COMPAT 1
#endif

#if defined( B3_CORE_MATH_COMPAT )

#include <math.h>
#include <stdint.h>

// Read by the patches in this directory.
#define CORE_MATH_NO_INT128 1
#define CORE_MATH_NO_BUILTIN_ROUNDEVEN 1
#define CORE_MATH_NO_CONST_NAN 1

// The builtins the files call, mapped to the C99 functions. Each one is exact or
// correctly rounded by IEEE-754 (fma, fabs, copysign, round), so the mapping changes no value.
#define __builtin_fma( x, y, z ) fma( x, y, z )
#define __builtin_fmaf( x, y, z ) fmaf( x, y, z )
#define __builtin_fabs( x ) fabs( x )
#define __builtin_fabsf( x ) fabsf( x )
#define __builtin_copysign( x, y ) copysign( x, y )
#define __builtin_copysignf( x, y ) copysignf( x, y )
#define __builtin_round( x ) round( x )
#define __builtin_ctzll( x ) core_math_ctzll( x )

#if defined( _MSC_VER ) && !defined( __clang__ )
#include <intrin.h>
// __builtin_expect and __attribute__ carry no value semantics.
#define __builtin_expect( x, y ) ( x )
#define __attribute__( x )

#pragma intrinsic( _BitScanForward64 )

static __inline int core_math_ctzll( uint64_t x )
{
	unsigned long index;
	_BitScanForward64( &index, x );
	return (int)index;
}

// Full 64 x 64 -> 128 bit product: the low half is returned, the high half stored.
static __inline uint64_t core_math_mul64( uint64_t a, uint64_t b, uint64_t* hi )
{
#if defined( _M_X64 )
	return _umul128( a, b, hi );
#else
	*hi = __umulh( a, b );
	return a * b;
#endif
}
#else
// The portable versions, compiled only by the --prove build.
static inline int core_math_ctzll( uint64_t x )
{
	int n = 0;
	while ( ( x & 1u ) == 0 )
	{
		x >>= 1;
		n += 1;
	}
	return n;
}

static inline uint64_t core_math_mul64( uint64_t a, uint64_t b, uint64_t* hi )
{
	uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
	uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
	uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	uint64_t mid = ( p00 >> 32 ) + ( p01 & 0xffffffffu ) + ( p10 & 0xffffffffu );
	*hi = p11 + ( p01 >> 32 ) + ( p10 >> 32 ) + ( mid >> 32 );
	return ( mid << 32 ) | ( p00 & 0xffffffffu );
}
#endif

#endif
