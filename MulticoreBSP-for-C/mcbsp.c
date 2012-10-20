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

//for documentation see the .h file:
#include "mcbsp-internal.h"

int *MCBSP_MANUAL_AFFINITY = NULL;
enum mcbsp_affinity_mode MCBSP_AFFINITY;

void bsp_begin( const MCBSP_PROCESSOR_INDEX_DATATYPE P ) {
	struct mcbsp_init_data * const init = bsp_begin_check();

	//if the check did not return an init struct, we are a
	//spawned thread and should just continue the SPMD
	//code.
	if( init == NULL )
		return;

	//otherwise we need to start the SPMD code 
	int *pinning = mcbsp_util_pinning( MCBSP_AFFINITY, P );
	if( pinning == NULL ) {
		fprintf( stderr, "Could not get a valid pinning!\n" );
		mcbsp_util_fatal();
	}

	init->threads = malloc( P * sizeof( pthread_t ) );
	if( init->threads == NULL ) {
		fprintf( stderr, "Could not allocate new threads!\n" );
		mcbsp_util_fatal();
	}

	pthread_attr_t attr;

#ifndef __MACH__
	cpu_set_t mask;
#endif

	//further initialise init object
	init->P     = P;
	init->abort = false;
	init->ended = false;
	init->sync_entry_counter = 0;
	init->sync_exit_counter  = 0;
	pthread_mutex_init( &(init->mutex), NULL );
	pthread_cond_init ( &(init->condition), NULL );
	pthread_cond_init ( &(init->mid_condition), NULL );
	mcbsp_util_address_table_initialise( &(init->global2local), P );
	init->threadData = malloc( P * sizeof( struct mcbsp_thread_data * ) );
	init->tagSize = 0;

	//spawn P-1 threads. The condition checks for both signed and unsigned types
	//since user may set MCBSP_PROCESSOR_INDEX_DATATYPE to a signed type.
	for( MCBSP_PROCESSOR_INDEX_DATATYPE s = P - 1; s < P && s >= 0; --s ) {
		//allocate new thread-local data
		struct mcbsp_thread_data *thread_data = malloc( sizeof( struct mcbsp_thread_data ) );
		if( thread_data == NULL ) {
			fprintf( stderr, "Could not allocate local thread data!\n" );
			mcbsp_util_fatal();
		}
		//provide a link to the SPMD program init struct
		thread_data->init   = init;
		//set local ID
		thread_data->bsp_id = s;
		//set the maximum number of registered globals at any time (0, since SPMD not started yet)
		thread_data->localC = 0;
		//initialise local to global map
		mcbsp_util_address_map_initialise( &(thread_data->local2global ) );
		//initialise stack used for efficient registration of globals after de-registrations
		mcbsp_util_stack_initialise( &(thread_data->removedGlobals), sizeof( unsigned long int ) );
		//initialise stack used for de-registration of globals
		mcbsp_util_stack_initialise( &(thread_data->localsToRemove), sizeof( void * ) );
		//initialise stacks used for communication
		thread_data->queues = malloc( P * sizeof( struct mcbsp_util_stack ) );
		for( MCBSP_PROCESSOR_INDEX_DATATYPE i = 0; i < P; ++i )
			mcbsp_util_stack_initialise( &(thread_data->queues[ i ]), sizeof( struct mcbsp_communication_request) );
		//initialise default tag size
		thread_data->newTagSize = 0;
		//initialise BSMP queue
		mcbsp_util_stack_initialise( &(thread_data->bsmp), sizeof( void * ) );
		//initialise push request queue
		mcbsp_util_stack_initialise( &(thread_data->localsToPush), sizeof( struct mcbsp_push_request ) );
		//provide a link back to this thread-local data struct
		init->threadData[ s ] = thread_data;

		//spawn new threads if s>0
		if( s > 0 ) {
			//create POSIX threads attributes (for pinning)
			pthread_attr_init( &attr );
#ifndef __MACH__
			CPU_ZERO( &mask );
			CPU_SET ( pinning[ s ], &mask );
			pthread_attr_setaffinity_np( &attr, sizeof( cpu_set_t ), &mask );
#endif

			//spawn the actual thread
			if( pthread_create( &(init->threads[ s ]), &attr, mcbsp_internal_spmd, thread_data ) != 0 ) {
				fprintf( stderr, "Could not spawn new thread!\n" );
				mcbsp_util_fatal();
			}

#ifdef __MACH__
			thread_port_t osx_thread = pthread_mach_thread_np( init->threads[ s ] );
			struct thread_affinity_policy ap;
			if( MCBSP_AFFINITY == SCATTER ) {
				//Affinity API release notes do not specify whether 0 is a valid tag, or in fact equal to NULL; so 1-based to be sure
				ap.affinity_tag = s + 1;
			} else if( MCBSP_AFFINITY == COMPACT ) {
				ap.affinity_tag = 1;
			} else if( MCBSP_AFFINITY == MANUAL ) {
				ap.affinity_tag = MCBSP_MANUAL_AFFINITY[ s ];
			} else {
				fprintf( stderr, "Unhandled affinity type for Mac OS X!\n" );
				mcbsp_util_fatal();
			}
			thread_policy_set( osx_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&ap, THREAD_AFFINITY_POLICY_COUNT );
#endif

			//destroy attributes object
			pthread_attr_destroy( &attr );
		} else {
			//continue ourselves as bsp_id 0. Do pinning
#ifdef __MACH__
			thread_port_t osx_thread = pthread_mach_thread_np( pthread_self() );
			struct thread_affinity_policy ap;
			if( MCBSP_AFFINITY == SCATTER || MCBSP_AFFINITY == COMPACT )
				ap.affinity_tag = 1;
			else if( MCBSP_AFFINITY == MANUAL )
				ap.affinity_tag = MCBSP_MANUAL_AFFINITY[ s ];
			else {
				fprintf( stderr, "Unhandled affinity type for Mac OS X!\n" );
				mcbsp_util_fatal();
			}
			thread_policy_set( osx_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&ap, THREAD_AFFINITY_POLICY_COUNT );
#else
			CPU_ZERO( &mask );
			CPU_SET ( pinning[ s ], &mask );
			if( pthread_setaffinity_np( pthread_self(), sizeof( cpu_set_t ), &mask ) != 0 ) {
				fprintf( stderr, "Could not pin master thread to requested hardware thread (%d)!\n", pinning[ s ] );
				mcbsp_util_fatal();
			}
#endif
			//record our own descriptor
			init->threads[ 0 ] = pthread_self();
			//copy part of mcbsp_internal_spmd.
			const int rc = pthread_setspecific( mcbsp_internal_thread_data, thread_data );
			if( rc != 0 ) {
				fprintf( stderr, "Could not store thread-local data in continuator thread!\n" );
				fprintf( stderr, "(%s)\n", strerror( rc ) );
				mcbsp_util_fatal();
			}
#ifdef __MACH__
			//get rights for accessing Mach's timers
			const kern_return_t rc1 = host_get_clock_service( mach_host_self(), SYSTEM_CLOCK, &(thread_data->clock) );
			if( rc1 != KERN_SUCCESS ) {
				fprintf( stderr, "Could not access the Mach system timer (%s)\n", mach_error_string( rc1 ) );
				mcbsp_util_fatal();
			}
			const kern_return_t rc2 = clock_get_time( thread_data->clock, &(thread_data->start) );
			if( rc2 != KERN_SUCCESS ) {
				fprintf( stderr, "Could not get starting time (%s)\n", mach_error_string( rc2 ) );
				mcbsp_util_fatal();
			}
#else
			clock_gettime( CLOCK_MONOTONIC, &(thread_data->start) );
#endif
			//this one is extra, enables possible BSP-within-BSP execution.
			if( pthread_setspecific( mcbsp_internal_init_data, NULL ) != 0 ) {
				fprintf( stderr, "Could not reset initialisation data to NULL on SPMD start!\n" );
				mcbsp_util_fatal();
			}
		}
	}
	//free pinning only if it was not manually defined
	if( MCBSP_AFFINITY != MANUAL )
		free( pinning );
}

