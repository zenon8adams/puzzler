#ifndef __UTIL__
#define __UTIL__

#include <ctime>
#include <string>
#include <thread>
#include <chrono>
#include <limits>

#define NEG_INF std::numeric_limits<int>::min()

namespace detail
{

enum class Dir
{
	NL,
	NT, ST, WT, ET,
	NE, SW, NW, SE
};

namespace util
{

inline std::string reversed( const std::string& s )
{
	return { s.crbegin(), s.crend() };
}
}
}

#endif
