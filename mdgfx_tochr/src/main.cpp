#include <getopt.h>

#include "filesys.hpp"
#include <chrgfx/chrgfx.hpp>
#include <fstream>
#include <iostream>
#include <png++/png.hpp>
#include <string>
#include <vector>

#include "common.hpp"
#include "gfxdef.hpp"
#include "gfxutils.hpp"
#include "project.hpp"
#include "tileopt.hpp"

using namespace std;
using namespace chrgfx;
using namespace png;

void process_args(int argc, char ** argv);
void print_help();
void process_unoptimized(const buffer<byte_t> & tiles, size_t const bank_size,
												 size_t const img_width_chr);

void process_optimized(const buffer<byte_t> & tiles, size_t const bank_size,
											 size_t const img_width_chr);

struct RuntimeConfig
{
	string in_image_path;
	string out_prefix;

	// any value besides 0 indicates banked mode
	size_t rows_per_bank;

	// base tile in VRAM (added to tilemap tile indices)
	size_t tile_base;

	VDPPal pal_line;
	// mark tiles as priority in tilemap
	bool tile_priority;

	bool make_palette;
	bool optimize;
	bool chr_by_bank;
	bool make_tilemaps;
	bool width_header;
	bool chirari_rle;

	RuntimeConfig() :
			rows_per_bank(0), tile_base(0), pal_line(PAL0), tile_priority(false),
			make_palette(false), optimize(false), make_tilemaps(false),
			chr_by_bank(false), width_header(false), chirari_rle(false)
	{
	}
} cfg;

int main(int argc, char ** argv)
{
	try
	{
		process_args(argc, argv);

		// validity checks
		if(cfg.in_image_path.empty())
		{
			cerr << "No source image specified" << endl;
			exit(9);
		}

		if(cfg.out_prefix.empty())
		{
			cfg.out_prefix = strip_extension(cfg.in_image_path);
		}

		image<index_pixel> input_image;
		try
		{
			input_image.read(cfg.in_image_path);
		}
		catch(const exception & e)
		{
			cerr << "Failed to read source image " << cfg.in_image_path << endl;
			cerr << e.what() << endl;
			exit(3);
		}

		size_t tiles_per_bank = 0;
		size_t const
				// number of tiles per row in input image
				img_width_chr { input_image.get_width() / MD_CHR.width() },
				// number of rows in the input image
				img_height_chr { input_image.get_height() / MD_CHR.height() },
				// number of banks (aka "frames" or "tile groupings") in the source
				// image (only if the user specified we should process by bank)
				banks { cfg.rows_per_bank > 0 ? img_height_chr / cfg.rows_per_bank
																			: 0 },
				// number of tiles per bank
				bank_size { img_width_chr * cfg.rows_per_bank };

		buffer<byte_t> input_basic_tiles { png_chunk(
				MD_CHR.width(), MD_CHR.height(), input_image.get_pixbuf()) };

		if(cfg.optimize)
			process_optimized(input_basic_tiles, bank_size, img_width_chr);
		else
			process_unoptimized(input_basic_tiles, bank_size, img_width_chr);

		if(cfg.make_palette)
		{
			uptr<byte_t> out_pal { encode_pal(MD_PAL, MD_COL,
																				input_image.get_palette()) };
			string palette_out_path { cfg.out_prefix };
			palette_out_path.append(".pal");
			auto palette_out { ofstream_checked(palette_out_path) };
			// only copy one subpalette's worth of colors
			palette_out.write((char *)out_pal.get(), MD_PAL.datasize() / 8);
			palette_out.close();
		}
	}
	catch(exception const & e)
	{
		cerr << "Fatal Error: " << e.what() << endl;
		return -1;
	}
	return 0;
}