void bsp_end() {
	//get thread-local data
	struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );
	if( data == NULL ) {
		fprintf( stderr, "Error: could not get thread-local data in call to bsp_abort( error_message )!\n" );
		mcbsp_util_fatal();
	}

	//record end
	data->init->ended = true;

	//get lock
	pthread_mutex_lock( &(data->init->mutex) );

	//see if synchronisation is complete
	if( data->init->sync_entry_counter++ == data->init->P - 1 ) {
		data->init->sync_entry_counter = 0;
		pthread_cond_broadcast( &(data->init->condition) );
	} else
		pthread_cond_wait( &(data->init->condition), &(data->init->mutex) );

	//unlock mutex
	pthread_mutex_unlock( &(data->init->mutex) );

	//set thread-local data to NULL
	if( pthread_setspecific( mcbsp_internal_thread_data, NULL ) != 0 ) {
		fprintf( stderr, "Could not set thread-local data to NULL on thread exit.\n" );
		mcbsp_util_fatal();
	}

	//free data and exit gracefully,
#ifdef __MACH__
	mach_port_deallocate( mach_task_self(), data->clock );
#endif
	mcbsp_util_address_map_destroy( &(data->local2global) );
	mcbsp_util_stack_destroy( &(data->removedGlobals) );
	mcbsp_util_stack_destroy( &(data->localsToRemove) );
	for( MCBSP_PROCESSOR_INDEX_DATATYPE s = 0; s < data->init->P; ++s ) {
		mcbsp_util_stack_destroy( &(data->queues[ s ]) );
	}
	free( data->queues );
	mcbsp_util_stack_destroy( &(data->bsmp) );
	mcbsp_util_stack_destroy( &(data->localsToPush) );

	//exit if not master thread
	if( data->bsp_id != 0 ) {
		//free thread-local data
		free( data );
		pthread_exit( NULL );
	}

	//master thread cleans up init struct
	struct mcbsp_init_data *init = data->init;

	//that's everything we needed from the thread-local data struct
	free( data );

	//wait for other threads
	for( MCBSP_PROCESSOR_INDEX_DATATYPE s = 1; s < init->P; ++s )
		pthread_join( init->threads[ s ], NULL );

	//destroy mutex and condition 
	pthread_mutex_destroy( &(init->mutex) );
	pthread_cond_destroy(  &(init->condition) );
	pthread_cond_destroy(  &(init->mid_condition) );
	
	//destroy global address table
	mcbsp_util_address_table_destroy( &(init->global2local) );

	//destroy pointers to thread-local data structs
	free( init->threadData );

	//exit gracefully, free threads array
	free( init->threads );

	//exit gracefully, free BSP program init data
	free( init );
}

