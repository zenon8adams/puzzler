#ifndef PUZZLER_PUZZLE_SIMULATOR_HPP
#define PUZZLER_PUZZLE_SIMULATOR_HPP

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <condition_variable>
#include <iomanip>
#include <climits>
#include <utility>
#include <list>
#include "puzzle-solver.hpp"
#include "option-builder.hpp"

#define STRINGIFY_IMPL( cmd) #cmd
#define STRINGIFY( cmd) STRINGIFY_IMPL( cmd)
#define Q( value) ( *(STRINGIFY( value)))

#define KEY_QUIT     q
#define KEY_PAUSE    p
#define KEY_RESTART  r
#define KEY_NEXT     n
#define KEY_PREVIOUS   b



class StateProvider
{
public:
	static StateProvider *instance( int index = 0)
	{
		static std::list<StateProvider> providers;
		if( providers.size() <= index)
			providers.emplace_back( StateProvider());
		return ( index == 0 ? providers.begin() : std::next( providers.begin(), index)).operator->();
	}

	void setWinSize( size_t v_rows, size_t v_cols)
	{
		static bool initial_run = true;
		cols = v_cols;
		lines = v_rows;
		if( !initial_run)
			StateProvider::resized() = true;

		if( update_callback)
			update_callback( true);

		initial_run = false;
	}

	static void registerWinUpdateCallback( const std::function<void( bool)>& cb)
	{
	   StateProvider::instance()->update_callback = cb;
	}

	bool &paused()
	{
		return is_paused;
	}

	static bool& resized()
	{
		return StateProvider::instance()->is_resized;
	}

	static size_t getWinLines()
	{
		return StateProvider::instance()->lines;
	}

	static size_t getWinCols()
	{
		return StateProvider::instance()->cols;
	}

	static bool& firstFocus()
	{
		return StateProvider::instance()->is_first_focus;
	}

private:
	StateProvider() = default;
	size_t cols{}, lines{};
	bool is_paused{}, is_resized{},
	     is_first_focus{ true};
	std::function<void( bool)> update_callback;
};

enum class  Event
{
	Resize,
	Quit,
	Pause,
	Restart,
	Focus,
	Next,
	Previous,
	NoOp
};

enum class Conclusion
{
	Finished,
	Rewind,
	Forward
};

bool isReady( size_t ms)
{
	struct timeval timeout = { 0, static_cast<long>( ms * 1000)};
	fd_set rfds;
	FD_ZERO( &rfds);
	FD_SET( STDIN_FILENO, &rfds);
	if( select( STDIN_FILENO + 1, &rfds, nullptr, nullptr, ms == 0 ? nullptr : &timeout) > 0
		&& FD_ISSET( STDIN_FILENO, &rfds))
		return true;

	return false;
}

Event watchEvent( size_t ms)
{
	if( isReady( ms))
	{
		int input = std::getchar();
		if( input == Q( KEY_QUIT))
			return Event::Quit;
		else if( input == Q( KEY_PAUSE))
			return Event::Pause;
		else if( input == Q( KEY_RESTART))
			return Event::Restart;
		else if( input == '\x1B')
			return Event::Focus;
		else if( input == Q( KEY_NEXT))
			return Event::Next;
		else if(input == Q( KEY_PREVIOUS))
			return Event::Previous;
	}
	else if( StateProvider::resized())
	{
		StateProvider::resized() = false;
		return Event::Resize;
	}

	return Event::NoOp;
}

class PuzzleSimulator
{
public:
	virtual Conclusion simulate( std::ostream &, int puzzle_number, bool refresh_run) = 0;
	virtual ~PuzzleSimulator() = default;

protected:
	explicit PuzzleSimulator(PuzzleSolver solver, OptionBuilder  options)
		: _solver( std::move( solver)), _options( std::move( options))
	{
		_solver.solve();
	}

	PuzzleSolver _solver;
	OptionBuilder _options;
};

