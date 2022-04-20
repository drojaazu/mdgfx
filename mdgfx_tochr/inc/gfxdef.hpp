#ifndef MDGFX__GFXDEF_H
#define MDGFX__GFXDEF_H

#include "common.hpp"
#include <chrgfx/chrgfx.hpp>
#include <vector>

enum VDPPal : u8
{
	PAL0 = 0b00,
	PAL1 = 0b01,
	PAL2 = 0b10,
	PAL3 = 0b11
};

chrgfx::chrdef const MD_CHR {
	"Megadrive",
	8,
	8,
	4,
	std::vector<ushort> { 3, 2, 1, 0 },
	std::vector<ushort> { 0, 4, 8, 12, 16, 20, 24, 28 },
	std::vector<ushort> { 0, 32, 64, 96, 128, 160, 192, 224 }
};

// 8x8 @ 4bpp = 32 bytes
constexpr size_t MD_CHR_BYTESZ { 32 };

chrgfx::paldef const MD_PAL { "Megadrive", 16, 16, 4 };
chrgfx::rgbcoldef const MD_COL {
	"Megadrive", 3,
	std::vector<chrgfx::rgb_layout> {
			chrgfx::rgb_layout { pair { 1, 3 }, pair { 5, 3 }, pair { 9, 3 } } },
	true
};

// basic tiles info
size_t const BASIC_CHR_WIDTH { MD_CHR.width() };
size_t const BASIC_CHR_HEIGHT { MD_CHR.height() };

// basic tiles are 1 byte per pixel
constexpr size_t BASIC_CHR_BYTESZ { 64 };

#endif