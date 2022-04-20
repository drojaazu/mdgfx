
#include "tileopt.hpp"

using namespace std;
using namespace chrgfx;

TilemapEntry::TilemapEntry() :
		id(nullopt), runlength(nullopt), h_flip(false), v_flip(false) {};

TileOptInfo::TileOptInfo() :
		type(TileType::UNDEFINED), idx(0), idx_opt(0), dupe_of_idx(nullopt),
		flat_palidx(0), v_flip(false), h_flip(false), crc(0), crc_v_flip(0),
		crc_h_flip(0), crc_hv_flip(0), tile_data(nullptr) {};

/**
 * generates a list of TileOptInfo objects, one for each tile, which
 * will be used to optimize chr data inclusion in the graphics data and specify
 * tile references in the map data
 */
vector<TileOptInfo> analyze(buffer<byte_t> const & basic_tiles,
														size_t const start_chr, size_t const chr_count)
{
	// allocate some space for flipping a test tile around
	byte_t flip_buffer[BASIC_CHR_BYTESZ];

	vector<TileOptInfo> out_infolist;
	out_infolist.reserve(chr_count);

	size_t this_orig_idx { 0 };

	// pass 1 - identify flat & blank tiles and generate CRCs for normal tiles
	auto iter_chr = basic_tiles.begin<byte_t[BASIC_CHR_BYTESZ]>() + start_chr;
	auto iter_end = iter_chr + chr_count;
	while(iter_chr != iter_end)
	{
		auto this_chr = *iter_chr;
		TileOptInfo tileinfo;
		tileinfo.tile_data = this_chr;
		tileinfo.idx = this_orig_idx++;

		// we do not treat blanks as flats for two reasons
		// 	one, it is likely that chr 0 in VRAM is already blank, so there's no
		// 		reason to waste an extra tile when we can reference that one
		//	two, we can assume a blank tile is "non-existant" in terms
		//		of our output map, which will help to optimize maps with lots of
		//		blank space

		// TODO: we don't know that vram chr 0 is always blank, so add a flag that
		// treat blanks as flats and include in chr

		// check if tile is flat (all one color)
		if(is_flat_tile(this_chr))
		{
			// check if the flat color is palette entry 0
			// i.e. if the tile is blank
			if(this_chr[0] == 0)
			{
				tileinfo.type = BLANK;
			}
			else
			{
				tileinfo.type = FLAT;
				tileinfo.flat_palidx = this_chr[0];
			}
			// don't need to bother with CRCs if it's a flat
			goto end_checks;
		}

		// neither blank nor flat, must be normal
		tileinfo.type = NORMAL;

		// get CRC for tile in all positions
		// crc for normal
		tileinfo.crc = crc32(0, (Bytef *)this_chr, BASIC_CHR_BYTESZ);

		// crc for hflip
		copy(this_chr, this_chr + BASIC_CHR_BYTESZ, flip_buffer);
		h_flip_tile(flip_buffer);
		tileinfo.crc_h_flip = crc32(0, (Bytef *)flip_buffer, BASIC_CHR_BYTESZ);

		// crc for vflip
		copy(this_chr, this_chr + BASIC_CHR_BYTESZ, flip_buffer);
		v_flip_tile(flip_buffer);
		tileinfo.crc_v_flip = crc32(0, (Bytef *)flip_buffer, BASIC_CHR_BYTESZ);

		// crc for hvflip
		copy(this_chr, this_chr + BASIC_CHR_BYTESZ, flip_buffer);
		h_flip_tile(flip_buffer);
		v_flip_tile(flip_buffer);
		tileinfo.crc_hv_flip = crc32(0, (Bytef *)flip_buffer, BASIC_CHR_BYTESZ);

	end_checks:
		out_infolist.push_back(tileinfo);
		++iter_chr;
	}

	// **************** PASS 2
	// identify duplicate tiles
	// 	- out loop through each tile backwards from end (the work tile)
	// 	- inner loop through each tile forwards from start (the compare tile)
	// 	- compare each CRC variation of the work tile to only the NORMAL CRC of
	// 		the compare tile
	// 	- if there is a match, do a deep compare to ensure the match is valid
	// 		and not a CRC collision
	// 	- if a true match, mark the work tile as duplicate, point it to the
	//		compare tile as the original/"master" and set flip flags as
	//		necessary so it would match the master tile

	// loop backwards from back (work tiles)
	for(auto work_tile { out_infolist.rbegin() };
			work_tile != out_infolist.rend(); ++work_tile)
	{
		// all tiles should have been given a type in the previous pass
		if(work_tile->type == UNDEFINED)
			throw runtime_error(
					"Found tile marked UNDEFINED in pass 2 of tile info generation");

		// always ignore blank tiles since there's nothing inside to compare
		if(work_tile->type == BLANK)
			continue;

		// loop forwards from start (compare tiles)
		size_t compare_tile_idx { 0 };
		for(auto compare_tile { out_infolist.begin() };
				compare_tile != out_infolist.end(); ++compare_tile, ++compare_tile_idx)
		{
			// tile to compare against is blank? move along
			if(compare_tile->type == BLANK)
				continue;

			// working on the same tile? move along
			// TODO: if we've reached the current tile going forward, do we
			// even need to continue? presumably tiles *after* the current work
			// tile have already been checked anyway since the outer loop
			// goes backwards...
			// -- per above, we break here insted
			// it seems to work fine, but if there's some weirdness check here first
			// **************************
			// SOMETHING BROKEN? CHeck here first
			// **************************
			if(&(*work_tile) == &(*compare_tile))
			{
				// continue;
				break;
			}

			//	- if the compare tile already marked as duplicate of another tile
			//		elsewhere, move along
			//	- we only want to reference the "master" tile of any dupes
			if(compare_tile->dupe_of_idx)
				continue;

			// if our current tile is flat, check only against other flats
			if(work_tile->type == FLAT)
			{
				// if both tiles are flat, see if they share the same color
				if(compare_tile->type == FLAT &&
					 work_tile->flat_palidx == compare_tile->flat_palidx)
				{
					// we have a dupe!
					work_tile->dupe_of_idx = compare_tile_idx;
					// exit our dupe check loop
					break;
				}
				// ignore if the compare tile is not also flat
				continue;
			}

			// compare normal tile
			if(work_tile->crc == compare_tile->crc)
			{
				// we (might) have a dupe!
				// do deep compare to be sure there wasn't a CRC collision
				if(is_identical_tile(work_tile->tile_data, compare_tile->tile_data))
				{
					// we have a dupe!
					work_tile->dupe_of_idx = compare_tile_idx;
					// break out of the loop, since we've found a dupe
					// (presumably, the first one since our compare loop moves forward)
					break;
				}
			}

			// compare against hflip tile
			if(work_tile->crc_h_flip == compare_tile->crc)
			{
				// we (might) have a dupe!
				copy(work_tile->tile_data, work_tile->tile_data + BASIC_CHR_BYTESZ,
						 flip_buffer);
				h_flip_tile(flip_buffer);
				if(is_identical_tile(flip_buffer, compare_tile->tile_data))
				{
					// we have a dupe!
					work_tile->dupe_of_idx = compare_tile_idx;
					work_tile->h_flip = true;
					break;
				}
			}

			// compare against vflip tile
			if(work_tile->crc_v_flip == compare_tile->crc)
			{
				// we (might) have a dupe!
				copy(work_tile->tile_data, work_tile->tile_data + BASIC_CHR_BYTESZ,
						 flip_buffer);
				v_flip_tile(flip_buffer);
				if(is_identical_tile(flip_buffer, compare_tile->tile_data))
				{
					// we have a dupe!
					work_tile->dupe_of_idx = compare_tile_idx;
					work_tile->v_flip = true;
					break;
				}
			}

			// compare against hvflip tile
			if(work_tile->crc_hv_flip == compare_tile->crc)
			{
				// we (might) have a dupe!
				copy(work_tile->tile_data, work_tile->tile_data + BASIC_CHR_BYTESZ,
						 flip_buffer);
				h_flip_tile(flip_buffer);
				v_flip_tile(flip_buffer);
				if(is_identical_tile(flip_buffer, compare_tile->tile_data))
				{
					// we have a dupe!
					work_tile->dupe_of_idx = compare_tile_idx;
					work_tile->h_flip = true;
					work_tile->v_flip = true;
					break;
				}
			}
		}

		// if we reached this part of the loop, there have been no dupes
		// keep on movin'
	}

	// **************** PASS 3
	// 	- at this point, all tiles should have been evaluated for duplicates
	// 	- now assign a final order to all unique tiles
	// 	- repoint each duplicate tile to the final index of the
	//		tile it duplicates
	size_t optimized_idx { 0 };

	// put flats at the front
	// (no particular reason for this, just makes things "cleaner", imo)
	for(auto & this_tile : out_infolist)
		if((this_tile.type == FLAT) & !this_tile.dupe_of_idx)
			this_tile.idx_opt = optimized_idx++;

	// put the rest of the unique
	for(auto & this_tile : out_infolist)
		if(this_tile.type == NORMAL && !this_tile.dupe_of_idx)
			this_tile.idx_opt = optimized_idx++;

	// all unique tiles now have a final index assigned
	// now point all duplicated tiles to the final, optimized index of the
	// tile that they duplicate
	for(auto & this_tile : out_infolist)
		if(this_tile.dupe_of_idx)
			this_tile.idx_opt = out_infolist[this_tile.dupe_of_idx.value()].idx_opt;

	return out_infolist;
}

