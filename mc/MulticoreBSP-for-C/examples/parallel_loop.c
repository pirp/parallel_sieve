
#include "mcbsp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Example problem statement. We use the repeated trapezoidal rule
 * to integrate the function 4*sqrt(1-x^2) from 0 to 1. The answer
 * should equal pi.
 */
unsigned int precision = 100000000;

double f( const double x ) {
	return 4 * sqrt( 1 - x * x );
}

void inner_loop( double *I, const double x ) {
	*I += 2 * f( x );
}

double work() {
	const double h = 1.0 / ( (double)precision );
	double I = f( 0 ) + f( 1 );
	for( unsigned int i = 1; i < precision - 1; ++i )
		inner_loop( &I, i * h );
	return I / (double)(2*precision);
}
/*
 * End example problem statement.
 * void work() contains the parallelisable for-loop.
 */

//sequential code. Uses bsp_begin/end to enable use of bsp_time
void sequential() {
	bsp_begin( 1 );
	const double integral = work();
	printf( "Integral is %.14lf, time taken for sequential calculation: %f\n", integral, bsp_time() );
	bsp_end();
}

//parallel code. We first compute the start and end
//of the loop for each thread, then execute.
void parallel() {
	bsp_begin( bsp_nprocs() );

	//perform the local part of the loop
	const double h      = 1.0 / ( (double)precision );
	double partial_work = 0.0;

	unsigned int start = (unsigned int)  (  bsp_pid()  * precision / (double)bsp_nprocs());
	unsigned int end   = (unsigned int) ((bsp_pid()+1) * precision / (double)bsp_nprocs());

	if( bsp_pid() == 0 ) {
		partial_work += f( 0 );
		start = 1;
	}
	if( bsp_pid() == bsp_nprocs() - 1 ) {
		partial_work += f( 1 );
		end = precision - 1;
	}

	for( unsigned int i = start; i < end; ++i )
		inner_loop( &partial_work, i * h );

	partial_work /= (double)(2*precision);

	//prepare to communicate the partial work
	bsp_push_reg( &partial_work, sizeof( double ) );

	//ensure everyone is done computing their local part
	bsp_sync();

	//we gather all results at thread 0
	if( bsp_pid() == 0 ) {

		//include our own partial loop result in the final answer
		double integral = partial_work;
		//for each other processor...
		for( unsigned int s = 1; s < bsp_nprocs(); ++s ) {
			//get their partial result and store it in place of our own partial result
			bsp_direct_get( s, &partial_work, 0, &partial_work, sizeof( double ) );
			//add their partial result to the final answer
			integral += partial_work;
		}
		//done

		printf( "Integral is %.14lf, time taken for parallel calculation using %d threads: %f\n", integral, bsp_nprocs(), bsp_time() );
	}

	bsp_end();
}

int main( int argc, char **argv ) {

	bsp_init( &sequential, argc, argv );	
	sequential();

	bsp_init( &parallel, argc, argv );
	parallel();

	return EXIT_SUCCESS;
}

