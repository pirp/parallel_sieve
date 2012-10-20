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

#include "mcinternal.h"

pthread_key_t mcbsp_internal_init_data;

pthread_key_t mcbsp_internal_thread_data;

bool mcbsp_internal_keys_allocated = false;

pthread_mutex_t mcbsp_internal_keys_mutex = PTHREAD_MUTEX_INITIALIZER;

struct mcbsp_init_data * bsp_begin_check() {
	//check if keys are allocated
	mcbsp_internal_check_keys_allocated();
	//get necessary data
	struct mcbsp_init_data *init = pthread_getspecific( mcbsp_internal_init_data );
	if( init == NULL ) {
		//maybe we are SPMD threads revisiting the bsp_begin?
		const struct mcbsp_thread_data * const thread_data = pthread_getspecific( mcbsp_internal_thread_data );
		if( thread_data != NULL ) {
			//yes, so continue execution
			return NULL;
		} else {
			//no. We are not the ones spawning an SPMD program,
			//neither are we spawned from a corresponding SPMD...
			//
			//two possibilities: either fail hard

			/*fprintf( stderr, "Could not get initialisation data! Was the call to bsp_begin preceded by a call to bsp_init?\n" );
			mcbsp_util_fatal();*/

			//or assume we were called from main() and we construct an implied init
			init = malloc( sizeof( struct mcbsp_init_data ) );
			if( init == NULL ) {
				fprintf( stderr, "Could not perform an implicit initialisation!\n" );
				mcbsp_util_fatal();
			}
			init->spmd = NULL; //we want to call main, but (*void)(void) does not match its profile
			init->argc = 0;
			init->argv = NULL;
			mcbsp_internal_check_keys_allocated();

			if( pthread_setspecific( mcbsp_internal_init_data, init ) != 0 ) {
				fprintf( stderr, "Error: could not set BSP program key in implicit initialisation!\n" );
				mcbsp_util_fatal();
			}
		}
	}

	return init;
}

void bsp_init_internal( struct mcbsp_init_data * const initialisationData ) {
	//store using pthreads setspecific. Note this is per BSP program, not per thread
	//active within this BSP program!
	mcbsp_internal_check_keys_allocated();
	if( pthread_getspecific( mcbsp_internal_init_data ) != NULL ) {
		const struct mcbsp_init_data * const oldData = pthread_getspecific( mcbsp_internal_init_data );
		if( !oldData->ended ) {
			fprintf( stderr, "Warning: initialisation data corresponding to another BSP run found;\n" );
			fprintf( stderr, "         and this other run did not terminate (gracefully).\n" );
		}
	}
	if( pthread_setspecific( mcbsp_internal_init_data, initialisationData ) != 0 ) {
		fprintf( stderr, "Error: could not set BSP program key!\n" );
		mcbsp_util_fatal();
	}
}

void mcbsp_internal_check_keys_allocated() {
	//if already allocated, we are done
	if( mcbsp_internal_keys_allocated ) return;

	//lock mutex against data race
	pthread_mutex_lock( &mcbsp_internal_keys_mutex );

	//if still not allocated, allocate
	if( !mcbsp_internal_keys_allocated ) {
		if( pthread_key_create( &mcbsp_internal_init_data, free ) != 0 ) {
			fprintf( stderr, "Could not allocate mcbsp_internal_init_data key!\n" );
			mcbsp_util_fatal();
		}
		if( pthread_key_create( &mcbsp_internal_thread_data, free ) != 0 ) {
			fprintf( stderr, "Could not allocate mcbsp_internal_thread_data key!\n" );
			mcbsp_util_fatal();
		}
		if( pthread_setspecific( mcbsp_internal_init_data, NULL ) != 0 ) {
			fprintf( stderr, "Could not initialise mcbsp_internal_init_data to NULL!\n" );
			mcbsp_util_fatal();
		}
		if( pthread_setspecific( mcbsp_internal_thread_data, NULL ) != 0 ) {
			fprintf( stderr, "Could not initialise mcbsp_internal_thread_data to NULL!\n" );
			mcbsp_util_fatal();
		}
		mcbsp_internal_keys_allocated = true;
	}

	//unlock mutex and exit
	pthread_mutex_unlock( &mcbsp_internal_keys_mutex );
}

void* mcbsp_internal_spmd( void *p ) {
	//get thread-local data
	struct mcbsp_thread_data *data = (struct mcbsp_thread_data *) p;

	//store thread-local data
	const int rc = pthread_setspecific( mcbsp_internal_thread_data, data );
	if( rc != 0 ) {
		fprintf( stderr, "Could not store thread local data!\n" );
		fprintf( stderr, "(%s)\n", strerror( rc ) );
		mcbsp_util_fatal();
	}

#ifdef __MACH__
	//get rights for accessing Mach's timers
	const kern_return_t rc1 = host_get_clock_service( mach_host_self(), SYSTEM_CLOCK, &(data->clock) );
	if( rc1 != KERN_SUCCESS ) {
		fprintf( stderr, "Could not access the Mach system timer (%s)\n", mach_error_string( rc1 ) );
		mcbsp_util_fatal();
	}

	//record start time
	const kern_return_t rc2 = clock_get_time( data->clock, &(data->start) );
	if( rc2 != KERN_SUCCESS ) {
		fprintf( stderr, "Could not get start time (%s)\n", mach_error_string( rc2 ) );
		mcbsp_util_fatal();
	}
#else
	//record start time
	clock_gettime( CLOCK_MONOTONIC, &(data->start) );
#endif

	//continue with SPMD part
	if( data->init->spmd == NULL )
		main( 0, NULL ); //we had an implicit bsp_init
	else
		data->init->spmd(); //call user-defined SPMD program

	//exit cleanly
	return NULL;
}

void mcbsp_internal_check_aborted() {
	const struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );
#ifndef NDEBUG
	if( data == NULL ) {
		assert( false );
		fprintf( stderr, "Error: could not get thread-local data in call to mcbsp_check_aborted()!\n" );
		mcbsp_util_fatal();
	}
#endif
	if( data->init->abort )
		pthread_exit( NULL );
}

const struct mcbsp_thread_data * mcbsp_internal_const_prefunction() {
	return mcbsp_internal_prefunction();
}

struct mcbsp_thread_data * mcbsp_internal_prefunction() {
	//check if the BSP execution was aborted
	mcbsp_internal_check_aborted();

	//get thread-local data
	struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );

	//check for errors if not in high-performance mode
#ifndef NDEBUG
	if( data == NULL ) {
		fprintf( stderr, "Error: could not get thread-local data in call to bsp_abort( error_message )!\n" );
		mcbsp_util_fatal();
	}
#endif

	//return data
	return data;
}

