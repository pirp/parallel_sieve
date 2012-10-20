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
#include "mcinternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

unsigned long int checkPcount[ 3 ];
unsigned long int *checkLocalIntAddress[ 3 ];
unsigned char check = 0;
bool checkP[ 3 ];
unsigned char superstep = 0;
bool superstepOK = true;

pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;

void spmd() {
	//parallel over three processes
	bsp_begin( 3 );

	//test bsp_push_reg (results in next superstep)
	unsigned long int localInt;
	bsp_push_reg( &localInt, sizeof( unsigned long int ) );
	checkLocalIntAddress[ bsp_pid() ] = &localInt;

	//check pid/nprocs, both using primitives as well as manually
	checkPcount[ bsp_pid() ] = bsp_nprocs();
	pthread_mutex_lock( &test_mutex );
	check++;
	checkP[ bsp_pid() ] = true;
	pthread_mutex_unlock( &test_mutex );

	//nobody should be at superstep 0
	if( superstep == 1 )
		superstepOK = false;

	//test barrier synchronisation
	bsp_sync();

	//note someone is at superstep 1
	superstep = 1;

	//check bsp_time
	if( bsp_time() <= 0 )
		bsp_abort( "FAILURE \t bsp_time returned 0 or less!\n" );

	//set up a pop_reg, but should only take effect after the next sync
	//(testing the push_reg after this statement thus provides a free test)
	bsp_pop_reg( &localInt );
	struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );
	if( data->localsToRemove.top != 1 || data->localsToRemove.cap != 16 ||
		*((void**)(data->localsToRemove.array)) != (void*)&localInt ) {
		fprintf( stderr, "FAILURE \t bsp_pop_reg did not push entry on the to-remove stack (%p != %p)!\n",
			*((void**)(data->localsToRemove.array)), (void*)&localInt );
		mcbsp_util_fatal();
	}

	//check push_reg
	for( unsigned char i=0; i<3; ++i ) {
		if( checkLocalIntAddress[ i ] != mcbsp_util_address_table_get( &(data->init->global2local), 0, i )->address ) {
			fprintf( stderr, "FAILURE \t bsp_push_reg did not register correct address!\n" );
			mcbsp_util_fatal();
		}
	}

	bsp_sync();

	//check pop_reg
	for( unsigned char i=0; i<3; ++i ) {
		if( mcbsp_util_address_table_get( &(data->init->global2local), 0, i ) != NULL ||
			data->localC != 0 ) {
			fprintf( stderr, "FAILURE \t bsp_pop_reg did not de-register correctly (entry=%p)!\n",
				mcbsp_util_address_table_get( &(data->init->global2local), 0, i )->address );
			mcbsp_util_fatal();
		}
		//localInt = *(unsigned long int*)mcbsp_util_stack_pop( &(data->removedGlobals) );
	}

	bsp_sync();

	//going to test communication primitives on the following area
	unsigned long int commTest[ 3 ];
	commTest[ 0 ] = commTest[ 1 ] = bsp_pid();
	commTest[ 2 ] = bsp_nprocs();
	bsp_push_reg( &commTest, 3 * sizeof( unsigned long int ) );

	//make push valid
	bsp_sync();

	//after this put, commTest[ 0 ] should equal bsp_pid, commTest[ 1, 2 ] should equal bsp_pid-1 mod bsp_nprocs
	bsp_put( (bsp_pid() + 1) % bsp_nprocs(), &commTest, &commTest, sizeof( unsigned long int ), 2*sizeof( unsigned long int) );
	commTest[ 2 ] = ULONG_MAX; //this should not influence the result after sync.

	//test behind-the-scenes
	const struct mcbsp_util_stack queue = data->queues[ (bsp_pid() + 1) % bsp_nprocs() ];
	if( queue.cap != 16 || queue.top != 1 || queue.size != sizeof( struct mcbsp_communication_request ) ) {
		fprintf( stderr, "FAILURE \t bsp_put did not adapt the communication queue as expected!\n" );
		mcbsp_util_fatal();
	}
	const struct mcbsp_communication_request request = *(struct mcbsp_communication_request*) queue.array;
	if( request.source != NULL || request.length != 2*sizeof( unsigned long int) || ((unsigned long int*)request.payload)[0] != bsp_pid() ||
		((unsigned long int*)request.payload)[1] != bsp_pid() ) {
		fprintf( stderr, "FAILURE \t bsp_put did not push an expected communication request!\n" );
		//fprintf( stderr, "source=%p, length=%ld, payload=%p ([%ld %ld])\n", request.source, request.length, request.payload,
		//	((unsigned long int*)request.payload)[0], ((unsigned long int*)request.payload)[1] );
		mcbsp_util_fatal();
	}
	//note there is no easy way to check request.destination; the top-level BSP test will handle that one

	bsp_sync();

	//test for the above expectation after bsp_put, namely
	//commTest[ 0 ] should equal bsp_pid, commTest[ 1, 2 ] should equal bsp_pid-1 mod bsp_nprocs
	if( commTest[ 0 ] != bsp_pid() || commTest[ 1 ] != (bsp_pid()+bsp_nprocs()-1)%bsp_nprocs() ||
		commTest[ 2 ] != (bsp_pid()+bsp_nprocs()-1)%bsp_nprocs() ) {
		fprintf( stderr, "FAILURE \t array after bsp_put is not as expected! (%d: %ld %ld %ld))\n", bsp_pid(), commTest[ 0 ], commTest[ 1 ], commTest[ 2 ] );
		mcbsp_util_fatal();
	}
	
	//do a get on the next processor on the last element of commTest
	bsp_get( (bsp_pid() + 1) % bsp_nprocs(), &commTest, 2 * sizeof( unsigned long int ), &(commTest[ 2 ]), sizeof( unsigned long int ) );

	//fill the expected value after the get to test non-buffering
	commTest[ 2 ] = bsp_pid();

	//communicate
	bsp_sync();

	//commTest[ 0 ] should equal bsp_pid, commTest[ 1 ] should equal bsp_pid-1, commTest[ 2 ] should be bsp_pid+1
	if( commTest[ 0 ] != bsp_pid() || commTest[ 1 ] != (bsp_pid()+bsp_nprocs() - 1)%bsp_nprocs() ) {
		fprintf( stderr, "FAILURE \t start of array after bsp_get changed! (%d: %ld %ld %ld))\n", bsp_pid(), commTest[ 0 ], commTest[ 1 ], commTest[ 2 ] );
		mcbsp_util_fatal();
	}
	if( commTest[ 2 ] != (bsp_pid()+bsp_nprocs() + 1)%bsp_nprocs() ) {
		fprintf( stderr, "FAILURE \t last element of array after bsp_get erroneous! (%d: %ld %ld %ld))\n", bsp_pid(), commTest[ 0 ], commTest[ 1 ], commTest[ 2 ] );
		mcbsp_util_fatal();
	}

	bsp_sync();

	//test direct_get functionality
	unsigned long int commTest2[ 3 ];
	commTest2[ 0 ] = commTest[ 0 ];

	//get commTest[1] from right neighbour
	bsp_direct_get( (bsp_pid() + 1) % bsp_nprocs(), &commTest, sizeof( unsigned long int ), &(commTest2[ 1 ]), sizeof( unsigned long int ) );

	//get commTest[2] from left neighbour
	bsp_direct_get( (bsp_pid() + bsp_nprocs() - 1) % bsp_nprocs(), &commTest, 2 * sizeof( unsigned long int ), &(commTest2[ 2 ]), sizeof( unsigned long int ) );

	//now everything should equal bsp_pid
	if( commTest2[ 0 ] != bsp_pid() || commTest2[ 1 ] != bsp_pid() || commTest2[ 2 ] != bsp_pid() ) {
		fprintf( stderr, "FAILURE \t direct_get does not function properly! (%d: [%ld %ld %ld])\n", bsp_pid(), commTest2[ 0 ], commTest2[ 1 ], commTest2[ 2 ] );
		mcbsp_util_fatal();
	}

	//now test single BSMP message
	bsp_send( (bsp_pid() + 1) % bsp_nprocs(), NULL, &commTest, sizeof( unsigned long int ) );
	
	//check messages
	if( queue.cap != 16 || queue.top != 1 || queue.size != sizeof( struct mcbsp_communication_request ) ) {
		fprintf( stderr, "FAILURE \t bsp_send did not adapt the communication queue as expected!\n" );
		mcbsp_util_fatal();
	}
	const struct mcbsp_communication_request request2 = *(struct mcbsp_communication_request*) queue.array;
	if( request2.source != NULL || request2.destination != NULL ||
		request2.length != sizeof( unsigned long int) + sizeof( size_t ) ||
		*(unsigned long int *)(((char*)request2.payload) + sizeof(size_t)) != bsp_pid() ) {
		fprintf( stderr, "FAILURE \t bsp_send did not push the expected communication request!\n" );
		mcbsp_util_fatal();
	}

	bsp_sync();

	//inspect incoming BSMP queue
	if( data->bsmp.cap != 16 || data->bsmp.top != 1 || data->bsmp.size != sizeof( void * ) ) {
		fprintf( stderr, "FAILURE \t BSMP queue after superstep with sends is not as expected!\n" );
		mcbsp_util_fatal();
	}
	if( *(unsigned long int*)(*(char **)(data->bsmp.array) + sizeof( size_t )) !=
		( bsp_pid() + bsp_nprocs() - 1 ) % bsp_nprocs() ) {
		fprintf( stderr, "FAILURE \t Value in BSMP queue is not correct!\n" );
		mcbsp_util_fatal();
	}
	
	//inspect using primitives
	MCBSP_NUMMSG_TYPE   packets;
	MCBSP_BYTESIZE_TYPE packetSize;
	bsp_qsize( &packets, &packetSize );
	if( packets != 1 || packetSize != sizeof( unsigned long int ) ) {
		fprintf( stderr, "FAILURE \t bsp_qsize does not function correctly!\n" );
		mcbsp_util_fatal();
	}
	bsp_move( &commTest, sizeof( unsigned long int ) );
	if( commTest[ 0 ] != ( bsp_pid() + bsp_nprocs() - 1 ) % bsp_nprocs() ) {
		fprintf( stderr, "FAILURE \t bsp_move does not function correctly!\n" );
		mcbsp_util_fatal();
	}
	
	//check set_tagsize
	MCBSP_BYTESIZE_TYPE tsz = sizeof( unsigned long int );
	bsp_set_tagsize( &tsz );
	if( tsz != 0 ) {
		fprintf( stderr, "FAILURE \t return value of bsp_set_tagsize is incorrect!\n" );
		mcbsp_util_fatal();
	}

	bsp_sync();

	//check set_tagsize
	if( data->init->tagSize != sizeof( unsigned long int ) ) {
		fprintf( stderr, "FAILURE \t bsp_set_tagsize failed!\n" );
		mcbsp_util_fatal();
	}
	
	commTest[ 0 ] = bsp_pid();
	commTest[ 1 ] = 3;
	commTest[ 2 ] = 8 + bsp_pid();
	for( char i = 0; i < bsp_nprocs(); ++i )
		bsp_send( i, &commTest, &(commTest[1]), 2 * sizeof( unsigned long int ) );

	bsp_sync();

	MCBSP_BYTESIZE_TYPE status;
	unsigned long int tag;
	for( char i = 0; i < bsp_nprocs(); ++i ) {
		bsp_get_tag( &status, &tag );
		if( tag >= bsp_nprocs() || status != sizeof( unsigned long int ) ) {
			fprintf( stderr, "FAILURE \t error in BSMP tag handling! (tag=%ld)\n", tag );
			mcbsp_util_fatal();
		}
		unsigned long int *p_tag, *msg;
		bsp_hpmove( (void**)&p_tag, (void**)&msg );
		if( msg[ 0 ] != 3 || *p_tag != tag ) {
			fprintf( stderr, "FAILURE \t error in bsp_hpmove!\n" );
			mcbsp_util_fatal();
		}
		commTest[ tag ] = msg[ 1 ];
		free( p_tag );
	}
	for( unsigned short int i = 0; i < bsp_nprocs(); ++i ) {
		if( commTest[ i ] != 8 + i ) {
			fprintf( stderr, "FAILURE \t error in bsp_tag / bsp_(hp)move combination!\n" );
			mcbsp_util_fatal();
		}
	}

	bsp_sync();

