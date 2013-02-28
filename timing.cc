#include "timing.h"

MicrosecondCounter::MicrosecondCounter() : start_time(std::chrono::high_resolution_clock::now()) {
}

uint32_t MicrosecondCounter::read_and_reset() {
	// This computation is not exactly the same as just resetting start_time to now.
	// In the case of a clock with better-than-microsecond resolution, this computation leaves the fractional microseconds of difference in the new value of now - start_time.
	// This prevents error from accumulating.
	std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
	std::chrono::microseconds diff = now - start_time;
	start_time += diff;
	return static_cast<uint32_t>(diff.count());
}

