#ifndef PUZZLER_OPTION_PARSER_HPP
#define PUZZLER_OPTION_PARSER_HPP

#include <cstring>
#include <functional>

class OptionParser
{
public:
	OptionParser( int ac, char **av)
		: argc( ac), argv( av)
	{
	}

	void extract()
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
			if( hypen_count == 0)
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

	OptionParser& addOption( const char *long_key, int n_args = 0,
					const char *short_key = nullptr, const char *default_value = nullptr)
	{
		options[ long_key] = n_args;
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

		return *this;
	}

	std::unordered_map<std::string_view, size_t> options;
	std::unordered_map<std::string_view, std::string_view> option_pair;
	std::unordered_map<std::string_view, std::vector<std::string_view>> matched_options;
	std::unordered_map<std::string_view, std::string_view> options_default;
	char **argv;
	int argc;
	std::function<void(const char *)> mis_handler;
};
#endif //PUZZLER_OPTION_PARSER_HPP