#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	//test multiple regs
	double mreg[17];
	bsp_push_reg( &(mreg[0]), 7*sizeof( double ) );

	bsp_sync();

	double mregs = 1.3;
	bsp_put( (bsp_pid() + 1) % bsp_nprocs(), &mregs, &mreg, 6 * sizeof( double ), sizeof( double ) );
	bsp_push_reg( &(mreg[0]), 17*sizeof( double ) );

	bsp_sync();

	bsp_push_reg( &(mreg[0]), 13*sizeof( double ) );
	bsp_put( (bsp_pid() + 1) % bsp_nprocs(), &mregs, &mreg, 16 * sizeof( double ), sizeof( double ) );

	bsp_sync();

	if( mreg[ 6 ] != mreg[ 16 ] ||  mreg[ 6 ] != mregs ) {
		fprintf( stderr, "FAILURE \t error in bsp_put + multiple bsp_push_reg calls (%f,%f,%f,...,%f,%f)\n", mreg[ 5 ], mreg[ 6 ], mreg[ 7 ], mreg[ 15 ], mreg[ 16 ] );
		mcbsp_util_fatal();
	}
	bsp_pop_reg( &(mreg[0]) );
	bsp_pop_reg( &(mreg[0]) );

	bsp_sync();

	bsp_put( (bsp_pid() + 1) % bsp_nprocs(), &mregs, &mreg, 2 * sizeof( double ), sizeof( double ) );

	bsp_sync();

	if( mreg[ 2 ] != mregs ) {
		fprintf( stderr, "FAILURE \t error in bsp_put + multiple bsp_push_reg + multiple bsp_pop_reg calls\n" );
		mcbsp_util_fatal();
	}
