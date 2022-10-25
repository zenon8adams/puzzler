#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <algorithm>
#include <deque>
#include "puzzle-solver.hpp"

int main( int argc, char *argv[])
{
	if( argc != 2)
	{
		fprintf( stderr, "Usage: %s puzzle-file\n", *argv);
		exit( 1);
	}

	auto scope = std::ifstream( *++argv);
	if( !scope)
	{
		fprintf( stderr, "Invalid file!");
		exit( 1);
	}

	PuzzleFileReader reader( scope);
	auto response = reader.getPuzzles();
	if( response.empty())
	{
		fprintf( stderr, "Invalid file!");
		exit( 1);
	}

	PuzzleSolver solver( response.front().puzzle, response.front().keys);
	TerminalPuzzleSimulator term_simulator( solver);
	term_simulator.setSimulatorSpeed( 5);
	term_simulator.simulate( std::cout);
}
