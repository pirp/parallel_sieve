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

#ifndef _H_MCINTERNAL
#define _H_MCINTERNAL

#include "mcbsp-internal.h"
#include "mcutil.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>

#ifdef __MACH__
 #include <mach/mach.h>
 #include <mach/clock.h>
 #include <mach/mach_error.h>
#endif

/**
 * Initialisation struct.
 */
struct mcbsp_init_data {

	/** User-definied SPMD entry-point. */
	void (*spmd)(void);

	/** In case of a call from the C++ wrapper,
	 *  pointer to the user-defined BSP_program
	 */
	void *bsp_program;

	/** Passed argc from bsp_init. */
	int argc;

	/** Passed argv from bsp_init. */
	char **argv;

	/** Threads corresponding to this BSP program. */
	pthread_t *threads;

	/** Number of processors involved in this run. */
	MCBSP_PROCESSOR_INDEX_DATATYPE P;

	/** Whether the BSP program should be aborted. */
	bool abort;

	/** Whether the BSP program has ended. */
	bool ended;

	/**
	 * Barrier counter for this BSP execution.
	 * Synchronises synchronisation entry.
	 */
	MCBSP_PROCESSOR_INDEX_DATATYPE sync_entry_counter;

	/**
	 * Barrier counter for this BSP program.
	 * Synchronises synchronisation exit.
	 */
	MCBSP_PROCESSOR_INDEX_DATATYPE sync_exit_counter;

	/** Mutex used for critical sections (such as synchronisation). */
	pthread_mutex_t mutex;

	/** Condition used for critical sections. */
	pthread_cond_t condition;

	/** Condition used for critical sections. */
	pthread_cond_t mid_condition;

	/** Address table used for inter-thread communication. */
	struct mcbsp_util_address_table global2local;

	/** Pointers to all thread-local data, as needed for communication. */
	struct mcbsp_thread_data **threadData;

	/** Currently active tag size. */
	size_t tagSize;

};

/**
 * Thread-local data.
 */
struct mcbsp_thread_data {

	/** Initialisation data. */
	struct mcbsp_init_data *init;

	/** Thread-local BSP id. */
	MCBSP_PROCESSOR_INDEX_DATATYPE bsp_id;

#ifdef __MACH__
	/** OS port for getting timings */
	clock_serv_t clock;

	/** 
	 * Stores the start-time of this thread.
	 * (Mach-specific (OS X) timespec)
	 */
	mach_timespec_t start;
#else
	/** Stores the start-time of this thread. */
	struct timespec start;
#endif

	/** Local address to global variable map. */
	struct mcbsp_util_address_map local2global;

	/**
	 * Counts the maximum number of registered variables
	 * at any one time.
	 */
	unsigned long int localC;

	/**
	 * Keeps track of which global variables are removed
	 * (as per bsp_pop_reg).
	 */
	struct mcbsp_util_stack removedGlobals;

	/**
	 * Keeps track which globals will be removed before
	 * the next superstep arrives.
	 */
	struct mcbsp_util_stack localsToRemove;

	/**
	 * The various communication queues used for
	 * communication.
	 */
	struct mcbsp_util_stack * queues;

	/**
	 * Stores the tag size to become active after the
	 * next synchronisation.
	 */
	size_t newTagSize;

	/** The BSMP incoming message queue. */
	struct mcbsp_util_stack bsmp;

	/** The push request queue. */
	struct mcbsp_util_stack localsToPush;

};

/** A generic communication request. */
struct mcbsp_communication_request {
	
	/** Source */
	void * source;

	/** Destination */
	void * destination;

	/** Length */
	size_t length;

	/** Data */
	void * payload;

};

/** Struct corresponding to a single push request. */
struct mcbsp_push_request {

	/** The local address to push. */
	void * address;

	/** The memory range to register. */
	MCBSP_BYTESIZE_TYPE size;

};

/**
 * Performs a BSP intialisation using the init struct supplied.
 * The construction of this struct differs when called from C code,
 * or when called from the C++ wrapper.
 */
void bsp_init_internal( struct mcbsp_init_data * const initialisationData );

/**
 * Checks if everything is all right to start an SPMD program.
 *
 * If there is an error, execution is stopped using mcbsp_util_fatal.
 *
 * @return NULL if the call to bsp_begin was valid, but no further
 * 		actual is required; or a pointer to an initialisation
 * 		struct when the SPMD program is yet to be spawned.
 */
struct mcbsp_init_data * bsp_begin_check();

/** Per-MulticoreBSP program initialisation data. */
extern pthread_key_t mcbsp_internal_init_data;

/** Per-thread data. */
extern pthread_key_t mcbsp_internal_thread_data;

/** Whether mcbsp_internal_init_data is initialised. */
extern bool mcbsp_internal_keys_allocated;

/** 
 * Contorls thread-safe singleton access to mcbsp_internal_init_data,
 * as required for its initialisation.
 */
extern pthread_mutex_t mcbsp_internal_key_mutex;

/**
 * Singleton thread-safe allocator for mcbsp_internal_init_data.
 */
void mcbsp_internal_check_keys_allocated();

/**
 * Entry-point of MulticoreBSP threads. Initialises internals
 * and then executes the user-defined SPMD program.
 */
void* mcbsp_internal_spmd();

/**
 * Checks if a abort has been requested, and if so,
 * exits the current thread.
 */
void mcbsp_internal_check_aborted();

/**
 * Common part executed by all BSP primitives when in SPMD part.
 * This version assumes local thread data used by BSP remains
 * unchanged.
 */ 
const struct mcbsp_thread_data * mcbsp_internal_const_prefunction();

/**
 * Common part executed by all BSP primitives when in SPMD part.
 * This is the non-const version of mcbsp_internal_const_prefunction().
 */
struct mcbsp_thread_data * mcbsp_internal_prefunction();

/** Default SPMD function to call. */
extern int main( int argc, char** argv );

#endif

