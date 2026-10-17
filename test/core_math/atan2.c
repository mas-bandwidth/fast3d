// SPDX-FileCopyrightText: 2026 Rowan Claude
// SPDX-License-Identifier: MIT

// The CORE-MATH atan2 row, see docs/determinism-math.md.

#include "box3d/math_functions.h"

#include "../core_math_harness.h"
#include "../test_macros.h"

extern float cr_atan2f( float y, float x );

static b3CmHardCases s_hardCases;

static float CmAtan2( float y, float x )
{
	return b3Atan2( y, x );
}

static float CmAtan2Reference( float y, float x )
{
	return cr_atan2f( y, x );
}

static void CmAtan2Check( b3CmRow* row, float y, float x )
{
	b3CmCheck( row, x, y, CmAtan2( y, x ), CmAtan2Reference( y, x ) );
}

int CoreMathAtan2Test( void )
{
	b3CmMakeHardCases( &s_hardCases );

	b3CmRow row = b3CmMakeRow( "atan2", 0, true );

	// every ordered pair of a 256-entry subset of the hard cases
	int step = s_hardCases.count > 256 ? s_hardCases.count / 256 : 1;
	for ( int i = 0; i < s_hardCases.count; i += step )
	{
		for ( int j = 0; j < s_hardCases.count; j += step )
		{
			CmAtan2Check( &row, s_hardCases.values[i], s_hardCases.values[j] );
		}
	}

	// 2^20 random finite bit-pattern pairs
	uint64_t state = B3_CM_SEED;
	uint32_t randomCount = 0;
	while ( randomCount < B3_CM_DENSE_COUNT )
	{
		uint32_t uy = (uint32_t)b3CmXorshift64( &state );
		uint32_t ux = (uint32_t)b3CmXorshift64( &state );
		if ( ( uy & 0x7f800000u ) == 0x7f800000u || ( ux & 0x7f800000u ) == 0x7f800000u )
		{
			continue;
		}
		CmAtan2Check( &row, b3CmFloat( uy ), b3CmFloat( ux ) );
		randomCount += 1;
	}

	// the 1024 x 1024 grid on [-1, 1]^2
	for ( int i = 0; i < 1024; ++i )
	{
		float y = (float)( -1.0 + 2.0 * (double)i / 1023.0 );
		for ( int j = 0; j < 1024; ++j )
		{
			float x = (float)( -1.0 + 2.0 * (double)j / 1023.0 );
			CmAtan2Check( &row, y, x );
		}
	}

	int result = b3CmReport( &row );
	ENSURE( result == 0 );
	return 0;
}
