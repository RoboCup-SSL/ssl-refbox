#ifndef TIMING_H
#define TIMING_H

#include <chrono>
#include <cstdint>

class MicrosecondCounter {
	public:
		MicrosecondCounter();
		uint32_t read_and_reset();

	private:
		std::chrono::high_resolution_clock::time_point start_time;
};

#endif

