// SPDX-FileCopyrightText: 2026 Rowan Claude
// SPDX-License-Identifier: MIT

// The CORE-MATH determinism rows, see docs/determinism-math.md.

#include "core_math_harness.h"
#include "test_macros.h"

static b3CmHardCases s_hardCases;

static float CmSqrt( float x )
{
	return sqrtf( x );
}

// sqrtf is an IEEE-754 basic operation. The double square root is correctly rounded and
// rounding it to float is innocuous since 53 >= 2 * 24 + 2, so this is the reference.
static float CmSqrtReference( float x )
{
	return (float)sqrt( (double)x );
}

static int CoreMathSqrtTest( void )
{
	b3CmMakeHardCases( &s_hardCases );

	// every float of [1, 4) then 2^20 random non-negative bit patterns
	b3CmDense1 dense = { 0x3f800000u, 0x40800000u, B3_CM_DENSE_COUNT, 0x7fffffffu };
	int result = b3CmRunRow1( "sqrt", CmSqrt, CmSqrtReference, 0, &s_hardCases, dense, NULL );
	ENSURE( result == 0 );
	return 0;
}

// The reversed witness: a planted function one ulp off at exactly one input must turn the
// row red with maxulp=1, or the row runner proves nothing.
static uint32_t s_plantedBits;

static float CmPlantedSqrt( float x )
{
	float y = sqrtf( x );
	if ( b3CmBits( x ) == s_plantedBits )
	{
		return nextafterf( y, INFINITY );
	}
	return y;
}

static int CmWitnessRun( uint32_t plantedBits )
{
	s_plantedBits = plantedBits;

	// the same row runner on the hard cases and the random dense draws
	b3CmDense1 dense = { 0, 0, B3_CM_DENSE_COUNT, 0x7fffffffu };
	b3CmRow row;
	int result = b3CmRunRow1( "witness-sqrt", CmPlantedSqrt, CmSqrtReference, 0, &s_hardCases, dense, &row );
	ENSURE( result == 1 );
	ENSURE( row.maxulp == 1 );
	ENSURE( b3CmBits( row.worstX ) == plantedBits );
	printf( "core-math witness red maxulp=%u\n", row.maxulp );
	return 0;
}

static int CoreMathWitnessTest( void )
{
	b3CmMakeHardCases( &s_hardCases );

	// a hard-case input: the smallest positive subnormal
	ENSURE( CmWitnessRun( 0x00000001u ) == 0 );

	// a dense-set input: the first random draw
	uint64_t state = B3_CM_SEED;
	uint32_t first = (uint32_t)b3CmXorshift64( &state ) & 0x7fffffffu;
	ENSURE( CmWitnessRun( first ) == 0 );
	return 0;
}

#if B3_CORE_MATH_ROW_ATAN2
extern int CoreMathAtan2Test( void );
#endif

#if B3_CORE_MATH_ROW_SINCOS
extern int CoreMathSinCosTest( void );
#endif

int CoreMathTest( void )
{
	RUN_SUBTEST( CoreMathSqrtTest );
	RUN_SUBTEST( CoreMathWitnessTest );

#if B3_CORE_MATH_ROW_ATAN2
	RUN_SUBTEST( CoreMathAtan2Test );
#else
	printf( "core-math row=atan2 pending: cm-fast3d-atan2-sincos\n" );
#endif

#if B3_CORE_MATH_ROW_SINCOS
	RUN_SUBTEST( CoreMathSinCosTest );
#else
	printf( "core-math row=sincos pending: cm-fast3d-atan2-sincos\n" );
#endif

	return 0;
}
