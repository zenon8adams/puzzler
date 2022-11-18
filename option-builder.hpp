#ifndef PUZZLER_OPTION_BUILDER_HPP
#define PUZZLER_OPTION_BUILDER_HPP

#include <cstring>
#include <functional>

class OptionBuilder
{
public:
	OptionBuilder( int ac, char **av)
		: argv( av), argc( ac)
	{
	}

	void build()
	{
		int i = 1;
		while( i < argc)
		{
			auto *current_option = argv[ i++];
			int hypen_count = 0;
			while( *current_option == '-' && hypen_count <= 2)
			{
				++current_option;
				++hypen_count;
			}
			if( hypen_count == 0 && mis_handler)
			{
				mis_handler( current_option);
				continue;
			}
			auto *equal_to_position = strchr( current_option, '=');
			char *key = current_option, *value{ nullptr};
			if( equal_to_position != nullptr)
			{
				key  = current_option;
				*equal_to_position = '\0';
				value = equal_to_position + 1;
			}
			auto matching = options.find( key);
			if( matching != options.cend())
			{
				auto count = matching->second;
				if( count > 0 && value == nullptr)
				{
					while( count-- && i < argc)
						matched_options[ key].emplace_back( argv[ i++]);
				}
				else
					matched_options[ key].emplace_back( value == nullptr ? key : value);
			}
			else
				mis_handler( current_option);
		}
	}

	std::vector<std::string_view> get( const char *key) const
	{
		auto match = matched_options.find( key);
		if( match == matched_options.cend())
		{
			auto o_key = option_pair.find( key);
			if( o_key != option_pair.cend())
				match = matched_options.find( o_key->second);
		}
		return match == matched_options.cend() ? std::vector<std::string_view>{} : match->second;
	}

	std::string_view asDefault( const char *key) const
	{
		auto result = get( key);
		return result.empty() ? "" : result.front();
	}

	auto asBool( const char *key) const
	{
		auto result = asDefault( key);
		if( result.empty() || strcasestr( result.data(), "no"))
			return false;
		else if( strcasestr( result.data(), "yes"))
			return true;
		return false;
	}

	auto asInt( const char *key) const
	{
		auto result = asDefault( key);
		return result.empty() ? 0 : strtol( result.data(), nullptr, 10);
	}

	void addMismatchConsumer( const std::function<void(const char *)>& handler)
	{
		mis_handler = handler;
	}

	OptionBuilder& addOption( const char *long_key, const char *short_key = nullptr,
							  const char *default_value = nullptr, const char *help_string = nullptr, int n_args = 1)
	{
		options[ long_key] = n_args;
		long_options.emplace_back( long_key);
		if( short_key != nullptr)
		{
			options[ short_key] = n_args;
			option_pair[ long_key] = short_key;
			option_pair[ short_key] = long_key;
			if( default_value != nullptr)
				options_default[ short_key] = default_value;
		}
		if( default_value != nullptr)
			options_default[ long_key] = default_value;

		if( help_string != nullptr)
			options_help[ long_key] = help_string;

		return *this;
	}

	void showHelp() const
	{
		printf( "Usage: %s [OPTIONS...] puzzle-file\n\n", APP_NAME);
		size_t max_long_option_length = 0;
		for( auto& long_option : long_options)
			max_long_option_length = std::max( max_long_option_length, long_option.size());

		size_t max_short_option_length = 0;
		for( auto& option : long_options)
		{
			auto pair_iter = option_pair.find( option);
			if( pair_iter == option_pair.cend())
				continue;

			max_short_option_length = std::max( max_short_option_length, pair_iter->second.size());
		}

		const size_t max_hypens = 2, separation = 3;
		auto alignment = max_long_option_length + max_hypens + max_short_option_length + max_hypens - 1 + separation;
		for( auto& long_option : long_options)
		{
			auto help_iter = options_help.find( long_option);
			if( help_iter == options_help.cend())
				continue;

			std::string_view short_option;
			auto short_option_iter = option_pair.find( long_option);
			if( short_option_iter != option_pair.cend())
				short_option = short_option_iter->second;

			std::cout << std::string( max_hypens, '-') << long_option;
			if( !short_option.empty())
				std::cout << ", " << std::string( max_hypens - 1, '-') << short_option;
			auto padding = std::string( alignment - max_hypens - ( int)long_option.size() - ( int)short_option.size()
										- (!short_option.empty()) * ( max_hypens - 1 + 2), ' ');
			std::cout << padding << help_iter->second <<'\n';
		}
	}

	std::vector<std::string_view> long_options;
	std::unordered_map<std::string_view, size_t> options;
	std::unordered_map<std::string_view, std::string_view> option_pair;
	std::unordered_map<std::string_view, std::vector<std::string_view>> matched_options;
	std::unordered_map<std::string_view, std::string_view> options_default;
	std::unordered_map<std::string_view, std::string_view> options_help;
	char **argv;
	int argc;
	std::function<void(const char *)> mis_handler;
};
#endif //PUZZLER_OPTION_BUILDER_HPP