void bsp_init( void (*spmd)(void), int argc, char **argv ) {
	//create a BSP-program specific initial data struct
	struct mcbsp_init_data *initialisationData = malloc( sizeof( struct mcbsp_init_data ) );
	if( initialisationData == NULL ) {
		fprintf( stderr, "Error: could not allocate MulticoreBSP initialisation struct!\n" );
		mcbsp_util_fatal();
	}
	//set values
	initialisationData->spmd 	= spmd;
	initialisationData->bsp_program = NULL;
	initialisationData->argc 	= argc;
	initialisationData->argv 	= argv;
	//continue initialisation
	bsp_init_internal( initialisationData );
}

MCBSP_PROCESSOR_INDEX_DATATYPE bsp_pid() {
	const struct mcbsp_thread_data * const data = mcbsp_internal_const_prefunction();
	return data->bsp_id;
}

MCBSP_PROCESSOR_INDEX_DATATYPE bsp_nprocs() {
	const struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );
	if( data == NULL ) {
		//called outside from SPMD environment.
		//return machineInfo data.
		return mcbsp_util_getMachineInfo()->P;
	} else {
		//check if the BSP execution was aborted
		mcbsp_internal_check_aborted();
		//return nprocs involved in this SPMD run
		return data->init->P;
	}
}

void bsp_abort( char *error_message, ... ) {
	//get variable arguments struct
	va_list args;
	va_start( args, error_message );

	//pass to bsp_vabort
	bsp_vabort( error_message, args );

	//mark end of variable arguments
	va_end( args );
}

