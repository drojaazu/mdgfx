#ifndef MDGFX__GFXUTILS_H
#define MDGFX__GFXUTILS_H

#include "gfxdef.hpp"
#include <chrgfx/chrgfx.hpp>
#include <png++/png.hpp>

bool is_blank_tile(byte_t const * chr);

bool is_flat_tile(byte_t const * chr);

bool is_identical_tile(byte_t const * chr1, byte_t const * chr2);

void v_flip_tile(byte_t * chr);

void h_flip_tile(byte_t * chr);

void dump_md_palette(png::palette const & pal, std::ostream & out);

void dump_md_tiles(buffer<byte_t> const & bank, std::ostream & out);

void dump_md_tiles(buffer<byte_t> const & bank, std::ostream & out,
									 size_t index, size_t length);

void dump_md_tiles(std::vector<byte_t *> const & bank, std::ostream & out);

void dump_md_tiles(std::vector<byte_t *> const & bank, std::ostream & out,
									 size_t index, size_t length);

void dump_md_tilemap(std::vector<u16> const & map, std::ostream & out);

u16 make_nametable_entry(u16 tile_index, enum VDPPal const pal_line = PAL0,
												 bool const priority = false, bool const h_flip = false,
												 bool const v_flip = false);

std::vector<u16> make_simple_tilemap(std::size_t const index,
																		 std::size_t const length,
																		 enum VDPPal const pal_line = PAL0,
																		 bool const priority = false,
																		 u16 const base = 0);

#endif