class TerminalPuzzleSimulator: public PuzzleSimulator
{
public:
	explicit TerminalPuzzleSimulator( const PuzzleSolver& solver, const OptionBuilder& options)
		: PuzzleSimulator( solver, options)
	{
		auto clone = _solver.puzzle();
		puzzle.resize( clone.size());
		std::transform( clone.cbegin(), clone.cend(), puzzle.begin(),
		                []( const auto& str)
		                {
			                std::vector<char> inner( str.size());
			                std::copy( str.cbegin(), str.cend(), inner.begin());
			                return inner;
		                });
		words = _solver.words();
		longest_size = static_cast<int>( std::max_element( _solver.matches().cbegin(), _solver.matches().cend(),
                       []( auto& left, auto& right) { return left.word.size() < right.word.size();})
		               ->word.size()) + 2;
	}

	void setSimulatorSpeed( int fps )
	{
		m_sim_speed = fps > 0 ? 1000 / fps : m_sim_speed;
	}

	Conclusion simulate( std::ostream &strm, int puzzle_number, bool refresh_run) override
	{
		auto state_provider = StateProvider::instance( puzzle_number - 1);
		auto matches = _solver.matches();
		std::vector<PuzzleSolver::underlying_type> base_order( matches.size());
		std::copy( matches.cbegin(), matches.cend(), base_order.begin());
		// Add a bit of un-determinism in the selection order
		if( !_options.asBool( "predictable"))
			std::shuffle( base_order.begin(), base_order.end(), std::random_device());
		std::vector<PuzzleSolver::underlying_type> soln( base_order.size());
		std::copy( base_order.cbegin(), base_order.cend(), soln.begin());

		int n_lines, padding, rem_lines, word_row, word_col;
		size_t n_cols;
		auto populate = [&]()
		{
			printf("\x1B[2J\x1B[H");    // Clear screen and move cursor to origin
			std::tie( n_lines, padding) = display( strm, puzzle_number);
			rem_lines = (int) StateProvider::getWinLines() - n_lines;
			n_cols = (int) StateProvider::getWinCols() / longest_size;
			word_row = word_col = 0;
		};
		populate();

		auto restart = [&]( auto& b_soln, auto& reset)
		{
			reset = true;
			last_x_pos = last_y_pos = NEG_INF;
			last_char = CHAR_MAX;
			std::copy( base_order.cbegin(), base_order.cend(), soln.begin());
			b_soln = soln.begin();
			populate();
		};

		auto freeze = [&]( auto& b_soln, auto& reset)
		{
			while( state_provider->paused())
			{
				int input = std::getchar();
				if( input == Q( KEY_QUIT))
					exit( EXIT_SUCCESS);
				else if( input == Q( KEY_PAUSE))
					state_provider->paused() = false;
				else if( input == Q( KEY_RESTART))
				{
					state_provider->paused() = false;
					restart( b_soln, reset);
				}
				else if( input == Q( KEY_NEXT) || input == Q( KEY_PREVIOUS))
				{
					fast_forward = state_provider->paused();
					return input == Q( KEY_NEXT) ? Conclusion::Forward : Conclusion::Rewind;
				}
			}

			return Conclusion::Finished;
		};

		// Disable buffering in terminal
		setvbuf( stdout, nullptr, _IONBF, 0);
		// Save current cursor position
		printf( "\x1B[s");
		bool reset;
		for( auto b_soln = soln.begin(), e_soln = soln.end(); b_soln != e_soln; reset ? b_soln : ++b_soln)
		{
			auto m = *b_soln;
			int color;  // Generate random color.
			if( !color_selection[ m.word])
				color = color_selection[ m.word] = 30 + TerminalPuzzleSimulator::random_color();
			else
				color = color_selection[ m.word];
			reset = false;
			for( auto b_w = b_soln->word.begin(), e_w = b_soln->word.end(); !reset && b_w != e_w; ++b_w)
			{
				auto w = *b_w;
				printf( "\x1B[%d;%dH\x1B[%dm%c\x1B[0m\x1B", m.start.x + 1 + 2, 3 * m.start.y + padding,
				        color, puzzle[ m.start.x][ m.start.y]);
				if( fast_forward || refresh_run)
				{
					if( last_char ==  w && last_x_pos == m.start.x && last_y_pos == m.start.y)
					{
						if( refresh_run)
							return Conclusion::Rewind;

						fast_forward = false;
					}
				}
				else
				{
					{
						auto result = freeze( b_soln, reset);
						if ( result != Conclusion::Finished)
							return result;
					}
					switch( watchEvent( m_sim_speed))
					{
						case Event::Resize:
							fast_forward = reset = true;
							last_x_pos = m.start.x;
							last_y_pos = m.start.y;
							last_char = w;
							std::copy( base_order.cbegin(), base_order.cend(), soln.begin());
							b_soln = soln.begin();
							populate();
							continue;
						case Event::Quit:
							exit( EXIT_SUCCESS);
						case Event::Pause:
						{
							last_x_pos = m.start.x;
							last_y_pos = m.start.y;
							last_char  = w;
							state_provider->paused() = true;
							auto result = freeze( b_soln, reset);
							if( result != Conclusion::Finished)
								return result;
							break;
						}
						case Event::Restart:
							restart( b_soln, reset);
							continue;
						case Event::Focus:
						{
							char remaining[ 3]{};
							// The first focus is a false trigger. Discard it.
							if( !StateProvider::firstFocus() && read( STDIN_FILENO, remaining, 2) == 2)
							{
								if( strcmp( remaining, "[I") == 0)
									state_provider->paused() = false;
								else if( strcmp( remaining, "[O") == 0)
								{
									last_x_pos = m.start.x;
									last_y_pos = m.start.y;
									last_char  = w;
									state_provider->paused() = true;
									freeze( b_soln, reset);
								}
							}
							StateProvider::firstFocus() = false;
							break;
						}
						case Event::Next:
							return Conclusion::Forward;
						case Event::Previous:
							return Conclusion::Rewind;
						case Event::NoOp:
							break;
					}
				}

				m.start = PuzzleSolver::next( m.dmatch )( m.start);
			}
			if( rem_lines > 0 && !reset)
			{
				// Display search complete indicator for word.
				auto current_word = ( m.reversed ? reversed( m.word) : m.word)
					.append( std::string( longest_size - m.word.size(), ' '));
				printf( "\x1B[%d;%zuH\x1B[%dm%s", n_lines + (word_row % rem_lines),
				        (( word_row > 0 && ( word_row % rem_lines == 0) ? ++word_col
				         : word_col) % n_cols) * longest_size, color, current_word.c_str());
				++word_row;
			}
		}
		// We are done! Restore cursor position
		printf("\x1B[u");

		return Conclusion::Finished;
	}

private:

#if defined( __GNUC__) || defined( __clang__)
#define popcount8( x) __builtin_popcount( x)
#else
	#define popcount8(x)                                \
        do {                                            \
              uint8_t v = x;                            \
              v = ( v & 0x55) << 1 | (( v >> 1) & 0x55); \
              v = ( v & 0x33) << 1 | (( v >> 1) & 0x33); \
              v = ( v & 0x0F) << 1 | (( v >> 1) & 0x0F); \
        }while( 0);
#endif