void bsp_vabort( char *error_message, va_list args ) {

	//print error message
	vfprintf( stderr, error_message, args );
	
	//get thread-local data and check for errors
	const struct mcbsp_thread_data * const data = mcbsp_internal_const_prefunction();

	//always check for failure of getting thread data, even in high-performance mode
#ifdef NDEBUG
	if( data == NULL ) {
		fprintf( stderr, "Error: could not get thread-local data in call to bsp_abort( error_message )!\n" );
		mcbsp_util_fatal();
	}
#endif
	
	//send signal to all sibling threads
	data->init->abort = true;

	//if there are threads in sync, wake them up
	//first get lock, otherwise threads may sync
	//while checking for synched threads
	pthread_mutex_lock( &(data->init->mutex) );
	if( data->init->sync_entry_counter > 0 )
		pthread_cond_broadcast( &(data->init->condition) );
	pthread_mutex_unlock( &( data->init->mutex) );
	
	//quit execution
	pthread_exit( NULL );
}

void bsp_sync() {
	//get local data
	struct mcbsp_thread_data * const data = pthread_getspecific( mcbsp_internal_thread_data );

	//get lock
	pthread_mutex_lock( &(data->init->mutex) );

	//see if synchronisation is complete
	if( data->init->sync_entry_counter++ == data->init->P - 1 ) {
		data->init->sync_entry_counter = 0;
		pthread_cond_broadcast( &(data->init->condition) );
	} else
		pthread_cond_wait( &(data->init->condition), &(data->init->mutex) );

	//unlock mutex
	pthread_mutex_unlock( &(data->init->mutex) );

	//before continuing execution, check if we woke up due to an abort
	//and now exit if so (we could not exit earlier as not unlocking the
	//sync mutex will cause a deadlock).
	mcbsp_internal_check_aborted();

	//check for mismatched sync/end
	if( data->init->ended ) {
		fprintf( stderr, "Mismatched bsp_sync and bsp_end detected!\n" );
		mcbsp_util_fatal();
	}

	//handle the various BSP requests
	
	//update tagSize, phase 1
	if( data->bsp_id == 0 && data->newTagSize != data->init->tagSize )
		data->init->tagSize = data->newTagSize;

	//look for requests with destination us, first cache get-requests
	for( MCBSP_PROCESSOR_INDEX_DATATYPE s = 0; s < data->init->P; ++s ) {
		struct mcbsp_util_stack * const queue = &(data->init->threadData[ s ]->queues[ data->bsp_id ]);
		//each request in queue is directed to us. Handle all of them.
		for( size_t r = 0; r < queue->top; ++r ) {
			struct mcbsp_communication_request * const request = (struct mcbsp_communication_request *) (((char*)(queue->array)) + r * queue->size);
			if( request->payload == NULL ) {
				//allocate payload
				request->payload = malloc( request->length );
				//no data race here since we are the only ones allowed to write here
				memcpy( request->payload, request->source, request->length );
				//nullify payload (effectively turning the request into a put-request)
				request->source = NULL;
			}
		}
	}

	//handle bsp_pop_reg
	while( !mcbsp_util_stack_empty( &(data->localsToRemove ) ) ) {
		//get local memory address to remove registration of
		void * const toRemove = *((void**)(mcbsp_util_stack_pop( &(data->localsToRemove) )));

		//get corresponding global key
		const unsigned long int globalIndex = mcbsp_util_address_map_get( &(data->local2global), toRemove );
		if( globalIndex == ULONG_MAX ) {
			fprintf( stderr, "Error: bsp_pop_reg requested on non-registered pointer!\n" );
			mcbsp_util_fatal();
		}

		//delete from table
		if( mcbsp_util_address_table_delete( &(data->init->global2local), globalIndex, data->bsp_id ) ) {
			//NOTE: this is safe, since it is guaranteed that this address table entry
			//	will not change during synchronisation.

			//delete from map
			mcbsp_util_address_map_remove( &(data->local2global), toRemove );
		}

		//register globalIndex now is free
		if( data->localC == globalIndex + 1 )
			--(data->localC);
		else
			mcbsp_util_stack_push( &(data->removedGlobals), (void*)(&globalIndex) );
	}

	//handle push_reg
	while( !mcbsp_util_stack_empty( &(data->localsToPush) ) ) {
		//get address
		const struct mcbsp_push_request request =
			*((struct mcbsp_push_request*)mcbsp_util_stack_pop( &(data->localsToPush) ));
		void * const address = request.address;
		const MCBSP_BYTESIZE_TYPE size = request.size;

		//get global index of this registration. First check map if the key already existed
		const unsigned long int mapSearch     = mcbsp_util_address_map_get( &(data->local2global), address);
		//if the key was not found, create a new global entry
		const unsigned long int global_number = mapSearch != ULONG_MAX ? mapSearch :
								mcbsp_util_stack_empty( &(data->removedGlobals) ) ?
								data->localC++ :
								*(unsigned long int*)mcbsp_util_stack_pop( &(data->removedGlobals) );

		//insert value, local2global map (if this is a new global entry)
		if( mapSearch == ULONG_MAX )
			mcbsp_util_address_map_insert( &(data->local2global), address, global_number );

		//insert value, global2local map (false sharing is possible here, but effects should be negligable)
		mcbsp_util_address_table_set( &(data->init->global2local), global_number, data->bsp_id, address, size );
	}

	//coordinate exit using the same mutex (but not same condition!)
	pthread_mutex_lock( &(data->init->mutex) );
	if( data->init->sync_exit_counter++ == data->init->P - 1 ) {
		data->init->sync_exit_counter = 0;
		pthread_cond_broadcast( &(data->init->mid_condition) );
	} else
		pthread_cond_wait( &(data->init->mid_condition), &(data->init->mutex) );
	pthread_mutex_unlock( &(data->init->mutex) );

	//update tagsize, phase 2 (check)
	if( data->newTagSize != data->init->tagSize ) {
		fprintf( stderr, "Different tag sizes requested from different processes (%ld requested while process 0 requested %ld)!\n", data->newTagSize, data->init->tagSize );
		mcbsp_util_fatal();
	}
	
	//now process put requests to local destination
	for( MCBSP_PROCESSOR_INDEX_DATATYPE s = 0; s < data->init->P; ++s ) {
		struct mcbsp_util_stack * const queue = &(data->init->threadData[ s ]->queues[ data->bsp_id ]);
		//each request in queue is directed to us. Handle all of them.
		while( !mcbsp_util_stack_empty( queue ) ) {
			struct mcbsp_communication_request * const request = (struct mcbsp_communication_request *) mcbsp_util_stack_pop( queue );
			if( request->source == NULL && request->destination == NULL && request->payload != NULL ) {
				//this is a BSMP message
				//construct message
				void * message = malloc( request->length );
				memcpy( message, request->payload, request->length );
				//record message
				mcbsp_util_stack_push( &(data->bsmp), &message );
				//free payload
				free( request->payload );
			} else if( request->source == NULL && request->payload != NULL ) {
				//no data race here since we are the only ones allowed to write here
				memcpy( request->destination, request->payload, request->length );
				//free payload
				free( request->payload );
			} else {
				fprintf( stderr, "Unknown BSP communication request encountered!\n" );
				mcbsp_util_fatal();
			}
		}
	}

	//final sync
	pthread_mutex_lock( &(data->init->mutex) );
	if( data->init->sync_entry_counter++ == data->init->P - 1 ) {
		data->init->sync_entry_counter = 0;
		pthread_cond_broadcast( &(data->init->condition) );
	} else
		pthread_cond_wait( &(data->init->condition), &(data->init->mutex) );
	pthread_mutex_unlock( &(data->init->mutex) );
}

