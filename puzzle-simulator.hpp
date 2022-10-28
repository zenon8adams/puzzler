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
#include "option_parser.hpp"

class WindowSizeProvider
{
	size_t cols{}, lines{};
public:
	static WindowSizeProvider *instance()
	{
		static WindowSizeProvider provider;
		return &provider;
	}

	void setNewSize( size_t v_rows, size_t v_cols)
	{
		std::unique_lock<std::mutex> lock( mtx_resource);
		cols = v_cols;
		lines = v_rows;
		resized = true;
		watcher.notify_one();
	}

	static size_t getLines()
	{
		return WindowSizeProvider::instance()->lines;
	}

	static size_t getCols()
	{
		return WindowSizeProvider::instance()->cols;
	}

	std::mutex mtx_resource;
	std::condition_variable watcher;
	bool resized{};
private:
	WindowSizeProvider() = default;
};

bool hasWindowResized( size_t ms)
{
	auto instance = WindowSizeProvider::instance();
	auto lock = std::unique_lock<std::mutex>( instance->mtx_resource);
	auto status = instance->watcher.wait_for( lock, std::chrono::milliseconds( ms), [&instance]()
	{
		return instance->resized;
	});
	if( status)
		instance->resized = false;
	return status;
}

class PuzzleSimulator
{
public:
	virtual void simulate( std::ostream&) = 0;

	void replaceSolver( const PuzzleSolver& solver)
	{
		_solver = solver;
		_solver.solve();
	}

	virtual ~PuzzleSimulator() = default;

protected:
	explicit PuzzleSimulator( PuzzleSolver solver, OptionParser  options)
		: _solver( std::move( solver)), _options( std::move( options))
	{
		_solver.solve();
	}

	PuzzleSolver _solver;
	OptionParser _options;
};

class TerminalPuzzleSimulator: public PuzzleSimulator
{
public:
	explicit TerminalPuzzleSimulator( const PuzzleSolver& solver, const OptionParser& options)
		: PuzzleSimulator( solver, options)
	{
	}

	void setSimulatorSpeed( int fps )
	{
		m_sim_speed = fps > 0 ? 1000 / fps : m_sim_speed;
	}

	void simulate( std::ostream& strm) override
	{
		auto clone = _solver.puzzle();
		std::vector<std::vector<char>> puzzle( clone.size());
		std::transform( clone.cbegin(), clone.cend(), puzzle.begin(),
		               []( const auto& str )
		                {
			                std::vector<char> inner;
			                for( const auto& elm : str )
				                inner.emplace_back(elm);
			                return inner;
		                } );
		auto words  = _solver.words();
		auto matches = _solver.matches();
		auto longest_size = std::max_element( matches.cbegin(), matches.cend(),
						    []( auto& left, auto& right) { return left.word.size() < right.word.size();})
							->word.size() + 2;
		int last_x_pos{-1}, last_y_pos{-1};
		char last_char{CHAR_MAX};
		bool fast_forward = false;
		std::unordered_map<std::string, int> color_selection;

		std::vector<PuzzleSolver::underlying_type> base_order( matches.size());
		std::copy( matches.cbegin(), matches.cend(), base_order.begin());
		// Add a bit of un-determinism in the selection order
		if( !_options.asBool( "predictable"))
			std::shuffle( base_order.begin(), base_order.end(), std::random_device());
		reset:
		std::vector<PuzzleSolver::underlying_type> soln( base_order.size());
		std::copy( base_order.cbegin(), base_order.cend(), soln.begin());
		printf("\x1B[2J\x1B[H");    // Clear screen and move cursor to origin
		auto adjustments = display( puzzle, words, strm);
		auto n_lines = adjustments.first,
			 padding = adjustments.second;
		auto rem_lines = (int)WindowSizeProvider::getLines() - n_lines;
		auto word_row = 0, word_col = 0;
		auto n_cols = (int)WindowSizeProvider::getCols() / longest_size;
		// Disable buffering in terminal
		setvbuf(stdout, nullptr, _IONBF, 0);
		// Save current cursor position
		printf( "\x1B[s");
		for( auto& m : soln )
		{
			int color;  // Generate random color.
			if( !color_selection[ m.word])
				color = color_selection[ m.word] = 30 + randc();
			else
				color = color_selection[ m.word];
			for( const auto& w : m.word )
			{
				printf( "\x1B[%d;%dH\x1B[%dm%c\x1B[0m\x1B", m.start.x + 1 + 2, 3 * m.start.y + padding,
				        color, puzzle[ m.start.x][ m.start.y]);
				if( fast_forward)
				{
					if( last_char ==  w && last_x_pos == m.start.x && last_y_pos == m.start.y)
						fast_forward = false;
				}
				else if( hasWindowResized( m_sim_speed))
				{
					fast_forward = true;
					last_x_pos = m.start.x;
					last_y_pos = m.start.y;
					last_char  = w;
					goto reset;
				}

				m.start = PuzzleSolver::next( m.dmatch )( m.start );
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
	template <typename PuzzleContainer, typename WordContainer>
	std::pair<int, int> display( const PuzzleContainer& puzzle, const WordContainer& words, std::ostream& strm )
	{
		auto rows = WindowSizeProvider::getLines(),
			 cols = WindowSizeProvider::getCols();
		auto cols_padding = ((int)(cols - 3 * puzzle.size() + 1)) / 2;
		if( 0 > cols_padding || puzzle.front().size() > rows)
		{
			fprintf( stdout, "Puzzle too large for your terminal");
			exit( 1);
		}
		std::string heading( "Puzzle #1");
		strm << std::setw((int)(cols - heading.size()) / 2) << "\x1B[4m" << heading << "\x1B[24m" <<"\n\n";
		int n_lines = 2 + puzzle.size();
		for( std::size_t i = 0, i_size = puzzle.size(); i < i_size; ++i )
		{
			strm << std::setw( cols_padding);
			if( !_options.asBool( "matches-only"))
			{
				for( std::size_t j = 0, j_size = puzzle[ i].size(); j < j_size; ++j )
					strm << puzzle[ i][ j] << ( j + 1 == j_size ? "" : "  ");
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

	int m_sim_speed = 1000/2;
};

#endif //PUZZLER_PUZZLE_SIMULATOR_HPP