void process_unoptimized(buffer<byte_t> const & tiles, size_t const bank_size,
												 size_t const img_width_chr)
{
	// TODO does this imply we can output by bank only if tilemaps are also
	// generated?
	// what if we just want to chunk out groups of tiles from a large
	// source without the need of maps?
	bool by_bank { bank_size > 0 && (cfg.make_tilemaps || cfg.chr_by_bank) };

	if(!by_bank || (by_bank && !cfg.chr_by_bank))
	{
		string tiles_out_path { cfg.out_prefix };
		tiles_out_path.append(".chr");
		auto tiles_out { ofstream_checked(tiles_out_path) };
		dump_md_tiles(tiles, tiles_out);
	}

	if(!by_bank && cfg.make_tilemaps)
	{
		string map_out_path { cfg.out_prefix };
		map_out_path.append(".map");
		auto map_out { ofstream_checked(map_out_path) };
		auto tilemap { make_simple_tilemap(0, tiles.size(), cfg.pal_line,
																			 cfg.tile_priority, cfg.tile_base) };
		if(cfg.width_header)
			tilemap.emplace(tilemap.begin(), img_width_chr);
		dump_md_tilemap(tilemap, map_out);
	}

	if(by_bank)
	{
		size_t bank_count = tiles.size() / bank_size;
		for(auto bankidx { 0 }; bankidx < bank_count; ++bankidx)
		{
			stringstream ss;
			ss << cfg.out_prefix << '.' << setw(3) << setfill('0') << bankidx;

			if(cfg.chr_by_bank)
			{
				string path = ss.str() + ".chr";
				auto tiles_out { ofstream_checked(path) };
				dump_md_tiles(tiles, tiles_out, bank_size * bankidx, bank_size);
			}

			if(cfg.make_tilemaps)
			{
				string path = ss.str() + ".map";
				auto map_out { ofstream_checked(path) };
				auto tilemap { make_simple_tilemap(bank_size * bankidx, bank_size,
																					 cfg.pal_line, cfg.tile_priority,
																					 cfg.tile_base) };
				if(cfg.width_header)
					tilemap.emplace(tilemap.begin(), img_width_chr);
				dump_md_tilemap(tilemap, map_out);
			}
		}
	}
}

void process_optimized(buffer<byte_t> const & basic_tiles,
											 size_t const bank_size, size_t const img_width_chr)
{
	// bank_size = number of tiles in a bank
	bool by_bank { bank_size > 0 && (cfg.make_tilemaps || cfg.chr_by_bank) };

	if(!by_bank || (by_bank && !cfg.chr_by_bank))
	{
		auto infolist { analyze(basic_tiles, 0,
														basic_tiles.size<byte_t[BASIC_CHR_BYTESZ]>()) };
		string tiles_out_path { cfg.out_prefix };
		tiles_out_path.append(".chr");
		auto tiles_out { ofstream_checked(tiles_out_path) };
		auto filtered { filter_chrs(infolist) };
		dump_md_tiles(filtered, tiles_out);
	}

	if(!by_bank && cfg.make_tilemaps)
	{
		auto infolist { analyze(basic_tiles, 0,
														basic_tiles.size<byte_t[BASIC_CHR_BYTESZ]>()) };
		string map_out_path { cfg.out_prefix };
		map_out_path.append(".map");
		auto map_out { ofstream_checked(map_out_path) };

		vector<u16> tilemap;
		if(cfg.chirari_rle)
			tilemap = make_rle_tilemap(infolist, 0, infolist.size(), img_width_chr,
																 cfg.tile_base);
		else
		{
			tilemap = make_optinfo_tilemap(infolist, 0, infolist.size(), cfg.pal_line,
																		 cfg.tile_priority, cfg.tile_base);
			if(cfg.width_header)
				tilemap.emplace(tilemap.begin(), img_width_chr);
		}
		dump_md_tilemap(tilemap, map_out);
	}

	if(by_bank)
	{
		size_t bank_count =
				(basic_tiles.size<byte_t[BASIC_CHR_BYTESZ]>()) / bank_size;
		for(auto bankidx { 0 }; bankidx < bank_count; ++bankidx)
		{
			auto infolist { analyze(basic_tiles, bank_size * bankidx, bank_size) };
			stringstream ss;
			ss << cfg.out_prefix << '.' << setw(3) << setfill('0') << bankidx;

			if(cfg.chr_by_bank)
			{
				string path = ss.str() + ".chr";
				auto tiles_out { ofstream_checked(path) };
				dump_md_tiles(filter_chrs(infolist), tiles_out);
			}

			if(cfg.make_tilemaps)
			{
				string path = ss.str() + ".map";
				auto map_out { ofstream_checked(path) };
				vector<u16> tilemap;
				if(cfg.chirari_rle)
					tilemap = make_rle_tilemap(infolist, bank_size * bankidx, bank_size,
																		 img_width_chr, cfg.tile_base);
				else
				{
					tilemap = make_optinfo_tilemap(infolist, 0, bank_size, cfg.pal_line,
																				 cfg.tile_priority, cfg.tile_base);
					if(cfg.width_header)
						tilemap.emplace(tilemap.begin(), img_width_chr);
				}
				dump_md_tilemap(tilemap, map_out);
			}
		}
	}
}