double bsp_time() {

	//get init data
	const struct mcbsp_thread_data * const data = mcbsp_internal_const_prefunction();

	//get stop time

#ifdef __MACH__
	//get rights for accessing Mach's timers
	const kern_return_t rc1 = host_get_clock_service( mach_host_self(), SYSTEM_CLOCK, &(data->clock) );
	if( rc1 != KERN_SUCCESS ) {
		fprintf( stderr, "Could not access the Mach system timer (%s)\n", mach_error_string( rc1 ) );
		mcbsp_util_fatal();
	}

	mach_timespec_t stop;
	const kern_return_t rc2 = clock_get_time( data->clock, &stop );
	if( rc2 != KERN_SUCCESS ) {
		fprintf( stderr, "Could not get time at call to bsp_time (%s)\n", mach_error_string( rc2 ) );
		mcbsp_util_fatal();
	}
#else
	struct timespec stop;
	clock_gettime( CLOCK_MONOTONIC, &stop);
#endif

	//return time
	double time = (stop.tv_sec-data->start.tv_sec);
	time += (stop.tv_nsec-data->start.tv_nsec)/1000000000.0;
	return time;
}

void bsp_push_reg( void * const address, const MCBSP_BYTESIZE_TYPE size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t size = (size_t) size_in;

	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//construct the push request
	struct mcbsp_push_request toPush;
	toPush.address = address;
	toPush.size    = size;

	//push the request
	mcbsp_util_stack_push( &(data->localsToPush), (void*)(&toPush) );
}