/**
 * Create a list of pointers to the unique, "master" tiles to be
 * exported as the final collection of CHR graphics for use
 */
vector<byte_t *> filter_chrs(vector<TileOptInfo> const & infolist)
{
	size_t unique_tile_count { 0 };

	// not the most efficient way to do things but eh...
	for(auto const & this_tile : infolist)
	{
		// find the tile with the highest optimized index and use that as
		// the final tile count...
		if(this_tile.idx_opt > unique_tile_count)
			unique_tile_count = this_tile.idx_opt;
	}
	// ... plus one, because idxOpt is a 0 based index
	++unique_tile_count;

	vector<byte_t *> unique_chrs(unique_tile_count, nullptr);

	for(auto const & tileinfo : infolist)
	{
		if(tileinfo.type == BLANK)
			continue;
		if(unique_chrs[tileinfo.idx_opt] == nullptr)
			unique_chrs[tileinfo.idx_opt] = tileinfo.tile_data;
	}

	return unique_chrs;
}

u16 make_chirari_rle_entry(TileOptInfo info, u16 tile_base = 0, u8 tile_run = 1)
{
	// tilemap format:
	// |   | | |           |
	//  xxx v h ttttttttttt
	// t - tile id
	// v, h - flip bits
	// xxx - empty/run length

	// for xxx, if only low bit is set, indicates this is a run of blank tiles
	// all lower bits (0 to 12) will be used for the run length of blank tiles
	// if any other xxx bits are set, all lower bits are as normal, and the xxx
	// bits count as the run length

	if(tile_run < 1)
		throw out_of_range("Tile run must be positive value");

	u16 outMapEntry { 0 };

	if(info.type == BLANK)
	{
		// if it's a single blank tile, don't mess with RLE
		// if (tileRun == 1)
		//	return 0;
		// ACKCHUALLY...
		// we need "single runs" to identify blanks from actual entries
		// with tile index 0...
		// this was a design oversight and we could probably make things more
		// elegant but for now run of 1 for blanks is valid

		// set the blank tile run flag (lowest bit of RLE bitfield)
		outMapEntry = 0x2000;
		outMapEntry |= (tile_run & 0x1fff);
		return outMapEntry;
	}

	// make sure the tile id is within VRAM range
	outMapEntry = ((tile_base + info.idx_opt) & 0x7ff);

	if(tile_run > 1)
	{
		// make sure the tile run is within RLE range
		tile_run &= 0x7;
		outMapEntry |= (tile_run << 13);
	}

	// apply flip flags
	if(info.h_flip)
		outMapEntry |= 0x800;
	if(info.v_flip)
		outMapEntry |= 0x1000;

	return outMapEntry;
}

