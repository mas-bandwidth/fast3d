// SPDX-FileCopyrightText: 2026 Rowan Claude
// SPDX-License-Identifier: MIT

// Harness for the CORE-MATH determinism rows, see docs/determinism-math.md.
// A row compares a library function against its correctly rounded reference on the hard
// cases plus a dense set, and fails when the worst distance in ulps exceeds the row bound.
// Row cards copy the sqrt row in test_core_math.c and add one file under test/core_math/.

#pragma once

#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_WARNINGS )
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t b3CmBits( float x )
{
	uint32_t u;
	memcpy( &u, &x, sizeof( u ) );
	return u;
}

static inline float b3CmFloat( uint32_t u )
{
	float x;
	memcpy( &x, &u, sizeof( x ) );
	return x;
}

static inline bool b3CmIsNan( float x )
{
	return ( b3CmBits( x ) & 0x7fffffffu ) > 0x7f800000u;
}

// Ordered integer for a float: monotonic in the float order, with -0 one below +0 so the
// two zeros are equal only when their bit patterns are equal.
static inline int64_t b3CmOrdered( float x )
{
	uint32_t u = b3CmBits( x );
	if ( u & 0x80000000u )
	{
		return -(int64_t)( u & 0x7fffffffu ) - 1;
	}
	return (int64_t)u;
}

// Distance in ulps. NaN equals NaN whatever the payload; NaN against a number is the maximum.
static inline uint32_t b3CmUlp( float got, float want )
{
	bool gotNan = b3CmIsNan( got );
	bool wantNan = b3CmIsNan( want );
	if ( gotNan || wantNan )
	{
		return gotNan && wantNan ? 0u : UINT32_MAX;
	}
	int64_t d = b3CmOrdered( got ) - b3CmOrdered( want );
	return (uint32_t)( d < 0 ? -d : d );
}

// Fixed seed xorshift64, never seeded from time.
#define B3_CM_SEED 0x9E3779B97F4A7C15ull

static inline uint64_t b3CmXorshift64( uint64_t* state )
{
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	*state = x;
	return x;
}

// B3_CORE_MATH_EXHAUSTIVE=1 runs every float bit pattern on a univariate row and 2^28
// inputs on a bivariate row. The default is the CI size: hard cases plus 2^20 dense inputs.
static inline bool b3CmExhaustive( void )
{
	const char* e = getenv( "B3_CORE_MATH_EXHAUSTIVE" );
	return e != NULL && strcmp( e, "1" ) == 0;
}

#define B3_CM_DENSE_COUNT ( 1u << 20 )
#define B3_CM_BIVARIATE_EXHAUSTIVE_COUNT ( 1u << 28 )

// Hard cases, both signs where a sign applies:
// - subnormals: every 2^10th, plus the smallest and the largest
// - the 64 floats either side of the float nearest k*pi/2, k in -64..64
// - huge arguments: the 64 largest finite floats and the powers of two 2^20..2^127
// - zeros, infinities, NaN
#define B3_CM_HARD_CAPACITY 40000

typedef struct b3CmHardCases
{
	float values[B3_CM_HARD_CAPACITY];
	int count;
} b3CmHardCases;

static inline void b3CmPushSigned( b3CmHardCases* h, uint32_t magnitudeBits )
{
	h->values[h->count++] = b3CmFloat( magnitudeBits );
	h->values[h->count++] = b3CmFloat( magnitudeBits | 0x80000000u );
}

