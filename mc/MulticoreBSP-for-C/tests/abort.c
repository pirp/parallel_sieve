/*
 * Copyright (c) 2012 by Albert-Jan N. Yzelman
 *
 * This file is part of MulticoreBSP in C --
 *        a port of the original Java-based MulticoreBSP.
 *
 * MulticoreBSP for C is distributed as part of the original
 * MulticoreBSP and is free software: you can redistribute
 * it and/or modify it under the terms of the GNU Lesser 
 * General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * MulticoreBSP is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the 
 * implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General 
 * Public License along with MulticoreBSP. If not, see
 * <http://www.gnu.org/licenses/>.
 */


#include "mcbsp.h"
#include "mcutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

bool fail;

void spmd() {
	bsp_begin( 3 );
	if( bsp_pid() == 0 ) {
		bsp_abort( "abort test" );
	}
	bsp_sync();
	fail = true;
	bsp_end();
}

int main(int argc, char **argv) {
	//test bsp_init
	bsp_init( spmd, argc, argv );

	//test bsp_begin and bsp_end, init test
	mcbsp_util_getMachineInfo();
	MCBSP_MACHINE_INFO->P = 7;
	MCBSP_MANUAL_AFFINITY = malloc( 3 * sizeof( int ) );
	for( unsigned char i=0; i<3; ++i )
		MCBSP_MANUAL_AFFINITY[ i ] = 0;
	MCBSP_AFFINITY = MANUAL;
	fail = false;

	spmd();

	//cleanup
	free( MCBSP_MANUAL_AFFINITY );

	//report if failure
	if( fail ) {
		fprintf( stdout, "FAILURE \t execution did not stop before bsp_sync()!" );
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

