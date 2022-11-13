#ifndef PUZZLER_PUZZLE_SIMULATOR_HPP
#define PUZZLER_PUZZLE_SIMULATOR_HPP

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <condition_variable>
#include <iomanip>
#include <climits>
#include <utility>
#include "puzzle-solver.hpp"
#include "option-builder.hpp"

class StateProvider
{
public:
	static StateProvider *instance()
	{
		static StateProvider provider;
		return &provider;
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

	static bool& paused()
	{
		return StateProvider::instance()->is_paused;
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

private:
	StateProvider() = default;
	size_t cols{}, lines{};
	bool is_paused{}, is_resized{};
	std::function<void( bool)> update_callback;
};

enum class Event
{
	Resize,
	Quit,
	Pause,
	NoOp
};

Event watchEvent( size_t ms)
{
	struct timeval timeout = { 0, static_cast<long>( ms * 1000)};
	fd_set rfds;
	FD_ZERO( &rfds);
	FD_SET( STDIN_FILENO, &rfds);
	if( select( STDIN_FILENO + 1, &rfds, nullptr, nullptr, &timeout) > 0 && FD_ISSET( STDIN_FILENO, &rfds))
	{
		int input = std::getchar();
		if( input == 'q')
			return Event::Quit;
		else if( input == 'p')
			return Event::Pause;
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
	virtual void simulate( std::ostream &, bool refresh_run) = 0;

	void replaceSolver( const PuzzleSolver& solver)
	{
		_solver = solver;
		_solver.solve();
	}

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
		longest_size = std::max_element( _solver.matches().cbegin(), _solver.matches().cend(),
                       []( auto& left, auto& right) { return left.word.size() < right.word.size();})
		               ->word.size() + 2;
	}

	void setSimulatorSpeed( int fps )
	{
		m_sim_speed = fps > 0 ? 1000 / fps : m_sim_speed;
	}

	void simulate( std::ostream &strm, bool refresh_run) override
	{
		auto matches = _solver.matches();
		bool fast_forward = false;
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
			std::tie( n_lines, padding) = display( strm);
			rem_lines = (int) StateProvider::getWinLines() - n_lines;
			n_cols = (int) StateProvider::getWinCols() / longest_size;
			word_row = word_col = 0;
		};
		populate();
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
				color = color_selection[ m.word] = 30 + randc();
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
							return;

						fast_forward = false;
					}
				}
				else
				{
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
							last_char = w;
							StateProvider::paused() = true;
							while( StateProvider::paused())
							{
								int input = std::getchar();
								if( input == 'q')
									exit( EXIT_SUCCESS);
								else if( input == 'p')
									StateProvider::paused() = false;
							}
							break;
						}
						case Event::NoOp:
							break;
					}
				}

				m.start = PuzzleSolver::next( m.dmatch )( m.start);
			}
			if( rem_lines > 0)
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
	}

private:
	std::pair<int, int> display( std::ostream& strm)
	{
		auto rows = StateProvider::getWinLines(),
			 cols = StateProvider::getWinCols();
		auto cols_padding = ((int)(cols - 3 * puzzle.size() + 1)) / 2;
		if( 0 > cols_padding || puzzle.front().size() > rows)
		{
			fprintf( stdout, "Puzzle too large for your terminal");
			exit( 1);
		}
		std::string heading( "Puzzle #1");
		strm << std::setw((int)(cols - heading.size()) / 2) << "\x1B[4m" << heading << "\x1B[24m" <<"\n\n";
		auto n_lines = static_cast<int>( 2 + puzzle.size());
		for( auto & makeup : puzzle)
		{
			strm << std::setw( cols_padding);
			if( !_options.asBool( "matches-only"))
			{
				for(std::size_t j = 0, j_size = makeup.size(); j < j_size; ++j )
					strm << makeup[ j] << ( j + 1 == j_size ? "" : "  ");
			}
			strm <<'\n';
		}
		auto remaining_lines = (int)rows - (int)n_lines;
		if( remaining_lines <= 0)
		{
			fprintf( stdout, "Puzzle too large for your terminal");
			exit( 1);
		}

		if( remaining_lines - 4 > 0)
			strm << "\n\n\x1B[4m\x1B[1mFound Words\x1B[24m\x1B[22m:";
		n_lines += 4;
		return { n_lines, cols_padding};
	}

	static int randc()
	{
		static std::random_device dev;
		static std::mt19937_64 gen( dev());
		static std::uniform_int_distribution<int> dist( RED, CYAN);
		return dist( gen);
	}

	std::vector<std::vector<char>> puzzle;
	std::vector<std::string> words;
	std::unordered_map<std::string, int> color_selection;
	size_t last_x_pos{ SIZE_MAX},
		   last_y_pos{ SIZE_MAX},
		   longest_size{};
	char last_char{ CHAR_MAX};
	int m_sim_speed = 1000/2;

};

#endif //PUZZLER_PUZZLE_SIMULATOR_HPP