vector<u16> make_optinfo_tilemap(vector<TileOptInfo> const & infolist,
																 size_t const index, size_t const length,
																 enum VDPPal const pal_line,
																 bool const priority, u16 const tile_base)
{
	vector<u16> out_map;
	out_map.reserve(length);

	for(size_t i { index }; i < (index + length); ++i)
	{
		TileOptInfo info = infolist[i];
		u16 tileidx = info.type == BLANK ? 0 : info.idx_opt + tile_base;
		out_map.push_back(make_nametable_entry(tileidx, pal_line, priority,
																					 info.h_flip, info.v_flip));
	}

	return out_map;
}

// TODO this is still using that temp style with iterators, fix it up
vector<u16> make_rle_tilemap(vector<TileOptInfo> const & infolist,
														 size_t const index, size_t const length,
														 size_t const tilemap_width, u16 const tile_base)
{

	vector<u16> outTilemap;

	outTilemap.reserve(length + 2);
	outTilemap.push_back(tilemap_width);

	size_t runlength { 1 };

	auto this_info { infolist.begin() + index };
	auto info_end { this_info + length };
	auto info_prev = this_info;

	// begin comparison at the second tile
	++this_info;

	int counter = 0;
	do
	{
		++counter;
		// if this tile and the previous were empty, add to run
		if((this_info->type == BLANK) && (info_prev->type == BLANK))
		{
			++runlength;
			++info_prev;
			continue;
		}

		// if we're here, we're dealing with a flat or normal tile
		// check if the tile ID, hflip and vflip are all identical
		if((this_info->idx_opt == info_prev->idx_opt) &&
			 (this_info->h_flip == info_prev->h_flip) &&
			 (this_info->v_flip == info_prev->v_flip) && (info_prev->type != BLANK) &&
			 (this_info->type != BLANK))
		{
			++runlength;
			// max run of 7 due to only have 3 bits to work with
			if(runlength < 7)
			{
				// our run is not yet maxxed, check the next tile
				continue;
			}
			// we've hit a full run, stop and process it

			// (it's important to skip over the current tile for the next check
			// otherwise it will cause issues for long runs (runs over 7)
			// this is because this_tile is being included as the end of a run now
			// and if we do not skip over it, it will be marked as prev_tile in the
			// next iteration, recognized as part of a run, and included as part
			// of the next run as well, throwing things further and further off
			// with the length of the run)
			// (this was a bug that gave me quite a headache)
			++this_info;
			// kind of hack-y...
			if(this_info == info_end)
				break;
		}

		// at this point, we should have accounted for a tile run
		auto tilemapEntry =
				make_chirari_rle_entry(*info_prev, tile_base, runlength);
		outTilemap.push_back(tilemapEntry);

		// prepare for next check
		runlength = 1;
		info_prev = this_info;

		//} while (++thisTile != infoEnd);
	} while(++this_info != info_end);

	// and account for the very last tile
	auto tilemapEntry = make_chirari_rle_entry(*info_prev, tile_base, runlength);
	outTilemap.push_back(tilemapEntry);

	// map terminator
	outTilemap.push_back(0xffff);

	return outTilemap;
}
