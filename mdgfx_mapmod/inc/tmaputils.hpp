#ifndef MDMAPMOD__TMAPUTILS_H
#define MDMAPMOD__TMAPUTILS_H

#include "common.hpp"

#define PRIORITY_BIT 15
#define PALETTE_BIT 13
#define VFLIP_BIT 12
#define HFLIP_BIT 11

#define TILE_MASK 0x7ff

u16 priority_flag(u16 entry, bool set = true)
{
	return set ? entry |= (1 << PRIORITY_BIT) : entry &= ~(1 << PRIORITY_BIT);
}

u16 vflip_flag(u16 entry, bool set = true)
{
	return set ? entry |= (1 << VFLIP_BIT) : entry &= ~(1 << VFLIP_BIT);
}

u16 hflip_flag(u16 entry, bool set = true)
{
	return set ? entry |= (1 << HFLIP_BIT) : entry &= ~(1 << HFLIP_BIT);
}

u16 set_pal_line(u16 entry, u8 pal_line)
{
	u16 out = entry;
	out &= 0x9fff;
	return out |= (pal_line << PALETTE_BIT);
}

u16 modify_chridx(u16 entry, s16 idx_delta)
{
	s16 chridx = entry & TILE_MASK;
	chridx += idx_delta;
	chridx &= TILE_MASK;
	entry &= 0xf800;
	return entry | chridx;
}

#endif
