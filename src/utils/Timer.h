#pragma once
#include <chrono>


class Timer {
public:
	using clock = std::chrono::high_resolution_clock;
	Timer() : start_(clock::now()) {}
	void reset() { start_ = clock::now(); }
	double elapsed_seconds() const {
		return std::chrono::duration<double>(clock::now() - start_).count();
	}
private:
	clock::time_point start_;
};