#endif

	bsp_end();
}

int main(int argc, char **argv) {
	//test bsp_init
	bsp_init( spmd, argc, argv );
	if( !mcbsp_internal_keys_allocated ) {
		fprintf( stderr, "FAILURE \t bsp_init did not initialise internal keys!\n" );
		mcbsp_util_fatal();
	}
	struct mcbsp_init_data *initialisationData = pthread_getspecific( mcbsp_internal_init_data );
	if( initialisationData == NULL ) {
		fprintf( stderr, "FAILURE \t did not retrieve correct program initialisation data!\n" );
		mcbsp_util_fatal();
	}
	if( initialisationData->spmd != spmd ) {
		fprintf( stderr, "FAILURE \t did not retrieve correct user-defined SPMD entry point!\n" );
		mcbsp_util_fatal();
	}
	if( initialisationData->argc != argc ) {
		fprintf( stderr, "FAILURE \t did not retrieve correct argument count!\n" );
		mcbsp_util_fatal();
	}
	if( initialisationData->argv != argv ) {
		fprintf( stderr, "FAILURE \t did not retrieve correct arguments!\n" );
		mcbsp_util_fatal();
	}
	//bsp_init OK
	
	//test bsp_begin and bsp_end, init test
	mcbsp_util_getMachineInfo();
	MCBSP_MACHINE_INFO->P = 7;
	MCBSP_MANUAL_AFFINITY = malloc( 3 * sizeof( int ) );
	for( unsigned char i=0; i<3; ++i )
		MCBSP_MANUAL_AFFINITY[ i ] = 0;
	MCBSP_AFFINITY = MANUAL;
	checkP[ 0 ] = checkP[ 1 ] = checkP[ 2 ] = false;

	//actual test
	spmd();
	if( check != 3 ) {
		fprintf( stderr, "FAILURE \t bsp_begin(3) did not correctly start three processes!\n" );
		mcbsp_util_fatal();
	}
	if( !( checkP[ 0 ] && checkP[ 1 ] && checkP[ 2 ] ) ) {
		fprintf( stderr, "FAILURE \t bsp_pid does not function correctly!\n" );
		mcbsp_util_fatal();
	}
	for( unsigned char i=0; i<3; ++i ) {
		if( checkPcount[ i ] != 3 ) {
			fprintf( stderr, "FAILURE \t bsp_nprocs does not function correctly!\n" );
			mcbsp_util_fatal();
		}
	}
	if( !superstepOK ) {
		fprintf( stderr, "FAILURE \t bsp_sync allowed one or more threads past a synchronisation point before at least one other thread reached it!\n" );
		mcbsp_util_fatal();
	}
	
	//cleanup
	free( MCBSP_MANUAL_AFFINITY );
	MCBSP_MANUAL_AFFINITY = NULL;
	MCBSP_AFFINITY = SCATTER;
	//bsp_begin & bsp_end OK
	
	fprintf( stdout, "SUCCESS\n" );
	exit( EXIT_SUCCESS );
}

