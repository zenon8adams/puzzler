#ifndef __UTIL__
#define __UTIL__

#include <ctime>
#include <string>
#include <thread>
#include <chrono>

#define NEG_INF -1000000

enum Dir
{
	NL,
	NT, ST, WT, ET,
	NE, SW, NW, SE
};

void pause( size_t ms )
{
	std::this_thread::sleep_for( std::chrono::milliseconds( ms));
}

std::string reversed( const std::string& s )
{
	return { s.crbegin(), s.crend() };
}

#endif