	static int byteCount( uint8_t c)
	{
		// Check if leading byte is an ASCII character
		if(( c & 0x80) != 0x80)
			return 1;
		// If not, reverse the byte
		c =   ( c & 0x55) << 1 | ( c & 0xAA) >> 1;
		c =   ( c & 0x33) << 2 | ( c & 0xCC) >> 2;
		c = (( c & 0x0F) << 4  | ( c & 0xFF) >> 4);
		// Mask out the continuous runs of ones in the leading byte( now trailing)
		c = ( c ^ ( c + 1)) >> 1;
		// Count the remaining bits in the byte.
		return popcount8( c);
	}

	static size_t mb_strsize( const char *ps)
	{
		size_t iters = 0;
		while( *ps)
		{
			ps += byteCount( *ps);
			++iters;
		}

		return iters;
	}

	std::pair<int, int> display( std::ostream& strm, int puzzle_number = 1)
	{
		auto rows = StateProvider::getWinLines(),
			 cols = StateProvider::getWinCols();
		auto cols_padding = ((int)(cols - 3 * puzzle.size() + 1)) / 2;
		if( 0 > cols_padding || puzzle.front().size() > rows)
			panic_exit();

		std::string heading( "Puzzle #" + std::to_string( puzzle_number));
		strm << std::setw((int)(cols - heading.size()) / 2)
			 << "\x1B[4m" << heading << "\x1B[24m" <<"\n\n";
		auto n_lines = static_cast<int>( 2 + puzzle.size());
		std::array control_info = {
			"╭──────────────────────╮",
			"│                      │",
			"│        Controls      │",
			"│                      │",
			"├───────────┬──────────┤",
			"│     " STRINGIFY( KEY_QUIT) "     │     Quit │",
			"├───────────┼──────────┤",
			"│     " STRINGIFY( KEY_RESTART) "     │  Restart │",
			"├───────────┼──────────┤",
			"│     " STRINGIFY( KEY_NEXT) "     │     Next │",
			"├───────────┼──────────┤",
			"│     " STRINGIFY( KEY_PREVIOUS) "     │ Previous │",
			"├───────────┼──────────┤",
			"│     " STRINGIFY( KEY_PAUSE) "     │    Pause │",
			"╰───────────┴──────────╯"
		};

		for( auto & makeup : puzzle)
		{
			strm << std::setw( cols_padding);
			if( !_options.asBool( "matches-only"))
			{
				for( std::size_t j = 0, j_size = makeup.size(); j < j_size; ++j )
					strm << makeup[ j] << ( j + 1 == j_size ? "" : "  ");
			}
			strm <<'\n';
		}

		auto max_text_size = static_cast<int>( mb_strsize( control_info.front()));
		if( max_text_size < cols_padding && control_info.size() < ( puzzle.size() + 4))
		{
			auto v_align = ( 4 + static_cast<int>( puzzle.size()) - static_cast<int>( control_info.size())) / 2,
				 h_align = ( cols_padding - max_text_size) / 2;
			for( size_t i = 0; i < control_info.size(); ++i)
				printf( "\x1B[%lu;%dH%s", v_align + i, h_align, control_info[ i]);
			printf( "\x1B[%d;%dH", n_lines + 1, 0);
		}

		auto remaining_lines = (int)rows - (int)n_lines;
		if( remaining_lines <= 0)
			panic_exit();

		if( remaining_lines - 4 > 0)
			strm << "\n\n\x1B[4m\x1B[1mFound Words\x1B[24m\x1B[22m:";
		n_lines += 4;
		return { n_lines, cols_padding};
	}