void process_args(int argc, char ** argv)
{
	std::vector<option> long_opts {

		{ "source", required_argument, nullptr, 's' },
		{ "output", required_argument, nullptr, 'o' },
		{ "rows-per-bank", required_argument, nullptr, 'r' },
		{ "tile-base", required_argument, nullptr, 'i' },
		{ "palette-line", required_argument, nullptr, 'l' },
		{ "make-palette", no_argument, nullptr, 'p' },
		{ "tile-priority", no_argument, nullptr, 'P' },
		{ "optimize", no_argument, nullptr, 'z' },
		{ "chr-by-bank", no_argument, nullptr, 'b' },
		{ "make-tilemap", no_argument, nullptr, 't' },
		{ "width-header", no_argument, nullptr, 'w' },
		{ "chirari-rle", no_argument, nullptr, 'e' },
		{ "help", no_argument, nullptr, 'h' }
	};
	std::string short_opts { ":s:o:r:i:l:pPzbtweh" };

	while(true)
	{
		const auto this_opt =
				getopt_long(argc, argv, short_opts.data(), long_opts.data(), nullptr);
		if(this_opt == -1)
			break;

		switch(this_opt)
		{
			case 's':
				cfg.in_image_path = optarg;
				break;

			// output prefix
			case 'o':
				cfg.out_prefix = optarg;
				break;

			// bank row size
			case 'r':
				try
				{
					cfg.rows_per_bank = (size_t)stoi(optarg);
				}
				catch(const out_of_range & ex)
				{
					cerr << "Invalid argument for bank row size: " << optarg << endl;
					exit(5);
				}
				break;

			// base tile index
			case 'i':
				try
				{
					cfg.tile_base = (size_t)stoi(optarg);
				}
				catch(const out_of_range & ex)
				{
					cerr << "Invalid argument for base tile index: " << optarg << endl;
					exit(14);
				}
				break;

			// set priority on tiles
			case 'P':
				cfg.tile_priority = true;
				break;

			case 'p':
				cfg.make_palette = true;
				break;

			case 'l':
				try
				{
					auto palid = stoi(optarg);
					if(palid > 3 || palid < 0)
					{
						cerr << "Invalid argument for palette ID: " << optarg << endl;
						exit(11);
					}
					cfg.pal_line = (VDPPal)palid;
				}
				catch(const out_of_range & ex)
				{
					cerr << "Invalid argument for palette ID: " << optarg << endl;
					exit(11);
				}
				break;

			// optimize tile usage
			case 'z':
				cfg.optimize = true;
				break;

			// optimize by bank instead of whole image
			case 'b':
				cfg.chr_by_bank = true;
				break;

			// do not generate tilemaps
			case 't':
				cfg.make_tilemaps = true;
				break;

			case 'w':
				cfg.width_header = true;
				break;

			case 'e':
				cfg.chirari_rle = true;
				break;

			// help
			case 'h':
				print_help();
				exit(0);

			case ':':
				cerr << "Missing argument for option " << to_string(optopt) << endl;
				exit(1);

			case '?':
				cerr << "Unknown option" << endl;
				exit(2);
		}
	}
}

void print_help()
{
	std::cout << PROJECT::PROJECT_NAME << " - ver. " << PROJECT::VERSION
						<< std::endl;
}
