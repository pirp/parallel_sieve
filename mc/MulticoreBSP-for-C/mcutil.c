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
#include "mcutil.h"

struct mcbsp_util_machine_info *MCBSP_MACHINE_INFO 	= NULL;
pthread_mutex_t mcbsp_util_machine_info_mutex		= PTHREAD_MUTEX_INITIALIZER;

struct mcbsp_util_machine_info *mcbsp_util_getMachineInfo() {
	if( MCBSP_MACHINE_INFO != NULL )
		return MCBSP_MACHINE_INFO;

	pthread_mutex_lock( &mcbsp_util_machine_info_mutex );
	if( MCBSP_MACHINE_INFO == NULL ) {
		MCBSP_MACHINE_INFO = malloc( sizeof( struct mcbsp_util_machine_info ) );
		bool Pset = false;
		FILE *fp = fopen ( "machine.info", "r" ) ;
		if( fp != NULL ) {
			char LINE_BUFFER[ 255 ];
			char key[ 253 ];
			unsigned long int value = 0;
			const char * const getsres = fgets( LINE_BUFFER, 255, fp );
			if( getsres != NULL && sscanf( LINE_BUFFER, "%s %lu", key, &value ) == 2 ) {
				if( strcmp( key, "cores" ) == 0 ) {
					MCBSP_MACHINE_INFO->P = value;
					Pset = true;
				} else
					fprintf( stderr, "Warning: unknown key in machine.info (%s)\n", key );
			} else
				fprintf( stderr, "Warning: error while reading machine.info\n" );
		}
		if( !Pset ) {
			MCBSP_MACHINE_INFO->P = sysconf( _SC_NPROCESSORS_ONLN );
		}
	}
	pthread_mutex_unlock( &mcbsp_util_machine_info_mutex );
	return MCBSP_MACHINE_INFO;
}

void mcbsp_util_destroyMachineInfo() {
	if( MCBSP_MACHINE_INFO != NULL ) {
		free( MCBSP_MACHINE_INFO );
		MCBSP_MACHINE_INFO = NULL;
	}
}

int* mcbsp_util_pinning( const enum mcbsp_affinity_mode affinity, const MCBSP_PROCESSOR_INDEX_DATATYPE P ) {
	if( P == 0 )
		return NULL;

	int * ret = NULL;
	if( affinity != MANUAL ) {
		ret = malloc( P * sizeof( int ) );
		if( ret == NULL ) {
			fprintf( stderr, "Error: could not allocate pinning array!\n" );
			mcbsp_util_fatal();
		}
	}
	switch( affinity ) {
		case SCATTER:
		{
			const double stepsize = ((double)mcbsp_util_getMachineInfo()->P)/((double)P);
			double currentPin = 0.0;
			for( MCBSP_PROCESSOR_INDEX_DATATYPE s = 0; s < P; ++s, currentPin += stepsize ) {
				ret[ s ] = (int)currentPin;
			}
		}
			break;
		case COMPACT:
		{
			const unsigned long int max = mcbsp_util_getMachineInfo()->P;
			const unsigned long int threads_per_core	= P / max;
			const unsigned long int plusOneUntil		= threads_per_core == 0 ? P : P % max;
			unsigned long int thread = 0;
			for( unsigned long int core = 0; core < max; ++core ) {
				for( unsigned long int k = 0; k < threads_per_core; ++k )
					ret[ thread++ ] = core;
				if( core < plusOneUntil )
					ret[ thread++ ] = core;
			}
		}
			break;
		case MANUAL:
			if( MCBSP_MANUAL_AFFINITY == NULL ) {
				fprintf( stderr, "No manual affinity array provided, yet a manual affinity strategy was set.\n" );
				mcbsp_util_fatal();
			}
			ret = MCBSP_MANUAL_AFFINITY;
			break;
		default:
			fprintf( stderr, "Apparently a strategy has been defined in `enum mcbsp_util_affinity_mode',\n" );
			fprintf( stderr, "but not implemented in mcbsp_util_pinning. Please correct.\n" );
			mcbsp_util_fatal();
	}
	return ret;
}