static inline void b3CmMakeHardCases( b3CmHardCases* h )
{
	h->count = 0;

	for ( uint32_t u = 1024u; u < 0x007fffffu; u += 1024u )
	{
		b3CmPushSigned( h, u );
	}
	b3CmPushSigned( h, 0x00000001u );
	b3CmPushSigned( h, 0x007fffffu );

	// The float nearest k*pi/2 is computed in double then rounded once to float.
	for ( int k = -64; k <= 64; ++k )
	{
		float center = (float)( (double)k * 1.57079632679489661923 );
		int64_t c = b3CmOrdered( center );
		for ( int64_t i = c - 64; i <= c + 64; ++i )
		{
			// back from ordered int to bits
			uint32_t u = i < 0 ? ( (uint32_t)( -( i + 1 ) ) | 0x80000000u ) : (uint32_t)i;
			h->values[h->count++] = b3CmFloat( u );
		}
	}

	for ( uint32_t u = 0x7f7fffffu - 63u; u <= 0x7f7fffffu; ++u )
	{
		b3CmPushSigned( h, u );
	}
	for ( uint32_t e = 20; e <= 127; ++e )
	{
		b3CmPushSigned( h, ( e + 127u ) << 23 );
	}

	b3CmPushSigned( h, 0x00000000u );
	b3CmPushSigned( h, 0x7f800000u );
	h->values[h->count++] = b3CmFloat( 0x7fc00000u );
}

// One row: the accumulator every row uses, univariate or bivariate.
typedef struct b3CmRow
{
	const char* name;
	uint32_t bound;
	uint64_t inputs;
	uint32_t maxulp;
	float worstX;
	float worstY;
	bool bivariate;
} b3CmRow;

static inline b3CmRow b3CmMakeRow( const char* name, uint32_t bound, bool bivariate )
{
	b3CmRow row = { name, bound, 0, 0, 0.0f, 0.0f, bivariate };
	return row;
}

static inline void b3CmCheck( b3CmRow* row, float x, float y, float got, float want )
{
	uint32_t d = b3CmUlp( got, want );
	if ( row->inputs == 0 || d > row->maxulp )
	{
		row->maxulp = d;
		row->worstX = x;
		row->worstY = y;
	}
	row->inputs += 1;
}

// Prints the row line and returns 1 when maxulp exceeds the bound.
static inline int b3CmReport( const b3CmRow* row )
{
	if ( row->bivariate )
	{
		printf( "core-math row=%s inputs=%llu maxulp=%u bound=%u worst=%a,%a\n", row->name, (unsigned long long)row->inputs,
				row->maxulp, row->bound, (double)row->worstX, (double)row->worstY );
	}
	else
	{
		printf( "core-math row=%s inputs=%llu maxulp=%u bound=%u worst=%a\n", row->name, (unsigned long long)row->inputs,
				row->maxulp, row->bound, (double)row->worstX );
	}
	return row->maxulp > row->bound ? 1 : 0;
}

typedef float b3CmFn1( float x );

// The dense set of a univariate row: every bit pattern in [rangeLo, rangeHi), then
// randomCount xorshift draws masked by randomMask. Exhaustive mode replaces it with every
// one of the 2^32 bit patterns.
typedef struct b3CmDense1
{
	uint64_t rangeLo;
	uint64_t rangeHi;
	uint32_t randomCount;
	uint32_t randomMask;
} b3CmDense1;

// The univariate row runner. It prints the row line, copies the row to out when out is not
// NULL, and returns 1 when maxulp exceeds the bound.
static inline int b3CmRunRow1( const char* name, b3CmFn1* fn, b3CmFn1* ref, uint32_t bound, const b3CmHardCases* hard,
							   b3CmDense1 dense, b3CmRow* out )
{
	b3CmRow row = b3CmMakeRow( name, bound, false );

	for ( int i = 0; i < hard->count; ++i )
	{
		float x = hard->values[i];
		b3CmCheck( &row, x, 0.0f, fn( x ), ref( x ) );
	}

	if ( b3CmExhaustive() )
	{
		dense.rangeLo = 0;
		dense.rangeHi = 1ull << 32;
		dense.randomCount = 0;
	}

	for ( uint64_t u = dense.rangeLo; u < dense.rangeHi; ++u )
	{
		float x = b3CmFloat( (uint32_t)u );
		b3CmCheck( &row, x, 0.0f, fn( x ), ref( x ) );
	}

	uint64_t state = B3_CM_SEED;
	for ( uint32_t i = 0; i < dense.randomCount; ++i )
	{
		float x = b3CmFloat( (uint32_t)b3CmXorshift64( &state ) & dense.randomMask );
		b3CmCheck( &row, x, 0.0f, fn( x ), ref( x ) );
	}

	if ( out != NULL )
	{
		*out = row;
	}
	return b3CmReport( &row );
}