void bsp_pop_reg( void * const address ) {
	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//register for removal
	mcbsp_util_stack_push( &(data->localsToRemove), (void*)(&address) );
}

void bsp_put( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const source,
	void * const destination, const MCBSP_BYTESIZE_TYPE offset_in,
	const MCBSP_BYTESIZE_TYPE size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t offset = (size_t) offset_in;
	const size_t size   = (size_t) size_in;

	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//build request
	struct mcbsp_communication_request request;

	//record source address
	request.source = NULL;

	//record destination
	const unsigned long int globalIndex = mcbsp_util_address_map_get( &(data->local2global), destination );
	const struct mcbsp_util_address_table_entry * const entry = mcbsp_util_address_table_get( &(data->init->global2local), globalIndex, pid );
	if( offset + size > entry->size ) {
		fprintf( stderr, "Error: bsp_put would go out of bounds at destination processor (offset=%ld, size=%ld, while registered memory area is %ld bytes)!\n", offset, size, entry->size );
		bsp_abort( "Aborting due to BSP primitive call with invalid arguments." );
	}
	request.destination = ((char*)(entry->address)) + offset;

	//record length
	request.length = size;

	//record payload
	request.payload = malloc( size );
	memcpy( request.payload, source, size );

	//record request
	mcbsp_util_stack_push( &(data->queues[ pid ]), &request );
}

void bsp_get( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const source,
	const MCBSP_BYTESIZE_TYPE offset_in, void * const destination,
	const MCBSP_BYTESIZE_TYPE size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t offset = (size_t) offset_in;
	const size_t size   = (size_t) size_in;

	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//build request
	struct mcbsp_communication_request request;

	//record source address
	const unsigned long int globalIndex = mcbsp_util_address_map_get( &(data->local2global), source );
	const struct mcbsp_util_address_table_entry * entry = mcbsp_util_address_table_get( &(data->init->global2local), globalIndex, pid );
	if( offset + size > entry->size ) {
		fprintf( stderr, "Error: bsp_get would go out of bounds at source processor (offset=%ld, size=%ld, while registered memory area is %ld bytes)!\n", offset, size, entry->size );
		bsp_abort( "Aborting due to BSP primitive call with invalid arguments." );
	}
	request.source = ((char*)(entry->address)) + offset;

	//record destination
	request.destination = destination;

	//record length
	request.length = size;

	//record payload
	request.payload = NULL;

	//record request
	mcbsp_util_stack_push( &(data->queues[ data->bsp_id ]), &request );
}

void bsp_direct_get( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const source,
        const MCBSP_BYTESIZE_TYPE offset_in, void * const destination,
	const MCBSP_BYTESIZE_TYPE size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t offset = (size_t) offset_in;
	const size_t size   = (size_t) size_in;
	
	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//get source address
	const unsigned long int globalIndex = mcbsp_util_address_map_get( &(data->local2global), source );
	const struct mcbsp_util_address_table_entry * entry = mcbsp_util_address_table_get( &(data->init->global2local), globalIndex, pid );
	if( offset + size > entry->size ) {
		fprintf( stderr, "Error: bsp_direct_get would go out of bounds at source processor (offset=%ld, size=%ld, while registered memory area is %ld bytes)!\n", offset, size, entry->size );
		bsp_abort( "Aborting due to BSP primitive call with invalid arguments." );
	}

	//perform direct get
	memcpy( destination, ((char*)(entry->address)) + offset, size );
}

void bsp_set_tagsize( MCBSP_BYTESIZE_TYPE * const size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t size = (size_t) *size_in;

	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//record new tagsize
	data->newTagSize = size;

	//return old tag size
	*size_in = (MCBSP_BYTESIZE_TYPE) (data->init->tagSize);
}