void mcbsp_util_fatal() {
	fflush( stderr );
	exit( EXIT_FAILURE );
}

void mcbsp_util_stack_initialise( struct mcbsp_util_stack * const stack, const size_t size ) {
	stack->cap   = 16;
	stack->top   = 0;
	stack->size  = size;
	stack->array = malloc( 16 * size );
}

void mcbsp_util_stack_grow( struct mcbsp_util_stack * const stack ) {
	void * replace = malloc( 2 * stack->cap * stack->size );
	memcpy( replace, stack->array, stack->top * stack->size );
	stack->cap *= 2;
	free( stack->array );
	stack->array = replace;
}

bool mcbsp_util_stack_empty( const struct mcbsp_util_stack * const stack ) {
	return stack->top == 0;
}

void * mcbsp_util_stack_pop( struct mcbsp_util_stack * const stack ) {
	return ((char*)(stack->array)) + (stack->size * (--(stack->top)));
}

void * mcbsp_util_stack_peek( const struct mcbsp_util_stack * const stack ) {
	return ((char*)(stack->array)) + (stack->size * ( stack->top - 1 ) );
}

void mcbsp_util_stack_push( struct mcbsp_util_stack * const stack, void * const item ) {
	if( stack->top == stack->cap )
		mcbsp_util_stack_grow( stack );

	//copy a single item into the stack
	memcpy( ((char*)(stack->array)) + stack->size * ((stack->top)++), item, stack->size );
}

void mcbsp_util_stack_destroy( struct mcbsp_util_stack * const stack ) {
	stack->cap  = 0;
	stack->top  = 0;
	stack->size = 0;
	if( stack->array != NULL ) {
		free( stack->array );
		stack->array = NULL;
	}
}

void mcbsp_util_address_table_initialise( struct mcbsp_util_address_table * const table, const unsigned long int P ) {
	pthread_mutex_init( &(table->mutex), NULL );
	table->cap  = 16;
	table->P    =  P;
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	table->table= malloc( 16 * sizeof( struct mcbsp_util_stack * ) );
	for( unsigned long int i = 0; i < 16; ++i ) {
		table->table[ i ] = malloc( P * sizeof( struct mcbsp_util_stack ) );
		for( unsigned long int k = 0; k < table->P; ++k )
			mcbsp_util_stack_initialise( &(table->table[ i ][ k ]), sizeof(struct mcbsp_util_address_table_entry) ); 
	}
#else
	table->table= malloc( 16 * sizeof( struct mcbsp_util_address_table_entry * ) );
	for( unsigned long int i = 0; i < 16; ++i )
		table->table[ i ] = malloc( P * sizeof( struct mcbsp_util_address_table_entry ) );
#endif
}

void mcbsp_util_address_table_grow( struct mcbsp_util_address_table * const table ) {
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	struct mcbsp_util_stack ** replace = malloc( 2 * table->cap * sizeof( struct mcbsp_util_stack * ) );
	for( unsigned long int i = 0; i < 2 * table->cap; ++i ) {
		replace[ i ] = malloc( table->P * sizeof( struct mcbsp_util_stack ) );
		if( i < table->cap ) {
			for( unsigned long int k = 0; k < table->P; ++k )
				replace[ i ][ k ] = table->table[ i ][ k ];
			free( table->table[ i ] );
		} else
			for( unsigned long int k = 0; k < table->P; ++k )
				mcbsp_util_stack_initialise( &(replace[ i ][ k ]), sizeof( struct mcbsp_util_address_table_entry )  );
	}
#else
	struct mcbsp_util_address_table_entry ** replace = malloc( 2 * table->cap * sizeof( struct mcbsp_util_address_table_entry * ) );
	for( unsigned long int i = 0; i < 2 * table->cap; ++i ) {
		replace[ i ] = malloc( table->P * sizeof( struct mcbsp_util_address_table_entry ) );
		if( i < table->cap ) {
			for( unsigned long int k = 0; k < table->P; ++k )
				replace[ i ][ k ] = table->table[ i ][ k ];
			free( table->table[ i ] );
		}
	}
#endif
	free( table->table );
	table->table = replace;
	table->cap  *= 2;
}

