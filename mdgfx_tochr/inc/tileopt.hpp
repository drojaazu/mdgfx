#ifndef MDGFX__TILEOPT_H
#define MDGFX__TILEOPT_H

#include "common.hpp"
#include "gfxutils.hpp"
#include "zlib.h"
#include <chrgfx/chrgfx.hpp>
#include <vector>

enum TileType
{
	UNDEFINED,
	BLANK,
	FLAT,
	NORMAL
};

// describes a tilemap entry
struct TilemapEntry
{
	// if not set, blank (null) tile
	std::optional<std::size_t> id;
	// number of entries; if not set, single tile
	std::optional<std::size_t> runlength;

	bool h_flip;
	bool v_flip;

	TilemapEntry();
};

// map entry optimization meta data
struct TileOptInfo
{
	/**
	 *
	 */
	TileType type;

	/**
	 * The index of this map entry within the source image
	 */
	u16 idx;

	/**
	 * The index of this tile within the block of final, optimized tiles
	 */
	u16 idx_opt;

	/**
	 * If this tile is a duplcate, contains the idxOrig of the "master" tile
	 * which is duplicates
	 */
	std::optional<u16> dupe_of_idx;

	/**
	 * The palette entry used if the tile is flat
	 */
	u8 flat_palidx;

	/**
	 * Indicates this tile must be V flipped in order to match the dupe source
	 */
	bool v_flip;

	/**
	 * Indicates this tile must be H flipped in order to match the dupe source
	 */
	bool h_flip;

	ulong crc;
	ulong crc_v_flip;
	ulong crc_h_flip;
	ulong crc_hv_flip;

	byte_t * tile_data;

	TileOptInfo();
};

std::vector<TileOptInfo> analyze(buffer<byte_t> const & src_tiles,
																 std::size_t const start_chr,
																 std::size_t const chr_count);

std::vector<byte_t *> filter_chrs(std::vector<TileOptInfo> const & mapInfoList);

u16 make_nametable_entry(TileOptInfo tileInfo, u16 vramTileBase = 0);

std::vector<u16> make_optinfo_tilemap(std::vector<TileOptInfo> const & infolist,
																			std::size_t const index,
																			std::size_t const length,
																			enum VDPPal const pal_line = PAL0,
																			bool const priority = false,
																			u16 const tile_base = 0);

std::vector<u16> make_rle_tilemap(std::vector<TileOptInfo> const & infolist,
																	std::size_t const index,
																	std::size_t const length,
																	std::size_t const tilemap_width,
																	u16 const tile_base);

#endif