void bsp_send( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const tag,
	const void * const payload, const MCBSP_BYTESIZE_TYPE size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t size = (size_t) size_in;

	//get init data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//build request
	struct mcbsp_communication_request request;

	//record source address
	request.source = NULL;

	//record destination
	request.destination = NULL;

	//record length
	request.length = size + data->init->tagSize + sizeof( size_t );

	//record payload (tag, payload_size, payload)
	request.payload = malloc( request.length );
	//tag message
	memcpy( request.payload, tag, data->init->tagSize );
	//payload size
	memcpy( (char*)request.payload + data->init->tagSize, &size, sizeof( size_t ) );
	//actual payload
	memcpy( (char*)request.payload + sizeof( size_t ) + data->init->tagSize, payload, size );

	//record request
	mcbsp_util_stack_push( &(data->queues[ pid ]), &request );
}

void bsp_qsize( MCBSP_NUMMSG_TYPE * const packets,
	MCBSP_BYTESIZE_TYPE * const accumulated_size ) {
	//get thread data
	const struct mcbsp_thread_data * const data = mcbsp_internal_const_prefunction();

	//if requested,
	if( accumulated_size != NULL ) {
		//update accumulated_size
		*accumulated_size = (MCBSP_BYTESIZE_TYPE) 0;
		for( size_t i = 0; i < data->bsmp.top; ++i ) {
			//get to the raw message triplet
			const char * const p_msg = *((char**)((char*)data->bsmp.array + i * data->bsmp.size));
			//get the second entry; the payload size
			const size_t msgSize = *((size_t*)(p_msg + data->init->tagSize));
			//add to accumulated size
			*accumulated_size += (MCBSP_BYTESIZE_TYPE) msgSize;
		}
	}

	//update number of packets still in queue
	*packets = (MCBSP_NUMMSG_TYPE) (data->bsmp.top);
}

void bsp_move( void * const payload, const MCBSP_BYTESIZE_TYPE max_copy_size_in ) {
	//library internals work with size_t only; convert if necessary
	const size_t max_copy_size = (size_t) max_copy_size_in;

	//get thread data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//if stack is empty, do not return anything
	if( mcbsp_util_stack_empty( &(data->bsmp) ) )
		return;

	//get message
	void * message = *((void**)mcbsp_util_stack_pop( &(data->bsmp) ));

	//get message size
	const size_t size = *((size_t*)((char*)message + data->init->tagSize));

	//copy message
	memcpy( payload, (char*)message + data->init->tagSize + sizeof( size_t ),
		size > max_copy_size ? max_copy_size : size );

	//free message triplet
	free( message );
}

void bsp_get_tag( MCBSP_BYTESIZE_TYPE * const status,
	void * const tag ) {
	//get thread data
	const struct mcbsp_thread_data * const data = mcbsp_internal_const_prefunction();

	//if stack is empty, set status to -1
	if( mcbsp_util_stack_empty( &(data->bsmp) ) ) {
		//return failure
		*status = (MCBSP_BYTESIZE_TYPE) -1;
	} else {
		//set status to tag size and copy tag into target memory area
		*status = (MCBSP_BYTESIZE_TYPE) data->init->tagSize;
		memcpy( tag, *(void**)mcbsp_util_stack_peek( &(data->bsmp) ), data->init->tagSize );
	}
}

MCBSP_BYTESIZE_TYPE bsp_hpmove( void* * const p_tag, void* * const p_payload ) {
	
	//get thread data
	struct mcbsp_thread_data * const data = mcbsp_internal_prefunction();

	//if empty, return -1
	if( mcbsp_util_stack_empty( &(data->bsmp) ) )
		return -1;

	//otherwise get pointers
	char * area = *(char**)mcbsp_util_stack_pop( &(data->bsmp) );
	const size_t size = *((size_t*)(area + data->init->tagSize));
	*p_tag     = area;
	*p_payload = area + data->init->tagSize + sizeof( size_t );

	//return the payload length
	return (MCBSP_BYTESIZE_TYPE) size;
}

#ifdef ENABLE_FAKE_HP_DIRECTIVES
void bsp_hpput( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const source,
        void * const destination, const MCBSP_BYTESIZE_TYPE offset,
	const MCBSP_BYTESIZE_TYPE size ) {
	bsp_put( pid, source, destination, offset, size );
}

void bsp_hpget( const MCBSP_PROCESSOR_INDEX_DATATYPE pid, const void * const source,
        const MCBSP_BYTESIZE_TYPE offset, void * const destination,
	const MCBSP_BYTESIZE_TYPE size ) {
	bsp_get( pid, source, offset, destination, size );
}
#endif

