#ifndef PUZZLER_OPTION_BUILDER_HPP
#define PUZZLER_OPTION_BUILDER_HPP

#include <cstring>
#include <functional>
#include <string_view>
#include <iostream>
#include <cstdio>

#ifndef APP_NAME
#   define APP_NAME ""
#endif

namespace detail
{
    
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
			auto current_option = std::string_view{ argv[ i++]};
            size_t j = 0;
			while( current_option[ j] == '-' && j <= 2)
                ++j;

            current_option.remove_prefix( j);

            if( j == 0 && mis_handler)
			{
				mis_handler( current_option);
				continue;
			}

			auto equal_to_position = std::find( current_option.rbegin(), current_option.rend(), '=').base();
            std::string key, value;
			if( equal_to_position != current_option.cend())
			{
				key  = std::string( current_option.begin(), std::prev( equal_to_position));
				value = std::string( equal_to_position, current_option.end());
			}
			auto matching = options.find( key);
			if( matching != options.cend())
			{
				auto count = matching->second;
				if( count > 0 && value.empty())
				{
					while( count-- && i < argc)
						matched_options[ key].emplace_back( argv[ i++]);
				}
				else
					matched_options[ key].emplace_back( value.empty() ? key : value);
			}
			else
				mis_handler( current_option);
		}
	}

	std::vector<std::string> get( std::string_view key) const
	{
		auto match = matched_options.find( key.data());
		if( match == matched_options.cend())
		{
			auto o_key = option_pair.find( key);
			if( o_key != option_pair.cend())
				match = matched_options.find( o_key->second.data());
		}

		return match == matched_options.cend() ? std::vector<std::string>{} : match->second;
	}

	std::string asDefault( std::string_view key) const
	{
		auto result = get( key);
		return result.empty() ? "" : result.front();
	}

	auto asBool( std::string_view key) const
	{
        std::string result = asDefault( key).data();
        std::transform( result.begin(), result.end(), result.begin(), ::tolower);
		if( result.empty() || result == "no")
			return false;
		else if( result == "yes")
			return true;
		return false;
	}

	auto asInt( const char *key) const
	{
		auto result = asDefault( key);
		return result.empty() ? 0 : strtol( result.data(), nullptr, 10);
	}

	void addMismatchConsumer( const std::function<void(std::string_view)>& handler)
	{
		mis_handler = handler;
	}

	OptionBuilder& addOption( std::string_view long_key, 
                              std::string_view short_key = {},
							  std::string_view default_value = {}, 
                              std::string_view help_string = {}, 
                              size_t n_args = 1)
	{
		options[ long_key] = n_args;
		long_options.emplace_back( long_key);
		if( !short_key.empty())
		{
			options[ short_key] = n_args;
			option_pair[ long_key] = short_key;
			option_pair[ short_key] = long_key;
			if( !default_value.empty())
				options_default[ short_key] = default_value;
		}
		if( !default_value.empty())
			options_default[ long_key] = default_value;

		if( !help_string.empty())
			options_help[ long_key] = help_string;

		return *this;
	}

	void showHelp() const
	{
		printf( "Usage: %s [OPTIONS...] puzzle-file\n\nOPTIONS:\n", APP_NAME);
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
			auto padding = std::string( alignment - max_hypens - long_option.size() - short_option.size()
										- (!short_option.empty()) * ( max_hypens - 1 + 2), ' ');
			std::cout << padding << help_iter->second <<'\n';
		}
	}

	std::vector<std::string_view> long_options;
	std::unordered_map<std::string_view, size_t> options;
	std::unordered_map<std::string_view, std::string_view> option_pair;
	std::unordered_map<std::string, std::vector<std::string>> matched_options;
	std::unordered_map<std::string_view, std::string_view> options_default;
	std::unordered_map<std::string_view, std::string_view> options_help;
	char **argv;
	int argc;
	std::function<void(std::string_view)> mis_handler;
};
};

#endif //PUZZLER_OPTION_BUILDER_HPP
