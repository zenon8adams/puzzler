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

std::string reversed( const std::string& s )
{
	return { s.crbegin(), s.crend() };
}

#endif