	static void panic_exit()
	{
		const auto win_width = StateProvider::getWinCols();
		const auto half_win_height = StateProvider::getWinLines() / 2;
		constexpr std::string_view main_message = "Puzzle too large for your terminal",
								   exit_message = "Press ENTER to exit";
		fprintf( stderr, "\x1B[?25l\x1B[%ld;%ldH\x1B[31m%s\x1B[0m", half_win_height,
				 ( win_width - main_message.size()) / 2, main_message.data());
		fprintf( stderr, "\x1B[%ld;%ldH\x1B[31m%s\x1B[0m", half_win_height + 1,
		         ( win_width - exit_message.size()) / 2, exit_message.data());
		while( getchar() != '\n')
			;
		exit( EXIT_FAILURE);
	}

	static int random_color()
	{
		static std::random_device dev;
		static std::mt19937_64 gen( dev());
		static std::uniform_int_distribution<int> dist( RED, CYAN);
		return dist( gen);
	}

	std::vector<std::vector<char>> puzzle;
	std::vector<std::string> words;
	std::unordered_map<std::string, int> color_selection;
	int last_x_pos{ NEG_INF},
		last_y_pos{ NEG_INF},
		longest_size{};
	bool fast_forward{};
	char last_char{ CHAR_MAX};
	int m_sim_speed = 1000/2;

};

#endif //PUZZLER_PUZZLE_SIMULATOR_HPP