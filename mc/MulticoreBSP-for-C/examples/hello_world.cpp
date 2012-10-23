
#include "mcbsp.hpp"

#include <cstdlib>
#include <iostream>

using namespace mcbsp;

class Hello_World: public BSP_program {

	protected:

		virtual void spmd() {
			std::cout << "Hello world from thread " << bsp_pid() << std::endl;
		}

		virtual BSP_program * newInstance() {
			return new Hello_World();
		}

	public:

		Hello_World() {}
};

int main() {
	BSP_program *p = new Hello_World();
	p->begin( 2 );
	p->begin();
	delete p;
	return EXIT_SUCCESS;
}