void mcbsp_util_address_table_setsize( struct mcbsp_util_address_table * const table, const unsigned long int target_size ) {
	//if capacity is good enough, exit
	if( table->cap > target_size )
		return;

	//otherwise get lock and increase table size
	pthread_mutex_lock( &(table->mutex) );
	//check again whether another thread already
	//increased the capacity; if so, do exit
	while( table->cap <= target_size )
		mcbsp_util_address_table_grow( table );
	pthread_mutex_unlock( &(table->mutex) );
}

void mcbsp_util_address_table_destroy( struct mcbsp_util_address_table * const table ) {
	for( unsigned long int i = 0; i < table->cap; ++i ) {
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
		for( unsigned long int k = 0; k < table->P; ++k )
			mcbsp_util_stack_destroy( &(table->table[ i ][ k ]) );
#endif
		free( table->table[ i ] );
	}
	free( table->table );
	pthread_mutex_destroy( &(table->mutex) );
	table->cap   = 0;
	table->P     = ULONG_MAX;
	table->table = NULL;
}

void mcbsp_util_address_table_set( struct mcbsp_util_address_table * const table, const unsigned long int key, const unsigned long int version, void * const value, const size_t size ) {
	mcbsp_util_address_table_setsize( table, key );
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	struct mcbsp_util_stack * const stack = &( table->table[ key ][ version ] );
	struct mcbsp_util_address_table_entry new_entry;
	new_entry.address = value;
	new_entry.size    = size;
	mcbsp_util_stack_push( stack, &new_entry );
#else
	struct mcbsp_util_address_table_entry * const entry = &( table->table[ key ][ version ] );
	entry->address = value;
	entry->size    = size;
#endif
}

const struct mcbsp_util_address_table_entry * mcbsp_util_address_table_get( const struct mcbsp_util_address_table * const table, const unsigned long int key, const unsigned long int version ) {
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	struct mcbsp_util_stack * const stack = &( table->table[ key ][ version ] );
	if( mcbsp_util_stack_empty( stack ) )
		return NULL;
	else {
		return (struct mcbsp_util_address_table_entry *)mcbsp_util_stack_peek( stack );
	}
#else
	return &(table->table[ key ][ version ]);
#endif
}

bool mcbsp_util_address_table_delete( struct mcbsp_util_address_table * const table, const unsigned long int key, const unsigned long int version ) {
#ifdef MCBSP_ALLOW_MULTIPLE_REGS
	struct mcbsp_util_stack * const stack = &( table->table[ key ][ version ] );
	mcbsp_util_stack_pop( stack );
	if( mcbsp_util_stack_empty( stack ) )
		return true;
	else
		return false;
#else
	struct mcbsp_util_address_table_entry * const entry = &( table->table[ key ][ version ] );
	entry->address = NULL;
	entry->size    = 0;
	return true;
#endif
}

void mcbsp_util_address_map_initialise( struct mcbsp_util_address_map * const address_map ) {
	if( address_map == NULL ) {
		fprintf( stderr, "Warning: mcbsp_util_address_map_initialise called with NULL argument.\n" );
		return;
	}
	
	address_map->cap    = 16;
	address_map->size   = 0;
	address_map->keys   = malloc( 16 * sizeof( void * ) );
	address_map->values = malloc( 16 * sizeof( unsigned long int ) );
	
	if( address_map->keys == NULL || address_map->values == NULL ) {
		fprintf( stderr, "Error: could not allocate memory in mcbsp_util_address_map_initialise!\n" );
		mcbsp_util_fatal();
	}
}

void mcbsp_util_address_map_grow( struct mcbsp_util_address_map * const address_map ) {
	if( address_map == NULL ) {
		fprintf( stderr, "Warning: mcbsp_util_address_map_grow called with NULL argument.\n" );
		return;
	}

	//create replacement arrays
	void * * replace_k = malloc( 2 * address_map->cap * sizeof( void * ) );
	unsigned long int * replace_v = malloc( 2 * address_map->cap * sizeof( unsigned long int ) );

	//copy old values
	for( unsigned long int i = 0; i < (address_map->size); ++i ) {
		replace_k[ i ] = address_map->keys[ i ];
		replace_v[ i ] = address_map->values[ i ];
	}

	//free old arrays
	free( address_map->keys   );
	free( address_map->values );

	//update struct
	address_map->keys   = replace_k;
	address_map->values = replace_v;
	address_map->cap   *= 2;
}

