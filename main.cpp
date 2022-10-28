#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <algorithm>
#include <deque>
#include <csignal>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "puzzle-simulator.hpp"
#include "option_parser.hpp"

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
	const char *puzzle_file = nullptr;
	OptionParser parser( argc, argv);
	parser.addMismatchConsumer([&]( const char *option)
	{
		puzzle_file = option;
	});
	parser.addOption( "speed", 1, "s", "1")
		  .addOption( "file", 1, "f")
		  .addOption( "matches-only", 1, "only", "no")
		  .addOption( "predictable", 1, "p", "no")
		  .extract();

	auto maybe_file = parser.asDefault( "file");
	if( !maybe_file.empty())
		puzzle_file = maybe_file.data();

	if( puzzle_file == nullptr)
	{
		fprintf( stderr, "Usage: %s puzzle-file\n", *argv);
		exit( 1);
	}

	auto scope = std::ifstream( puzzle_file);
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

	signal( SIGWINCH, resize_handler);
	raise( SIGWINCH);   // Get current window size.
	// If $LINES and $COLUMNS is non-zero, terminal supports cursor motion.
	if( WindowSizeProvider::getLines() && WindowSizeProvider::getCols())
	{
		struct termios orig_term_state{}, raw_term_state{};
		PuzzleSolver solver( response.front().puzzle, response.front().keys);
		TerminalPuzzleSimulator term_simulator( solver, parser);
		term_simulator.setSimulatorSpeed((int)parser.asInt( "speed"));
		term_simulator.simulate( std::cout);
		tcgetattr( STDIN_FILENO, &orig_term_state);
		cfmakeraw( &raw_term_state);
		tcsetattr( STDIN_FILENO, TCSANOW, &raw_term_state);
		printf("\x1B[?25l");
		while( std::getchar() != 'q')
			;
		printf("\x1B[?25h\x1B[2J"); // Clear screen
		tcsetattr( STDIN_FILENO, TCSANOW, &orig_term_state);
	}
	else
	{
		fprintf( stderr, "Terminal not supported!");
		exit( 1);
	}
}
