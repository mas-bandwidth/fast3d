// SPDX-FileCopyrightText: 2026 Rowan Claude
// SPDX-License-Identifier: MIT

// The CORE-MATH sin and cos rows, see docs/determinism-math.md.

#include "box3d/math_functions.h"

#include "../core_math_harness.h"
#include "../test_macros.h"

extern float cr_sinf( float x );
extern float cr_cosf( float x );

static b3CmHardCases s_hardCases;

static float CmSin( float x )
{
	return b3ComputeCosSin( x ).sine;
}

static float CmCos( float x )
{
	return b3ComputeCosSin( x ).cosine;
}

static void CmSinCosCheck( b3CmRow* sinRow, b3CmRow* cosRow, float x )
{
	b3CmCheck( sinRow, x, 0.0f, CmSin( x ), cr_sinf( x ) );
	b3CmCheck( cosRow, x, 0.0f, CmCos( x ), cr_cosf( x ) );
}

static void CmSinCosRun( b3CmRow* sinRow, b3CmRow* cosRow )
{
	// 2^20 random finite bit patterns
	uint64_t state = B3_CM_SEED;
	uint32_t randomCount = 0;
	while ( randomCount < B3_CM_DENSE_COUNT )
	{
		uint32_t u = (uint32_t)b3CmXorshift64( &state );
		if ( ( u & 0x7f800000u ) == 0x7f800000u )
		{
			continue;
		}
		CmSinCosCheck( sinRow, cosRow, b3CmFloat( u ) );
		randomCount += 1;
	}

	// 2^20 evenly spaced floats over [-8*pi, 8*pi]
	const double pi = 3.14159265358979323846;
	for ( uint32_t i = 0; i < B3_CM_DENSE_COUNT; ++i )
	{
		float x = (float)( -8.0 * pi + 16.0 * pi * (double)i / (double)( B3_CM_DENSE_COUNT - 1 ) );
		CmSinCosCheck( sinRow, cosRow, x );
	}
}

int CoreMathSinCosTest( void )
{
	b3CmMakeHardCases( &s_hardCases );

	b3CmRow sinRow = b3CmMakeRow( "sin", 0, false );
	b3CmRow cosRow = b3CmMakeRow( "cos", 0, false );

	for ( int i = 0; i < s_hardCases.count; ++i )
	{
		CmSinCosCheck( &sinRow, &cosRow, s_hardCases.values[i] );
	}

	CmSinCosRun( &sinRow, &cosRow );

	int sinResult = b3CmReport( &sinRow );
	int cosResult = b3CmReport( &cosRow );
	ENSURE( sinResult == 0 );
	ENSURE( cosResult == 0 );
	return 0;
}