void mcbsp_util_address_map_destroy( struct mcbsp_util_address_map * const address_map ) {
	//delete arrays
	free( address_map->keys   );
	free( address_map->values );

	//set empty values
	address_map->cap    = 0;
	address_map->size   = 0;
	address_map->keys   = NULL;
	address_map->values = NULL;
}

size_t mcbsp_util_address_map_binsearch( const struct mcbsp_util_address_map * const address_map, const void * const key, size_t lo, size_t hi ) {

	//check boundaries
	if( address_map->keys[ lo ] == key ) return lo;
	if( address_map->keys[ hi ] == key ) return hi;

	//initialise search for insertion point
	size_t mid;
	mid  = lo + hi;
	mid /= 2;
		
	//do binary search
	do {
		//check midpoint
		if( address_map->keys[ mid ] == key )
			return mid;
		else if( key < address_map->keys[ mid ] )
			hi = mid;
		else
			lo = mid;
		//get new midpoint
		mid  = lo + hi;
		mid /= 2;
	} while( lo != mid );
	//done, return current midpoint
	return mid;
}

void mcbsp_util_address_map_insert( struct mcbsp_util_address_map * const address_map, void * const key, unsigned long int value ) {

	//use binary search to find the insertion point
	size_t insertionPoint;
	if( address_map->size > 0 ) {
		insertionPoint = mcbsp_util_address_map_binsearch( address_map, key, 0, address_map->size - 1 );
		if( address_map->keys[ insertionPoint ] == key ) {
			fprintf( stderr, "Warning: mcbsp_util_address_map_insert ignored as key already existed (at %ld/%ld)!\n", insertionPoint, (address_map->size-1) );
			return; //error: key was already here
		} else if( address_map->keys[ insertionPoint ] < key )
			++insertionPoint; //we need to insert at position of higher key
	} else
		insertionPoint = 0;

	//scoot over
	for( unsigned long int i = address_map->size; i > insertionPoint; --i ) {
		address_map->keys  [ i ] = address_map->keys  [ i - 1 ];
		address_map->values[ i ] = address_map->values[ i - 1 ];
	}

	//actual insert
	address_map->keys  [ insertionPoint ] = key;
	address_map->values[ insertionPoint ] = value;
	address_map->size++;

	//check capacity
	if( address_map->size == address_map->cap )
		mcbsp_util_address_map_grow( address_map );

	//done
}

void mcbsp_util_address_map_remove( struct mcbsp_util_address_map * const address_map, void * const key ) {

	if( address_map->size == 0 ) {
		fprintf( stderr, "Warning: mcbsp_util_address_map_remove ignored since map was empty!\n" );
		return; //there are no entries
	}

	//use binary search to find the entry to remove
	const size_t index = mcbsp_util_address_map_binsearch( address_map, key, 0, address_map->size - 1 );

	//check equality
	if( address_map->keys[ index ] != key ) {
		fprintf( stderr, "Warning: mcbsp_util_address_map_remove ignored since key was not found!\n" );
		return; //key not found
	}

	//scoot over to delete
	for( size_t i = index; i < address_map->size - 1; ++i ) {
		address_map->keys  [ i ] = address_map->keys  [ i + 1 ];
		address_map->values[ i ] = address_map->values[ i + 1 ];
	}

	//update size
	--(address_map->size);

	//done
}

unsigned long int mcbsp_util_address_map_get( const struct mcbsp_util_address_map * const address_map, const void * const key ) {
	//check if we are in range
	if( address_map->size == 0 )
		return ULONG_MAX;

	//do binary search
	size_t found = mcbsp_util_address_map_binsearch( address_map, key, 0, address_map->size - 1 );

	//return if key is found
	if( address_map->keys[ found ] == key )
		return address_map->values[ found ];
	else //otherwise return ULONG_MAX
		return ULONG_MAX;
}

