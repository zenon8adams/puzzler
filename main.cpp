#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <algorithm>
#include <deque>
#include <csignal>
#include <sys/ioctl.h>
#include "puzzle-simulator.hpp"

static void resize_handler( int signal)
{
	if( SIGWINCH != signal)
		return;
	struct winsize window_size{};
	ioctl(0, TIOCGWINSZ, &window_size);
	auto w_instance = WindowSizeProvider::instance();
	w_instance->setNewSize( window_size.ws_row, window_size.ws_col);
}

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
	signal( SIGWINCH, resize_handler);
	raise( SIGWINCH);   // Get current window size.
	// If $LINES and $COLUMNS is non-zero, terminal supports cursor motion.
	if( WindowSizeProvider::getLines() && WindowSizeProvider::getCols())
	{
		term_simulator.setSimulatorSpeed( 3);
		term_simulator.simulate( std::cout);
		std::getchar();
		printf("\x1B[2J"); // Clear screen
	}
	else
	{
		fprintf( stderr, "Terminal not supported!");
		exit( 1);
	}
}
