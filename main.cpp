#define __USE_POSIX199309
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
#include "option-builder.hpp"

static void resize_handler( int signal)
{
	if( SIGWINCH != signal)
		return;
	struct winsize window_size{};
	ioctl(0, TIOCGWINSZ, &window_size);
	auto w_instance = StateProvider::instance();
	w_instance->setWinSize(window_size.ws_row, window_size.ws_col);
}

static struct termios orig_term_state;
static void refresh_before_exit( int signal)
{
	if( orig_term_state.c_iflag == 0)
		return;

	printf("\x1B[?25h\x1B[2J"); // Clear screen
	tcsetattr( STDIN_FILENO, TCSANOW, &orig_term_state);

	// exit() must not be called here to avoid infinite loop
	_Exit( signal);
}

int main( int argc, char *argv[])
{
	const char *puzzle_file = nullptr;
	OptionBuilder builder( argc, argv);
	builder.addMismatchConsumer([&](const char *option)
	{
		puzzle_file = option;
	});
	builder.addOption( "speed", "s", "1")
		   .addOption( "file", "f")
		   .addOption( "matches-only", "only", "no")
		   .addOption( "predictable", "p", "no")
		   .build();

	auto maybe_file = builder.asDefault("file");
	if( !maybe_file.empty())
		puzzle_file = maybe_file.data();

	if( puzzle_file == nullptr)
	{
		//TODO: Show help
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

	struct sigaction resize_action{};
	resize_action.sa_handler = resize_handler;
	sigaction( SIGWINCH, &resize_action, nullptr);
	raise( SIGWINCH);   // Get current window size.
	// If $LINES and $COLUMNS is non-zero, terminal supports cursor motion.
	if(StateProvider::getWinLines() && StateProvider::getWinCols())
	{
		struct sigaction refresh_action{};
		refresh_action.sa_handler = refresh_before_exit;
		sigaction( SIGINT, &refresh_action, nullptr);
		atexit( [] { refresh_before_exit( EXIT_SUCCESS);});

		struct termios raw_term_state{};
		tcgetattr( STDIN_FILENO, &orig_term_state);
		raw_term_state.c_iflag       = ICRNL | IUTF8;
		raw_term_state.c_oflag       = OPOST | OFILL | ONLCR | NL0;
		raw_term_state.c_lflag       = ISIG;
		raw_term_state.c_cflag       = CS8 | CREAD;
		raw_term_state.c_ispeed      = B9600;
		raw_term_state.c_ospeed      = B9600;
		raw_term_state.c_cc[ VINTR]  = 003;
		raw_term_state.c_cc[ VSUSP]  = 032;
		raw_term_state.c_cc[ VEOF]   = 004;
		raw_term_state.c_cc[ VMIN]   = 1;
		raw_term_state.c_cc[ VTIME]  = 0;
		raw_term_state.c_cc[ VERASE] = 0177;

		tcsetattr( STDIN_FILENO, TCSANOW, &raw_term_state);
		PuzzleSolver solver( response.front().puzzle, response.front().keys);
		TerminalPuzzleSimulator term_simulator( solver, builder);
		term_simulator.setSimulatorSpeed((int)builder.asInt("speed"));
		StateProvider::registerWinUpdateCallback( [&term_simulator]( bool refresh)
		{
			term_simulator.simulate( std::cout, refresh);
		});
		term_simulator.simulate( std::cout, false);
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
