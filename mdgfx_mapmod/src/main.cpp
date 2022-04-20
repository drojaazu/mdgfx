#include <getopt.h>

#include "/home/ryou/Projects/lib/endian.hpp"
#include "/home/ryou/Projects/lib/filesys.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "common.hpp"
#include "project.hpp"
#include "tmaputils.hpp"

using namespace std;

void process_args(int argc, char ** argv);
void print_help();

struct RuntimeConfig
{
	optional<u8> pal_line;
	optional<bool> hflip;
	optional<bool> vflip;
	optional<bool> priority;
	optional<s16> chridx_delta;

	u8 width;

	bool in_place;
	bool map_hflip;
	bool map_vflip;
	bool preserve_idx0;

	string in_tmap;
	string out_tmap;

	RuntimeConfig() :
			pal_line(nullopt), hflip(nullopt), vflip(nullopt), priority(nullopt),
			chridx_delta(nullopt), width(0), in_place(false), map_hflip(false),
			map_vflip(false), preserve_idx0(false)
	{
	}
} cfg;

int main(int argc, char ** argv)
{
	try
	{
		process_args(argc, argv);

		// validity checks
		if(cfg.in_tmap.empty())
		{
			cerr << "No source tilemap specified" << endl;
			exit(9);
		}

		if(!cfg.in_place && cfg.out_tmap.empty())
		{
			cerr << "No output specified" << endl;
			exit(8);
		}
		else
		{
			cfg.out_tmap = cfg.in_tmap;
		}

		if(cfg.pal_line.has_value() && cfg.pal_line.value() > 3)
			throw out_of_range("Palette line must be a value between 0 and 3");

		if((cfg.map_hflip || cfg.map_vflip) && cfg.width < 1)
			throw out_of_range(
					"Width must be set when using --map-hflip / --map-vflip");

		auto in_size = stat(cfg.in_tmap).st_size;
		if(in_size % 2 == 1)
			throw runtime_error(
					"Specified input file appears to be invalid (odd number of bytes)");

		if(cfg.width > 0 && (in_size % cfg.width > 0))
			throw out_of_range(
					"Tile count in source file not correct for specified width");

		auto in { ifstream_checked(cfg.in_tmap) };
		vector<u16> map;
		map.reserve(in_size / 2);

		u8 buff[2];
		u16 entry;
		auto endian_copy = endian_copy_func(true);
		while(in.good())
		{
			in.read((char *)buff, 2);
			if(in.good())
			{
				endian_copy(buff, buff + 2, (u8 *)&entry);
				map.push_back(entry);
			}
		}
		in.close();

		auto out { ofstream_checked(cfg.out_tmap) };

		for(auto & entry : map)
		{
			if(cfg.hflip.has_value())
				entry = hflip_flag(entry, cfg.hflip.value());

			if(cfg.vflip.has_value())
				entry = vflip_flag(entry, cfg.vflip.value());

			if(cfg.priority.has_value())
				entry = priority_flag(entry, cfg.priority.value());

			if(cfg.pal_line.has_value())
				entry = set_pal_line(entry, cfg.pal_line.value());

			if(cfg.chridx_delta.has_value() &&
				 (cfg.preserve_idx0 && (entry & TILE_MASK) != 0))
				entry = modify_chridx(entry, cfg.chridx_delta.value());
		}

		if(cfg.map_hflip)
		{
			size_t rows = map.size() / cfg.width;
			vector<u16> row_buff;
			row_buff.reserve(cfg.width);
			for(size_t i = 0; i < rows; ++i)
			{
				auto map_iter = map.begin() + (i * cfg.width);
				reverse_copy(map_iter, map_iter + cfg.width, row_buff.begin());
				copy(row_buff.begin(), row_buff.begin() + cfg.width, map_iter);
			}

			// toggle hflip flag
			for(auto & entry : map)
				entry ^= (1U << HFLIP_BIT);
		}
		else if(cfg.map_vflip)
		{
		}

		for(auto & entry : map)
		{
			endian_copy((u8 *)&entry, ((u8 *)&entry) + 2, buff);
			out.write((char *)buff, 2);
		}

		out.flush();
		out.close();
	}
	catch(exception const & e)
	{
		cerr << "Fatal Error: " << e.what() << endl;
		return -1;
	}
	return 0;
}

void process_args(int argc, char ** argv)
{
	std::vector<option> long_opts {
		{ "hflip", no_argument, nullptr, 'h' },
		{ "no-hflip", no_argument, nullptr, 'H' },
		{ "vflip", no_argument, nullptr, 'v' },
		{ "no-vflip", no_argument, nullptr, 'V' },
		{ "priority", no_argument, nullptr, 'p' },
		{ "no-priority", no_argument, nullptr, 'P' },
		{ "pal-line", required_argument, nullptr, 'l' },
		{ "chridx-delta", required_argument, nullptr, 'c' },
		{ "in-place", no_argument, nullptr, 'i' },
		{ "width", required_argument, nullptr, 'w' },
		{ "map-hflip", no_argument, nullptr, 'm' },
		{ "map-vflip", no_argument, nullptr, 'f' },
		{ "preserve-index-zero", no_argument, nullptr, 'z' },
		{ "output", required_argument, nullptr, 'o' }
	};
	std::string short_opts { "+:hHvVpPl:c:i" };

	while(true)
	{
		const auto this_opt =
				getopt_long(argc, argv, short_opts.data(), long_opts.data(), nullptr);
		if(this_opt == -1)
			break;

		switch(this_opt)
		{
			case 'h':
				cfg.hflip = true;
				break;

			case 'H':
				cfg.hflip = false;
				break;

			case 'v':
				cfg.vflip = true;
				break;

			case 'V':
				cfg.vflip = false;
				break;

			case 'p':
				cfg.priority = true;
				break;

			case 'P':
				cfg.priority = false;
				break;

			case 'l':
				try
				{
					cfg.pal_line = (u8)stoi(optarg);
				}
				catch(const exception & ex)
				{
					cerr << "Invalid argument for palette ID: " << optarg << endl;
					exit(11);
				}
				break;

			case 'c':
				try
				{
					cfg.chridx_delta = (s16)stoi(optarg);
				}
				catch(const exception & ex)
				{
					cerr << "Invalid argument for tile index delta: " << optarg << endl;
					exit(12);
				}
				break;

			case 'i':
				cfg.in_place = true;
				break;

			case 'o':
				cfg.out_tmap = optarg;
				break;

			case 'z':
				cfg.preserve_idx0 = true;
				break;

			case 'w':
				try
				{
					cfg.width = (u16)stoi(optarg);
				}
				catch(const exception & ex)
				{
					cerr << "Invalid argument for tile width: " << optarg << endl;
					exit(13);
				}
				break;

			case 'm':
				cfg.map_hflip = true;
				break;

			case 'f':
				cfg.map_vflip = true;
				break;

			case ':':
				cerr << "Missing argument for option " << to_string(optopt) << endl;
				exit(1);
				break;
			case '?':
				cerr << "Unknown option" << endl;
				exit(2);
		}

		// remaining option should be input
		cfg.in_tmap = argv[optind];
	}
}

void print_help()
{
	std::cout << PROJECT::PROJECT_NAME << " - ver. " << PROJECT::VERSION
						<< std::endl;
}
