#define _POSIX_C_SOURCE 199309L
#include <cmath>
#include <cstring>
#include <string>
#include <algorithm>
#include <deque>
#include <cstdlib>
#include <csignal>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "puzzle-simulator.hpp"
#include "option-builder.hpp"

#define NOT_SET  nullptr

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

	fprintf( stderr, "\x1B[?25h\x1B[2J\x1B[0;0H"); // Clear screen
	tcsetattr( STDIN_FILENO, TCSANOW, &orig_term_state);

	// Turn-off focus control
	printf( "\x1B[?1004l");

	// exit() must not be called here to avoid infinite loop
	_Exit( signal);
}

template<typename Pred, typename First, typename... Others>
bool compareAnd( First base, Others... others)
{
	return ( ... && Pred()( base, others));
}

int main( int argc, char *argv[])
{
	const char *puzzle_file = NOT_SET;
	OptionBuilder builder( argc, argv);
	builder.addMismatchConsumer([ &]( const char *option)
	{
		puzzle_file = option;
	});

	builder.addOption("help", "h", NOT_SET, "Show this page.")
		   .addOption("speed", "s", "1", "Set the simulation speed for the solver.")
		   .addOption( "file", "f", "Set the file containing the puzzle.")
		   .addOption( "matches-only", "only", "no", "Display matched words only.")
		   .addOption( "predictable", "p", "no", "Randomize the puzzle solution on every run.")
		   .addOption( "wrap", "w", "yes",
					   "The forward and rewind button switches to first and last on reaching the end.")
		   .addOption( "auto-next", "a", "no", "Press `next` before next puzzle is run.")
		   .addOption( "reverse-solve", "r", "no", "Reverse the effect of forward and rewind button.")
		   .build();

	if( !builder.asDefault( "help").empty())
	{
		builder.showHelp();
		exit( EXIT_SUCCESS);
	}

	auto maybe_file = builder.asDefault( "file");
	if( !maybe_file.empty())
		puzzle_file = maybe_file.data();

	if( puzzle_file == nullptr)
	{
		builder.showHelp();
		exit( EXIT_FAILURE);
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
	sigaction( SIGWINCH, &resize_action, NOT_SET);
	raise( SIGWINCH);   // Get current window size.
	// If $LINES and $COLUMNS is non-zero, terminal supports cursor motion.
	if(StateProvider::getWinLines() && StateProvider::getWinCols())
	{
		struct sigaction refresh_action{};
		refresh_action.sa_handler = refresh_before_exit;
		sigaction( SIGINT, &refresh_action, NOT_SET);
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

		// Turn-on focus control
		printf( "\x1B[?1004h");

		auto step = builder.asBool( "reverse-solve") ? -1 : 1;
		std::vector<std::unique_ptr<TerminalPuzzleSimulator>> sims( response.size());
		auto g_begin = step == -1 ? --response.cend() : response.cbegin(),
			 g_end   = step == -1 ? --response.begin() : response.cend();
		for( auto begin = g_begin, end = g_end; begin != end;)
		{
			auto puzzle_number = static_cast<int>( std::distance( response.cbegin(), begin) + 1);
			PuzzleSolver solver( begin->puzzle, begin->keys);
			if( !sims[ puzzle_number - 1])
				sims[ puzzle_number - 1] = std::make_unique<TerminalPuzzleSimulator>( solver, builder);

			auto& term_simulator = sims[ puzzle_number - 1];
			term_simulator->setSimulatorSpeed((int)builder.asInt("speed"));
			StateProvider::registerWinUpdateCallback( [&term_simulator, &puzzle_number]( bool refresh)
			                                          {
				                                          term_simulator->simulate( std::cout, puzzle_number, refresh);
			                                          });
			// Returns indication that this run completed.
			auto status = term_simulator->simulate( std::cout, puzzle_number, false);
			printf("\x1B[?25l");
			int input{};
			while( status == Conclusion::Finished  && !builder.asBool( "auto-next") &&
			       compareAnd<std::not_equal_to<char>>( input = std::getchar(),
				   Q( KEY_QUIT), Q( KEY_RESTART), Q( KEY_NEXT), Q( KEY_REWIND)))
				;
			if( status == Conclusion::Rewind || input == Q( KEY_REWIND))
			{
				begin = begin != g_begin ? begin - step
						: builder.asBool( "wrap") ? g_end - step : begin;
				continue;
			}
			else if( input == Q( KEY_RESTART))
				continue;
			else if( input == Q( KEY_QUIT))
				exit( EXIT_SUCCESS);

			begin = begin + step == g_end ? ( builder.asBool( "wrap") ? g_begin : begin) : begin + step;
		}
	}
	else
	{
		fprintf( stderr, "Terminal not supported!");
		std::getchar();
		exit( 1);
	}
}
