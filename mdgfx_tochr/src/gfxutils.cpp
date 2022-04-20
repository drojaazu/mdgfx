

#include "gfxutils.hpp"
#include "/home/ryou/Projects/lib/endian.hpp"

using namespace std;
using namespace chrgfx;
using namespace png;

bool is_blank_tile(byte_t const * chr)
{
	for(u8 pixel_iter { 0 }; pixel_iter < BASIC_CHR_BYTESZ; ++pixel_iter)
		if(chr[pixel_iter] != 0)
			return false;
	return true;
}

bool is_flat_tile(byte_t const * chr)
{
	u8 flatval = *chr;
	for(u8 pixel_iter { 1 }; pixel_iter < BASIC_CHR_BYTESZ; ++pixel_iter)
		if(chr[pixel_iter] != flatval)
			return false;
	return true;
}

bool is_identical_tile(byte_t const * chr1, byte_t const * chr2)
{
	for(u8 pixel_iter { 0 }; pixel_iter < BASIC_CHR_BYTESZ; ++pixel_iter)
		if(chr1[pixel_iter] != chr2[pixel_iter])
			return false;
	return true;
}

void v_flip_tile(byte_t * chr)
{
	int const swapcount { (int)BASIC_CHR_HEIGHT / 2 };
	byte_t * row1_offset { nullptr };
	byte_t * row2_offset { nullptr };

	for(byte_t this_rowswap { 0 }; this_rowswap < swapcount; ++this_rowswap)
	{
		row1_offset = chr + (this_rowswap * BASIC_CHR_WIDTH);
		row2_offset = chr + ((7 - this_rowswap) * BASIC_CHR_WIDTH);
		swap_ranges(row1_offset, row1_offset + BASIC_CHR_WIDTH, row2_offset);
	}
}

void h_flip_tile(byte_t * chr)
{
	int const swapcount { (int)BASIC_CHR_WIDTH / 2 };
	// u8* pxl1_offset{nullptr};
	// u8* pxl2_offset{nullptr};
	size_t pxl1_offset { 0 };
	size_t pxl2_offset { 0 };
	// two loops, one for each row
	// inner loop for each pixel swap in that row
	for(u8 this_row { 0 }; this_row < BASIC_CHR_HEIGHT; ++this_row)
	{
		for(u8 this_pxlswap { 0 }; this_pxlswap < swapcount; ++this_pxlswap)
		{
			// pxl1_offset = chr + (this_row * CHR_WIDTH) + this_pxlswap;
			// pxl2_offset = chr + (this_row * CHR_WIDTH) + (7 - this_pxlswap);

			pxl1_offset = (this_row * BASIC_CHR_WIDTH) + this_pxlswap;
			pxl2_offset = (this_row * BASIC_CHR_WIDTH) + (7 - this_pxlswap);
			std::swap(chr[pxl1_offset], chr[pxl2_offset]);
		}
	}
}

void dump_md_palette(palette const & pal, ostream & out)
{
	uptr<byte_t> out_pal { encode_pal(MD_PAL, MD_COL, pal) };
	// only copy one subpalette's worth of colors
	out.write((char *)out_pal.get(), MD_PAL.datasize() / 8);
	out_pal.reset();
}

void dump_md_tiles(buffer<byte_t> const & bank, ostream & out)
{
	dump_md_tiles(bank, out, 0, bank.size<byte_t[BASIC_CHR_BYTESZ]>());
}

void dump_md_tiles(buffer<byte_t> const & bank, ostream & out, size_t index,
									 size_t length)
{
	uptr<byte_t> chr;
	auto i_tile = bank.begin<byte_t[64]>() + index;
	auto i_tile_end = i_tile + length;
	while(i_tile != i_tile_end)
	{
		chr.reset(encode_chr(MD_CHR, *i_tile));
		out.write((char *)chr.get(), MD_CHR_BYTESZ / 8);
		++i_tile;
	}
	chr.reset();
}

void dump_md_tiles(vector<byte_t *> const & bank, ostream & out)
{
	dump_md_tiles(bank, out, 0, bank.size());
}

void dump_md_tiles(vector<byte_t *> const & bank, ostream & out, size_t index,
									 size_t length)
{
	uptr<byte_t> chr;
	// auto i_tile = bank.begin<byte_t[64]>() + index;
	// auto i_tile_end = i_tile + length;
	// while(i_tile != i_tile_end)
	for(auto ptr_tile : bank)
	{
		chr.reset(encode_chr(MD_CHR, ptr_tile));
		out.write((char *)chr.get(), MD_CHR_BYTESZ);
	}
	chr.reset();
}

void dump_md_tilemap(vector<u16> const & map, ostream & out)
{
	auto copy = endian_copy_func(true);
	u8 temp_buffer[2];
	for(auto entry : map)
	{
		copy((u8 *)&entry, (u8 *)&entry + 2, temp_buffer);
		out.write((char const *)temp_buffer, 2);
	}
}

u16 make_nametable_entry(u16 tile_index, enum VDPPal const pal_line,
												 bool const priority, bool const h_flip,
												 bool const v_flip)
{
	if(tile_index > 0x7ff || tile_index > 0x7ff)
		throw out_of_range("tile index invalid (max 0x7ff)");

	u16 entry = tile_index | (pal_line << 13);
	if(priority)
		entry |= 0x8000;
	if(h_flip)
		entry |= 0x0800;
	if(v_flip)
		entry |= 0x1000;

	return entry;
}

vector<u16> make_simple_tilemap(size_t const index, size_t const length,
																enum VDPPal const pal_line, bool const priority,
																u16 const base)
{
	size_t begin { index + base }, end { begin + length };
	if(begin > 0x7ff || end > 0x7ff)
		throw out_of_range("tile index or index + base too high (max 0x7ff)");

	vector<u16> map;
	map.reserve(length);
	for(size_t tile_index { begin }; tile_index < end; ++tile_index)
		map.push_back(
				make_nametable_entry(tile_index, pal_line, priority, false, false));

	return map;
}
