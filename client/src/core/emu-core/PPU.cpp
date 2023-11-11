/*
 * Copyright 2023 Edward C. Pinkston
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// FIXME: I THINK WE CAN ADD AN OPTION TO REMOVE SPRITE FLICKER BY POTENTIALLY
// SUPPRESSING THE SPRITE OVERFLOW FLAG OR SOMETHING
// IDK, I WOULD FIRST NEED TO KNOW HOW DEVS CODED IN THE FLICKERING, SINCE
// THIS LINK MAY BE HELPFUL
// https://retrocomputing.stackexchange.com/questions/1145/why-do-nes-sprites-flicker-when-there-are-a-lot-of-them
// THE FLICKERING IS NOT A PRODUCT OF THE HARDWARE
// FIXME: SPRITE OVERFLOW WORKS THE OLC WAY BUT DOES NOT WORK THE WAY THE NES DOES
//        YOU NEED TO RESEARCH HOW TO DO IT CORRECTLY
// FIXME: BUBBLE BOBBLE DOES NOT HAVE CONSISTENT STATE ON STARTUP, BUT REQUIRES
// BLACK AT BG MAIN COLOR
// TRIED THIS, BUT IT DIDN'T WORK. IT ENDS UP BLUE
// CLEARLY IT RELIES ON A CERTAIN STARTUP STATE WHICH I DON'T HAVE
#include "PPU.h"

#include <string.h>

#include "Bus.h"
#include "Cart.h"
#include "mappers/Mapper.h"
#include "../Util.h"

namespace NESCLE {
/* Helper Functions */
static uint8_t flip_bits(uint8_t x) {
    // The trivial algorithm is to say if the ith position is set,
    // then the (7 - i)th bit would be set in the result

    // However, this is a rather clever algorithm from GeeksForGeeks
    // https://www.geeksforgeeks.org/write-an-efficient-c-program-to-reverse-bits-of-a-number/
    int count = 7;
    uint8_t res = x;

    x >>= 1;
    while (x > 0) {
        res <<= 1;
        res |= x & 1;
        x >>= 1;
        count--;
    }

    res <<= count;
    return res;
}

static void set_loopy_register(uint16_t* reg, uint16_t value,
    uint16_t bitmask) {
    switch (bitmask) {
    case PPU::LOOPY_COARSE_X:
        value <<= 0;
        break;
    case PPU::LOOPY_COARSE_Y:
        value <<= 5;
        break;
    case PPU::LOOPY_NAMETBL_X:
        value <<= 10;
        break;
    case PPU::LOOPY_NAMETBL_Y:
        value <<= 11;
        break;
    case PPU::LOOPY_FINE_Y:
        value <<= 12;
        break;

    default:
        printf("set_loopy_register: invalid bitmask\n");
        break;
    }

    *reg = (*reg & ~bitmask) | (value & bitmask);
}

// Aligns loopy register with bit 0
static uint16_t align_loopy_register(uint16_t reg, uint16_t bitmask) {
    uint16_t ret = reg & bitmask;

    switch (bitmask) {
    case PPU::LOOPY_COARSE_X:
        ret >>= 0;
        break;
    case PPU::LOOPY_COARSE_Y:
        ret >>= 5;
        break;
    case PPU::LOOPY_NAMETBL_X:
        ret >>= 10;
        break;
    case PPU::LOOPY_NAMETBL_Y:
        ret >>= 11;
        break;
    case PPU::LOOPY_FINE_Y:
        ret >>= 12;
        break;

    default:
        printf("align_loopy_register: invalid bitmask\n");
        break;
    }

    return ret;
}

void PPU::ScreenWrite(int x, int y, uint32_t color) {
    // Avoids buffer overflow on overscan
    if (y >= RESOLUTION_Y || x >= RESOLUTION_X || x < 0 || y < 0)
        return;
    screen[y * RESOLUTION_X + x] = color;
}

void PPU::LoadBGShifters() {
    // Shift the current LSB to the MSB and then stick the
    // next tile byte as the LSB
    // This is for the pixel offset
    PPU* ppu = this;
    ppu->bg_shifter_pattern_lo = (ppu->bg_shifter_pattern_lo & 0xff00)
        | ppu->bg_next_tile_lsb;
    ppu->bg_shifter_pattern_hi = (ppu->bg_shifter_pattern_hi & 0xff00)
        | ppu->bg_next_tile_msb;

    // The selected palette only changes every 8 pixels, so to avoid any
    // complicated logic, we pad the value to take up a full 8-bits
    // The backgrounds only use the first 4 palettes, as the other 4 are for
    // foreground, so we only need 2 bits to figure out what palette to use
    ppu->bg_shifter_attr_lo = (ppu->bg_shifter_attr_lo & 0xff00)
        | ((ppu->bg_next_tile_attr & 1) ? 0xff : 0x00);
    ppu->bg_shifter_attr_hi = (ppu->bg_shifter_attr_hi & 0xff00)
        | ((ppu->bg_next_tile_attr & 2) ? 0xff : 0x00);
}

void PPU::UpdateShifters() {
    PPU* ppu = this;
    if (ppu->mask & PPU_MASK_BG_ENABLE) {
        ppu->bg_shifter_pattern_lo <<= 1;
        ppu->bg_shifter_pattern_hi <<= 1;
        ppu->bg_shifter_attr_lo <<= 1;
        ppu->bg_shifter_attr_hi <<= 1;
    }

    // THE FACT THIS THIS IS 1 AND NOT 2 MAKES ME THINK THE ORIGINAL LOOP IS WRONG
    // WITH TEH 2 OR THIS IS WRONG
    if ((ppu->mask & PPU_MASK_SPR_ENABLE) && ppu->cycle >= 1 && ppu->cycle < 258) {
        for (int i = 0; i < ppu->spr_count; i++) {
            // Decrement x coord until 0 before shifting, else
            // we will shift out the data before rendering the sprite
            if (ppu->spr_scanline[i].x > 0) {
                ppu->spr_scanline[i].x--;
            }
            else {
                ppu->spr_shifter_pattern_lo[i] <<= 1;
                ppu->spr_shifter_pattern_hi[i] <<= 1;
            }
        }
    }
}

void PPU::IncrementScrollX() {
    PPU* ppu = this;
    if ((ppu->mask & PPU_MASK_BG_ENABLE) || (ppu->mask & PPU_MASK_SPR_ENABLE)) {
        // Recall that a nametable is 32 across,
        // so we must wrap the value if we hit idx 31
        if (align_loopy_register(ppu->vram_addr, LOOPY_COARSE_X) == 31) {
            // Clear the coarse_x bits and flip the nametable bit
            ppu->vram_addr &= ~LOOPY_COARSE_X;
            uint16_t ntx = align_loopy_register(ppu->vram_addr,
                LOOPY_NAMETBL_X);
            set_loopy_register(&ppu->vram_addr, ~ntx, LOOPY_NAMETBL_X);
        }
        else {
            // Increment the coarse_x
            uint16_t course_x = align_loopy_register(ppu->vram_addr,
                LOOPY_COARSE_X);
            set_loopy_register(&ppu->vram_addr, course_x + 1,
                LOOPY_COARSE_X);
        }
    }
}

void PPU::IncrementScrollY() {
    PPU* ppu = this;
    if ((ppu->mask & PPU_MASK_BG_ENABLE) || (ppu->mask & PPU_MASK_SPR_ENABLE)) {
        uint16_t fine_y = align_loopy_register(ppu->vram_addr, LOOPY_FINE_Y);
        if (fine_y < 7) {
            set_loopy_register(&ppu->vram_addr, fine_y + 1, LOOPY_FINE_Y);
        }
        else {
            // Wipe the fine y bits
            ppu->vram_addr &= ~LOOPY_FINE_Y;

            uint16_t coarse_y = align_loopy_register(ppu->vram_addr,
                LOOPY_COARSE_Y);

            // If we are in the regular memory
            if (coarse_y == 29) {
                // Wipe the coarse y bits
                ppu->vram_addr &= ~LOOPY_COARSE_Y;

                // Flip nametable bit
                uint16_t nty = align_loopy_register(ppu->vram_addr,
                    LOOPY_NAMETBL_Y);
                set_loopy_register(&ppu->vram_addr, ~nty, LOOPY_NAMETBL_Y);
            }
            // If we are in the attribute memory
            // (recall last 2 rows are the attribute data)
            else if (coarse_y == 31) {
                // Wipe the coarse y bits
                ppu->vram_addr &= ~LOOPY_COARSE_Y;
            }
            else {
                // By default just increment coarse_y by 1
                set_loopy_register(&ppu->vram_addr, coarse_y + 1,
                    LOOPY_COARSE_Y);
            }
        }
    }
}

void PPU::TransferAddrX() {
    PPU* ppu = this;
    if ((ppu->mask & PPU_MASK_BG_ENABLE) || (ppu->mask & PPU_MASK_SPR_ENABLE)) {
        // Copy x components of tram into vram
        uint16_t ntx = align_loopy_register(ppu->tram_addr, LOOPY_NAMETBL_X);
        uint16_t coarse_x = align_loopy_register(ppu->tram_addr,
            LOOPY_COARSE_X);
        set_loopy_register(&ppu->vram_addr, ntx, LOOPY_NAMETBL_X);
        set_loopy_register(&ppu->vram_addr, coarse_x, LOOPY_COARSE_X);
    }
}

void PPU::TransferAddrY() {
    PPU* ppu = this;
    if ((ppu->mask & PPU_MASK_BG_ENABLE) || (ppu->mask & PPU_MASK_SPR_ENABLE)) {
        uint16_t fine_y = align_loopy_register(ppu->tram_addr, LOOPY_FINE_Y);
        uint16_t nty = align_loopy_register(ppu->tram_addr, LOOPY_NAMETBL_Y);
        uint16_t coarse_y = align_loopy_register(ppu->tram_addr,
            LOOPY_COARSE_Y);

        set_loopy_register(&ppu->vram_addr, fine_y, LOOPY_FINE_Y);
        set_loopy_register(&ppu->vram_addr, nty, LOOPY_NAMETBL_Y);
        set_loopy_register(&ppu->vram_addr, coarse_y, LOOPY_COARSE_Y);
    }
}

void PPU::WriteToSprPatternTbl(int idx, uint8_t palette,
    int tile, int x, int y) {
    PPU* ppu = this;
    for (int i = 0; i < 8; i++) {
        uint8_t tile_lsb = Read(0x1000 * idx + tile * 16 + i);
        uint8_t tile_msb = Read(0x1000 * idx + tile * 16 + i + 8);

        for (int j = 0; j < 8; j++) {
            uint8_t color = ((tile_msb & 1) << 1) | (tile_lsb & 1);

            uint32_t px = GetColorFromPalette(palette, color);
            ppu->sprpatterntbl[idx][i+y][7 - j + x] = px;

            tile_msb >>= 1;
            tile_lsb >>= 1;
        }
    }
}

uint32_t PPU::MapColor(int idx) {
    // Maps a value in range 0 to 0x40 to a valid NES color in ARGB format
    // All colors begin with ff to signify that they are opaque
    static const uint32_t color_map[0x40] = {
        0xff545454, 0xff001e74, 0xff081090, 0xff300088, 0xff440064,
        0xff5c0030, 0xff540400, 0xff3c1800, 0xff202a00, 0xff083a00,
        0xff004000, 0xff003c00, 0xff00323c,
        0xff000000, 0xff000000, 0xff000000,

        0xff989698, 0xff084cc4, 0xff3032ec, 0xff5c1ee4, 0xff8814b0,
        0xffa01464, 0xff982220, 0xff783c00, 0xff545a00, 0xff287200,
        0xff087c00, 0xff007628, 0xff006678,
        0xff000000, 0xff000000, 0xff000000,

        0xffeceeec, 0xff4c9aec, 0xff787cec, 0xffb062ec, 0xffe454ec,
        0xffec58b4, 0xffec6a64, 0xffd48820, 0xffa0aa00, 0xff74c400,
        0xff4cd020, 0xff38cc6c, 0xff38b4cc, 0xff3c3c3c,
        0xff000000, 0xff000000,

        0xffeceeec, 0xffa8ccec, 0xffbcbcec, 0xffd4b2ec, 0xffecaeec,
        0xffecaed4, 0xffecb4b0, 0xffe4c490, 0xffccd278, 0xffb4de78,
        0xffa8e290, 0xff98e2b4, 0xffa0d6e4, 0xffa0a2a0,
        0xff000000, 0xff000000
    };

    return color_map[idx];
}

uint32_t* PPU::GetPatternTable(uint8_t idx, uint8_t palette) {
    int x = 0;
    int y = 0;
    for (int tile = 0; tile < 256; tile++) {
        WriteToSprPatternTbl(idx, palette, tile, x, y);
        x += 8;
        if (x == 128) {
            x = 0;
            y += 8;
        }
    }

    // matrix will be interpreted as a linear array
    return (uint32_t*)sprpatterntbl[idx];
}

uint32_t PPU::GetColorFromPalette(uint8_t palette, uint8_t pixel) {
    // we need to read into the area of memory that the PPU reserves for the palette
    // each palette in the range is a 4 byte offset and then the individual
    // pixel color in that palette we want is a 1 byte offset on top of that
    // after we read that value it gives us an index into the nes's color
    // table so we want to return that color

    // Also just know that the first 4 palettes are for background and last 4 are for
    // foreground. Unlike the pattern memory, which usually uses the first half for bg
    // and the second half for fg, but does not force you to do that, you are forced
    // to use the first half for bg and second half for fg for the palette selection.

    // OLD WAY OF DOING FROM WHEN THE PPU DIDN'T PROPERLY MIRROR 0X3000 TO 0X4000 TO 0X2000 TO 0X3000
    //return map_color(PPU_Read(ppu, PPU_PALETTE_OFFSET + palette * 4 + pixel));

    // FIXME: MAY BE WRONG B/C OF LACK OF MIRRORING???
    return MapColor(this->palette[palette * 4 + pixel]);
}

/* Constructors/Destructors */
PPU::PPU(Bus& _bus) : bus(_bus) {
    PPU* ppu = this;
    Util_MemsetU32((uint32_t*)ppu->sprpatterntbl, 0xff000000,
        sizeof(ppu->sprpatterntbl)/sizeof(uint32_t));
    Util_MemsetU32((uint32_t*)ppu->screen, 0xff000000,
        sizeof(ppu->screen)/sizeof(uint32_t));
    Util_MemsetU32((uint32_t*)ppu->frame_buffer, 0xff000000,
        sizeof(ppu->frame_buffer)/sizeof(uint32_t));
    memset(ppu->palette_overrides, false, sizeof(ppu->palette_overrides));
}

/* Interrupts (technically the PPU has no notion of interrupts) */
// https://www.nesdev.org/wiki/PPU_rendering
void PPU::Clock() {
    PPU* ppu = this;
    // TODO: MAY WANNA DECOUPEL THE FG RENDER FROM THE BG RENDER

    // FIXME: SPRITE 0 COLLLIISION IS NOT FULLY CORRECT
    //        OR WE HAVE SOME TIMING DESYNC ISSUE, POSSIBLY
    //        WITH DMA
    //        BUT WE FREEZE IN MARIO RANDOMLY
    //        BECAUSE OF SPRITE 0 HITS

    // NES rendered in 340x260p, with many invisible pixels in the
    // overscan area. NES actually displayed in 256x240p, but many TVs
    // did not display that full resolution, so many emulators cut off the
    // top and bottom of the screen

    // Note that a lot of stuff in here will seem weird because cycle 0 is a
    // dummy cycle

    // Prerender and visible scanlines
    if (ppu->scanline >= -1 && ppu->scanline < PPU::RESOLUTION_Y) {
        uint16_t my_off;

        // Visible cycles and preparation cycles
        // The numbers here seem really weird, as you would think we would only
        // be off by 1, but if we started at 1, we would end up ignoring
        // the loaded bg shifters from the previous scanline loaded in the
        // horizontal blanking period. Similar logic
        // applies to the 321, except there we actually do want to load new
        // shifters, so we start there instead of at 322. Alternatively I'm
        // wrong and this is a weird quirk of OLC's implementation.
        // TODO: TEST THE BOUNDARIES HERE WITH A MORE COMPLICATED GAME TO SEE
        //       IF THERE IS ANY CLIPPING
        // NOTE: NO PERCIEVED DIFFERENCE BETWEEN USING 1 AND 2 HERE, I WILL
        //      STICK WITH 2 SINCE THAT IS WHAT OLC HAS

        // PROBABLY SHOULD BE 1 SINCE VERYTHING ELSE SEEMS TO USE 1
        if ((ppu->cycle >= 2 && ppu->cycle < 258)
            || (ppu->cycle >= 321 && ppu->cycle < 338)) {
            // NOTE: If we move this to the bottom we might only wanna do one cycle
            // cuz obviously this will get shifted before we did anything if starting
            // at one
            UpdateShifters();

            switch ((ppu->cycle - 1) % 8) {
            case 0:
                // Read next tile id
                // Tile id is read from the nametable, but I only want the
                // bottom 12 bits, hence the & 0x0fff
                LoadBGShifters();
                ppu->bg_next_tile_id = Read(NAMETBL_OFFSET | (ppu->vram_addr & 0x0fff));
                break;
            case 2:
                // Read attribute information (extra tiles at bottom of
                // nametbl that give color info)

                // Get the nametable bits
                my_off = ppu->vram_addr & 0x0c00;

                // Keep the top 3 bits of coarse y and x
                // and put them in the right place
                my_off |= ((ppu->vram_addr & LOOPY_COARSE_Y) >> 7) << 3;
                my_off |= ((ppu->vram_addr & LOOPY_COARSE_X) >> 2) << 0;

                // 0x23c0 is the starting address of the attribute memory
                ppu->bg_next_tile_attr = Read(0x23c0 | my_off);

                // Final info is only 2 bits
                // Doing some basic arithmetic, we can determine that one byte
                // in the attribute memory actually coressponds to a group of 4 tiles
                // we only need 2 bits to represent a color. so we do some arithmetic
                // to get the appropriate palette for each tile
                if (((ppu->vram_addr & LOOPY_COARSE_Y) >> 5) & 0x02)
                    ppu->bg_next_tile_attr >>= 4;
                if (((ppu->vram_addr & LOOPY_COARSE_X) >> 0) & 0x02)
                    ppu->bg_next_tile_attr >>= 2;
                ppu->bg_next_tile_attr &= 0x03;

                break;
            case 4:
                // Get LSB
                // The control register tells us what half of pattern memory to read from
                // which is why our bg_next_tile_id is only 8-bits, since it only needs
                // to cover 256 tiles instead of the whole 512
                my_off = ((ppu->control & PPU_CTRL_BG_TILE_SELECT) >> 4) << 12;
                my_off += ((uint16_t)ppu->bg_next_tile_id << 4);
                my_off += (ppu->vram_addr & LOOPY_FINE_Y) >> 12;

                ppu->bg_next_tile_lsb = Read(my_off);
                break;
            case 6:
                // Get MSB
                my_off = ((ppu->control & PPU_CTRL_BG_TILE_SELECT) >> 4) << 12;
                my_off += ((uint16_t)ppu->bg_next_tile_id << 4);
                my_off += (ppu->vram_addr & LOOPY_FINE_Y) >> 12;

                ppu->bg_next_tile_msb = Read(my_off + 8);
                break;
            case 7:
                IncrementScrollX();
                break;
            }
        } else if (ppu->scanline == -1 && ppu->cycle == 1) {
            // We have hit the top of the screen again, so clear VBLANK
            ppu->status &= ~PPU_STATUS_VBLANK;
            ppu->status &= ~PPU_STATUS_SPR_OVERFLOW;
            ppu->status &= ~PPU_STATUS_SPR_HIT;

            // optimization (maybe?)
            //ppu->status &= ~(PPU_STATUS_VBLANK | PPU_STATUS_SPR_OVERFLOW
            //      | PPU_STATUS_SPR_HIT);

            for (int i = 0; i < SPR_PER_LINE; i++) {
                ppu->spr_shifter_pattern_lo[i] = 0;
                ppu->spr_shifter_pattern_hi[i] = 0;
            }
        }
            // There is a weird quirk about the prerender scanline that makes
            // this cycle get skipped
        else if (ppu->scanline == 0 && ppu->cycle == 0)
            ppu->cycle = 1;
        else if (ppu->scanline == -1 && ppu->cycle >= 280 && ppu->cycle < 305)
            TransferAddrY();

        // First invisible cycle, increment to next scanline
        if (ppu->cycle == RESOLUTION_X)
            IncrementScrollY();
        // Prepare tiles for next scanline
        else if (ppu->cycle == RESOLUTION_X + 1) {
            LoadBGShifters();
            TransferAddrX();

            // May wanna put this in a separate if for readability
            if (ppu->scanline >= 0) {
                // Set sprite info to 0xff, because if the y-coord of the sprite
                // is 0xff, we will never see it
                memset(ppu->spr_scanline, 0xff, SPR_PER_LINE * sizeof(OAM));
                ppu->spr_count = 0;
                ppu->spr0_can_hit = false;

                int oam_entry = 0;

                // FIXME: WON'T PROPERLY DETECT OVRFLOW
                while (oam_entry < 64 && ppu->spr_count < 9) {
                    // FIXME: MAYBE NEED TO EXPLICITLY CAST TO SIGNED
                    int diff = ppu->scanline - ppu->oam[oam_entry].y;

                    // Sprites can be 16 or 8px tall depending on the mode
                    if (diff >= 0 && diff < ((ppu->control & PPU_CTRL_SPR_HEIGHT) ? 16 : 8)) {
                        // Hit a sprite that will be visible

                        // FIXME: THIS SHOULD BE 9
                        // If I haven't overflowed sprites, copy this sprite's pixel for the line
                        // from the oam
                        if (ppu->spr_count < 8) {
                            if (oam_entry == 0)
                                ppu->spr0_can_hit = true;

                            // TODO: Don't use memcpy, it's clearer to do explicitly
                            memcpy(&ppu->spr_scanline[ppu->spr_count], &ppu->oam[oam_entry],
                                sizeof(OAM));
                            ppu->spr_count++;
                        }
                    }

                    oam_entry++;
                }

                // Set sprite overflow flag
                if (ppu->spr_count > 8)
                    ppu->status |= PPU_STATUS_SPR_OVERFLOW;
                else
                    ppu->status &= ~PPU_STATUS_SPR_OVERFLOW;
            }
        }
        // Dummy reads that shouldn't affect anything (but could maybe due to
        // reading changing the state)
        else if (ppu->cycle == 338 || ppu->cycle == 340) {
            ppu->bg_next_tile_id = Read(NAMETBL_OFFSET | (ppu->vram_addr & 0x0fff));

            // NOTE: THIS IS WHERE THE INACCURACY COMES INTO PLAY (I THINK)
            // May wanna denest this and make 340 its own if
            if (ppu->cycle == 340) {
                for (int i = 0; i < ppu->spr_count; i++) {
                    // FIXME: CLEAN THIS CLUSTERFUCK UP
                    uint8_t spr_pattern_bits_lo, spr_pattern_bits_hi;
                    uint16_t spr_pattern_addr_lo, spr_pattern_addr_hi;

                    if (ppu->control & PPU_CTRL_SPR_HEIGHT) {
                        // 16x8
                        if (ppu->spr_scanline[i].attributes & (1 << 7)) {
                            // flipped

                            if (ppu->scanline - ppu->spr_scanline[i].y < 8) {
                                // top
                                uint16_t pattern_tbl = (ppu->spr_scanline[i].tile_id & 1) << 12;
                                spr_pattern_addr_lo = pattern_tbl
                                    | (((ppu->spr_scanline[i].tile_id & 0xfe) + 1) << 4)
                                    | (7 - (ppu->scanline - ppu->spr_scanline[i].y) % 8);
                            }
                            else {
                                // bottom
                                uint16_t pattern_tbl = (ppu->spr_scanline[i].tile_id & 1) << 12;
                                spr_pattern_addr_lo = pattern_tbl
                                    | (((ppu->spr_scanline[i].tile_id & 0xfe) + 0) << 4)
                                    | (7 - (ppu->scanline - ppu->spr_scanline[i].y) % 8);
                            }


                        }
                        else {
                            // not flippped

                            if (ppu->scanline - ppu->spr_scanline[i].y < 8) {
                                // top
                                uint16_t pattern_tbl = (ppu->spr_scanline[i].tile_id & 1) << 12;
                                spr_pattern_addr_lo = pattern_tbl
                                    | ((ppu->spr_scanline[i].tile_id & 0xfe) << 4)
                                    | ((ppu->scanline - ppu->spr_scanline[i].y) % 8);
                            }
                            else {
                                // bottom
                                uint16_t pattern_tbl = (ppu->spr_scanline[i].tile_id & 1) << 12;
                                spr_pattern_addr_lo = pattern_tbl
                                    | (((ppu->spr_scanline[i].tile_id & 0xfe) + 1) << 4)
                                    | ((ppu->scanline - ppu->spr_scanline[i].y) % 8);
                            }
                        }
                    }
                    else {
                        // 8x8
                        if (ppu->spr_scanline[i].attributes & (1 << 7)) {
                            // flipped
                            uint16_t pattern_tbl = (ppu->control & PPU_CTRL_SPR_TILE_SELECT) == PPU_CTRL_SPR_TILE_SELECT;
                            pattern_tbl <<= 12;

                            spr_pattern_addr_lo = pattern_tbl
                                | (ppu->spr_scanline[i].tile_id << 4)
                                | (7 - (ppu->scanline - ppu->spr_scanline[i].y));
                        }
                        else {
                            // not flippped
                            uint16_t pattern_tbl = (ppu->control & PPU_CTRL_SPR_TILE_SELECT) == PPU_CTRL_SPR_TILE_SELECT;
                            pattern_tbl <<= 12;

                            spr_pattern_addr_lo = pattern_tbl
                                | (ppu->spr_scanline[i].tile_id << 4)
                                | (ppu->scanline - ppu->spr_scanline[i].y);
                        }
                    }

                    // Recall pixel data is always 8 bytes apart
                    spr_pattern_addr_hi = spr_pattern_addr_lo + 8;
                    spr_pattern_bits_lo = Read(spr_pattern_addr_lo);
                    spr_pattern_bits_hi = Read(spr_pattern_addr_hi);

                    if (ppu->spr_scanline[i].attributes & (1 << 6)) {
                        // horizontal flipped
                        spr_pattern_bits_lo = flip_bits(spr_pattern_bits_lo);
                        spr_pattern_bits_hi = flip_bits(spr_pattern_bits_hi);
                    }

                    ppu->spr_shifter_pattern_lo[i] = spr_pattern_bits_lo;
                    ppu->spr_shifter_pattern_hi[i] = spr_pattern_bits_hi;

                }
            }
        }

        /* FG RENDERERING */
        // FIXME: THIS IS CHEATING, ALL OF THE SPRITE STUFF IS SUPPOSED TO HAPPEN IN MANY CYCLES
        //        BUT FOR EASE OF CODING, WE DO IT IN ONE
        //        THIS WILL PROBABLY BREAK MANY GAMES *COUGH* BATTLETOADS *COUGH*
    }
    // Dummy scanline, do nothing
    else if (ppu->scanline == PPU::RESOLUTION_Y) {

    } else if (ppu->scanline > PPU::RESOLUTION_Y && ppu->scanline <= 260) {
        // Enter the VBLANK period and emit an NMI if the control register says to
        if (ppu->scanline == 241 && ppu->cycle == 1) {
            ppu->status |= PPU_STATUS_VBLANK;
            if (ppu->control & PPU_CTRL_NMI)
                ppu->nmi = true;

            // TODO: see if triple buffering may be beneficial
            //       we currently use double buffering, which means
            //       if we manage to lap the renderer, we actually have to
            //       wait for it to finish

            // This is the point at which we are done rendering the frame,
            // so we copy the current screen to the frame buffer here

            // Don't want to write to buffer if renderer is still rendering

            // Copy the new frame to the frame_buffer
            memcpy(ppu->frame_buffer, ppu->screen, sizeof(ppu->screen));

        }
    }
    else {
        printf("PPU_Clock: invalid scanline value\n");
    }

    uint8_t bg_pixel = 0x00;
    uint8_t bg_palette = 0x00;

    // Only render bg if we are supposed to
    if (ppu->mask & PPU_MASK_BG_ENABLE) {
        // Bit mask for the top bit shifted by fine_x for if there is
        // horizontal scrolling involved
        // (I think only for horz scroll, but don't quote me on that)
        uint16_t bit_mux = 0x8000 >> ppu->fine_x;

        // Calculate pixel offset
        // TODO: I THINK I CAN CHANGE THE > TO A !=
        uint8_t p0_pixel = (ppu->bg_shifter_pattern_lo & bit_mux) != 0;
        uint8_t p1_pixel = (ppu->bg_shifter_pattern_hi & bit_mux) != 0;

        bg_pixel = (p1_pixel << 1) | p0_pixel;

        // Calculate the palette
        // TODO: I THINK I CAN CHANGE THE > TO A !=
        uint8_t bg_pal0 = (ppu->bg_shifter_attr_lo & bit_mux) != 0;
        uint8_t bg_pal1 = (ppu->bg_shifter_attr_hi & bit_mux) != 0;

        bg_palette = (bg_pal1 << 1) | bg_pal0;
    }

    uint8_t fg_pixel = 0;
    uint8_t fg_palette = 0;
    bool fg_back = false;

    if (ppu->mask & PPU_MASK_SPR_ENABLE) {
        // FIXME: MAY NEED TO FIX THIS FLAG REGARDLESS OF IF WE RENDER SPRITES????
        ppu->spr0_rendering = false;

        for (int i = 0; i < ppu->spr_count; i++) {
            if (ppu->spr_scanline[i].x == 0) {
                uint8_t fg_pixel_lo = (ppu->spr_shifter_pattern_lo[i] & (1 << 7)) != 0;
                uint8_t fg_pixel_hi = (ppu->spr_shifter_pattern_hi[i] & (1 << 7)) != 0;
                fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;

                // Bottom 2 bits tell me the palette, but recall the first 4 palettes
                // are for bg, so we need to add 4
                fg_palette = (ppu->spr_scanline[i].attributes & 0x3) + 4;

                // Priority of sprites is implicit in their ordering in OAM
                // but this extracts the priority of the sprite relative to the background
                fg_back = ppu->spr_scanline[i].attributes & (1 << 5);

                if (fg_pixel != 0) {
                    if (i == 0)
                        ppu->spr0_rendering = true;
                    break;
                }
            }
        }
    }

    // Combine bg and fg pixel to get final value
    uint8_t final_pixel = 0;
    uint8_t final_palette = 0;

    if (bg_pixel == 0 && fg_pixel == 0) {
        // nothing to do since pixel will be 0 (aka transparent)
    }
    else if (bg_pixel == 0 && fg_pixel > 0) {
        // fg takes priority since it is visble and bg is not
        final_pixel = fg_pixel;
        final_palette = fg_palette;
    }
    else if (bg_pixel > 0 && fg_pixel == 0) {
        // bg takes priority since it is the only non-transparent pixel
        final_pixel = bg_pixel;
        final_palette = bg_palette;
    }
    else {
        // need to compare priority to figure out who goes in front
        if (fg_back) {
            final_pixel = bg_pixel;
            final_palette = bg_palette;
        }
        else {

            final_pixel = fg_pixel;
            final_palette = fg_palette;
        }

        // check for spr0 hit
        if (ppu->spr0_can_hit && ppu->spr0_rendering) {
            if ((ppu->mask & PPU_MASK_BG_ENABLE) && (ppu->mask & PPU_MASK_SPR_ENABLE)) {
                // FIXME: COPY OLC IF THIS IS WRONG

                // decides if the 8 pixels to the left of the screen should be drawn
                // supposed to be used to make scrolling be less janky on the edges
                // TODO: INVESTIGATE HOW THIS WORKS ON NESDEV
                if (~(ppu->mask & PPU_MASK_BG_LEFT_COLUMN_ENABLE)
                    && ~(ppu->mask & PPU_MASK_SPR_LEFT_COLUMN_ENABLE)) {
                    // can only have hit after first 8 pixels
                    // TODO: INVESTIGATE CORRECNESS OF CYCLE VALUES
                    if (ppu->cycle >= 9 && ppu->cycle < 258) {
                        ppu->status |= PPU_STATUS_SPR_HIT;
                        //printf("enabling spr0_hit\n");
                    }
                }
                else {
                    if (ppu->cycle >= 1 && ppu->cycle < 258) {
                        ppu->status |= PPU_STATUS_SPR_HIT;
                        //printf("enabling spr0_hit\n");
                    }
                }
            }
        }

        // PREVENT FROM GETTING STUCK IN INFINITE LOOP
        // TODO: REMOVE ME
        //ppu->status |= PPU_STATUS_SPR_HIT;

    }

    // FIXME: CHECK FOR CORRECTNESS
    // Set first 8 pixels to the global background color if we aren't enabling
    // the bg left column (maybe also the spr left)
    if (!(ppu->mask & PPU_MASK_BG_LEFT_COLUMN_ENABLE)
        && !(ppu->mask & PPU_MASK_SPR_LEFT_COLUMN_ENABLE) && ppu->cycle < 9) {
        final_pixel = 0;
        final_palette = 0;
    }

    // We write to cycle-1 because cycle 0 is a dummy cycle
    ScreenWrite(ppu->cycle-1, ppu->scanline,
        GetColorFromPalette(final_palette, final_pixel));

    // Properly increment the cycle and scanline
    if ((ppu->mask & PPU_MASK_BG_ENABLE) || (ppu->mask & PPU_MASK_SPR_ENABLE)) {
        if (ppu->cycle == 260 && ppu->scanline < 240) {
            ppu->bus.GetCart().GetMapper()->CountdownScanline();
        }
    }
    ppu->cycle++;

    if (ppu->cycle > 340) {
        ppu->cycle = 0;
        ppu->scanline++;

        if (ppu->scanline > 260) {
            ppu->scanline = -1;
            ppu->frame_complete = true;
        }
    }
}

// https://www.nesdev.org/wiki/PPU_power_up_state
void PPU::PowerOn() {
    // TODO: INITIALIZE MORE SUTFF TO 0
    PPU* ppu = this;

    ppu->control = 0x00;
    ppu->mask = 0x00;

    // this is right, although the way we do it we may have issues
    // getting out of startup if the vblank bit is set like it is
    // in this, hte reset changes to 0x00 with no issues so just leave
    // this
    ppu->status = 0xc0;

    memset(ppu->screen, 0, sizeof(ppu->screen));

    // just run the reset for safety
    Reset();
}

// https://www.nesdev.org/wiki/PPU_power_up_state
void PPU::Reset() {
    PPU* ppu = this;
    // TODO: INITIALIZE MORE SUTFF TO 0

    // FIXME: THERE NEEDS TO BE LOGIC THAT PREVENTS GAME
    //        FROM WRITING TO CERTAIN REGISTERS BEFORE CERTAIN
    //        NUMBER OF CYCLES OCCURRED
    //        ALTHOUGH IDK IF THAT ACTUALLY DOES ANYTHING

    //        ALSO LOTS OF OTHER SPECIALIZED LOGIC THAT PROBABLY DOESN'T
    //        MATTER
    //        ALSO SOME DELAY IN PPU I GUESS, MAYBE IT MATTERS, MAYBE IT
    //          DOESN'T
    //        ALSO ON A TOP LOADER NES, ONLY CPU GETS RESET WHEN
    //          PUSHING RESET BUTTON
    //        VRAM ADDRESS TECHNICALLY UNCLEARED ON RESET, BUT THIS
    //        SEEMS LIKE IT WILL CREATE ALL KINDS OF ISSUES AND
    //        INCONSISTENCIES, SO I WILL JUST CLEAR EVRYTHING

    //        I GUESS THAT STATUS OAMADDR AND OAMDATA PPUDATA AND OAMDMA
    //        ALL WORK AT BOOT WITHOUT THE DELAY??

    ppu->control = 0x00;
    ppu->mask = 0x00;

    // technically this is wrong and should retain previous value, but
    // i've encountered no issues with this os will leave for now
    // although it should at least be the default value instead of 0
    // TODO: REMOVE THIS LINE, IT APPEARS TO BE POINTLESS

    // IT APPEARS THAT NOT WIPING THIS SEEMS POTENTIALLY DANGEROUS WITH CPU MAYBE
    // ENTERING VBLANK TOO EARLY, BUT WE MAY NEED TO CHANGE THIS LATER
    ppu->status = 0x00;

    // MAYBE DOESN'T NEED TO BE HERE
    ppu->oam_addr = 0;

    ppu->spr0_rendering = false;
    ppu->spr0_can_hit = false;

    ppu->spr_count = 0;

    // FIXME: MAY WANNA INVESTIGATE IF THE SCANLINE SHOULD BE -1, ALTHOUGH
    // IF THE FIRST FRAME IS WRONG WHO WILL CARE
    // MY GUT FEELING IS THAT THIS IS CORRECT, AND THAT STARTING ON SCANLINE
    // -1 WILL LEAD TO TIMING ISSUES WITH CPU.
    // nestest LOG WITH PPU SCANLINE AND CYCELS SEEMS TO CONFIRM THIS
    // SAME THING WITH FRAMETIMING DIAGRAM WHICH USES PRERENDER SCANLINE AS
    // 261 INSTEAD OF -1
    ppu->cycle = 0;
    ppu->scanline = 0;

    // FIXME: could be true
    ppu->frame_complete = false;
    ppu->nmi = false;

    // most of these are probably unimportant to set
    // TODO: SEE WHAT I ACTUALLY NEED TO RESET
    ppu->fine_x = 0;
    ppu->addr_latch = 0;
    ppu->data_buffer = 0;
    ppu->bg_next_tile_id = 0;
    ppu->bg_next_tile_attr = 0;
    ppu->bg_next_tile_lsb = 0;
    ppu->bg_next_tile_msb = 0;
    ppu->bg_shifter_pattern_lo = 0;
    ppu->bg_shifter_pattern_hi = 0;
    ppu->bg_shifter_attr_hi = 0;
    ppu->bg_shifter_attr_lo = 0;
    ppu->vram_addr = 0;
    ppu->tram_addr = 0;

}

uint8_t PPU::Read(uint16_t addr) {
    // Address can't be more than 16kb
    PPU* ppu = this;
    // Bus* bus = ppu->bus;
    addr %= 0x4000;

    // chr rom, vram, palette
    if (addr >= 0 && addr < 0x2000) {
        Mapper* mapper = bus.GetCart().GetMapper();
        return mapper->MapPPURead(addr);
    }
    else if (addr >= 0x2000 && addr < 0x4000) {
        // only 1kb in each half of nametable
        addr %= 0x1000;

        Mapper::MirrorMode mirror_mode = bus.GetCart().GetMapper()->GetMirrorMode();

        if (mirror_mode == Mapper::MirrorMode::HORIZONTAL) {
            // see vertical comments for explanation
            if (addr >= 0 && addr < 0x800)
                return ppu->nametbl[0][addr % 0x400];
            else if (addr >= 0x800 && addr < 0x1000)
                return ppu->nametbl[1][addr % 0x400];

        }
        else if (mirror_mode == Mapper::MirrorMode::VERTICAL) {
            // notice how two different address ranges map to the same thing
            // this is because each of the two nametables each contain 1kb of data
            // and we mirror vertically for the remaining parts of the address range
            if (addr >= 0 && addr < 0x400)
                return ppu->nametbl[0][addr % 0x400];
            else if (addr >= 0x400 && addr < 0x800)
                return ppu->nametbl[1][addr % 0x400];
            else if (addr >= 0x800 && addr < 0xc00)
                return ppu->nametbl[0][addr % 0x400];
            else if (addr >= 0xc00 && addr < 0x1000)
                return ppu->nametbl[1][addr % 0x400];
        }
        else if (mirror_mode == Mapper::MirrorMode::ONESCREEN_LO) {
            return ppu->nametbl[0][addr % 0x400];
        }
        else if (mirror_mode == Mapper::MirrorMode::ONESCREEN_HI) {
            return ppu->nametbl[1][addr % 0x400];
        }
        else {
            printf("INVALID MIRROR MODE\n");
        }
    }
    /*
    else if (addr >= 0x3f00 && addr < 0x4000) {
        // only 32 colors
        // we also have mirroring going on top of that for the last pixel transparency effect
        addr %= 32;

        switch (addr) {
        case 0x10:
            addr = 0x00;
            break;
        case 0x14:
            addr = 0x04;
            break;
        case 0x18:
            addr = 0x08;
            break;
        case 0x1c:
            addr = 0x0c;
            break;
        }

        // FIXME: MAY BREAK SOME STUFF
        // palette read also reads vram into the data buffer
        // but we also must prevent infinite recursion
        // FIXME: FAILS TO PASS TEST PROBABYL BECAUSE I STILL NEED TO DO THIS
        // EVEN WITH THESE ADDRESSES
        // FIXME: TRY HARD CODING IT FOR THE PALETTE READ CASE
        // FIXME: DOESN'T WORK, THINKS I'M CHANGING THE DATA BUFFER ON A
        //        PALETTE WRITE EVEN THOUGH THIS IS READ FUNCTION

        if (ppu->vram_addr < 0x3f00 || ppu->vram_addr >= 0x4000)
            PPU_Read(ppu, ppu->vram_addr);
        else {
            uint16_t addr2 = ppu->vram_addr % 32;
            switch (addr2) {
            case 0x10:
                addr2 = 0;
                break;
            case 0x14:
                addr2 = 4;
                break;
            case 0x18:
                addr2 = 8;
                break;
            case 0x1c:
                addr2 = 0x0c;
                break;
            }

            ppu->data_buffer = ppu->palette[addr2];
        }

        return ppu->palette[addr] & ((ppu->mask & PPU_MASK_GREYSCALE) ? 0x30 : 0x3f);
    }*/

    // FIXME: I WOULDN'T BE SURPRISED IF THIS BREAKS SOMETHING THAT ASSUMES RAM DEFAULT VAL OF 0
    return 0xff;
}

bool PPU::Write(uint16_t addr, uint8_t data) {
    PPU* ppu = this;
    // Bus* bus = ppu->bus;
    addr %= 0x4000;

    // chr rom, vram, palette
    if (addr >= 0 && addr < 0x2000) {
        Mapper* mapper = bus.GetCart().GetMapper();
        return mapper->MapPPUWrite(addr, data);
    }
    else if (addr >= 0x2000 && addr < 0x3f00) {
        //printf("writing to naemtable\n");
        // only 1kb in each half of nametable
        addr %= 0x1000;

        Mapper::MirrorMode mirror_mode = bus.GetCart().GetMapper()->GetMirrorMode();
        //printf("writing nametable\n");
        if (mirror_mode == Mapper::MirrorMode::HORIZONTAL) {
            if (addr >= 0 && addr < 0x800) {
                ppu->nametbl[0][addr % 0x400] = data;
                return true;
            }
            else if (addr >= 0x800 && addr < 0x1000) {
                //printf("writing to naemtable1\n");
                ppu->nametbl[1][addr % 0x400] = data;
                return true;
            }
        }
        else if (mirror_mode == Mapper::MirrorMode::VERTICAL) {
            if (addr >= 0 && addr < 0x400) {
                ppu->nametbl[0][addr % 0x400] = data;
                return true;
            }
            else if (addr >= 0x400 && addr < 0x800) {
                ppu->nametbl[1][addr % 0x400] = data;
                return true;
            }
            else if (addr >= 0x800 && addr < 0xc00) {
                ppu->nametbl[0][addr % 0x400] = data;
                return true;
            }
            else if (addr >= 0xc00 && addr < 0x1000) {
                ppu->nametbl[1][addr % 0x400] = data;
                return true;
            }
        }
        else if (mirror_mode == Mapper::MirrorMode::ONESCREEN_LO) {
            ppu->nametbl[0][addr % 0x400] = data;
            return true;
        }
        else if (mirror_mode == Mapper::MirrorMode::ONESCREEN_HI) {
            ppu->nametbl[1][addr % 0x400] = data;
            return true;
        }
        else {
            printf("INVALID MIRROR MODE\n");
            return false;
        }
    }
    else if (addr >= 0x3f00 && addr < 0x4000) {
        // only 32 colors
        // we also have mirroring going on top of that
        addr %= 32;

        switch (addr) {
        case 0x10:
            addr = 0x00;
            break;
        case 0x14:
            addr = 0x04;
            break;
        case 0x18:
            addr = 0x08;
            break;
        case 0x1c:
            addr = 0x0c;
            break;
        }

        ppu->non_overridden_palette[addr] = data;
        if (!ppu->palette_overrides[addr])
            palette[addr] = data;
    }

    return true;
}

uint8_t PPU::RegisterRead(uint16_t addr) {
    PPU* ppu = this;
    uint8_t tmp = 0xff;
    addr %= 8;

    switch (addr) {
    case 0: // control
        tmp = ppu->control;
        break;
    case 1: // mask
        tmp = ppu->mask;
        break;
    case 2: // status
        // only read certain bits
        tmp = (ppu->status & 0xe0) | (ppu->data_buffer & 0x1f);
        // reading the status register clears vblank and resets addr_latch
        ppu->status &= ~PPU_STATUS_VBLANK;
        ppu->addr_latch = 0;
        break;
    case 3: // OAM address (OAM = SPRITE)
        break;
    case 4: // OAM data
        tmp = reinterpret_cast<uint8_t*>(oam)[ppu->oam_addr];
        break;
    case 5: // scroll
        break;
    case 6: // ppu address
        break;
    case 7: // ppu data
        tmp = ppu->data_buffer;
        ppu->data_buffer = Read(ppu->vram_addr);

        // Palette memory isn't delayed by one cycle like the rest of memory

        // FIXME: THIS IS BROKEN IN PPU TEST BECAUSE THE WAY THAT WE READ FROM
        // PALETTE RAM IS WRONG. THE PALETTE RAM IS ALMOST ALWAYS A MIRROR OF 0X2000
        // TO 0X2FFF. WE NEED TO IMPLEMENT THAT AND THEN TELL THAT WHOLE THING
        // WHEN TO READ FROM PALETTE MEM
        // For info on how to fix, read this
        // https://forums.nesdev.org/viewtopic.php?t=22459

        // FIXME: THE PPU NEVER ACTUALLY READS FROM 0X3000-0X3FFF BY ITSELF, SO WE
        // CAN MORE OR LESS MIRROR THE WHOLE THING IN READ, BUT HAVE THE WRITE BEHAVE CORRECTLY

        if (ppu->vram_addr >= 0x3f00) {
            int pal_addr = ppu->vram_addr % 32;
            switch (pal_addr) {
            case 0x10:
                pal_addr = 0;
                break;
            case 0x14:
                pal_addr = 4;
                break;
            case 0x18:
                pal_addr = 8;
                break;
            case 0x1c:
                pal_addr = 0xc;
                break;
            }
            tmp = ppu->palette[pal_addr];
        }
        ppu->vram_addr += (ppu->control & PPU_CTRL_INCREMENT_MODE) ? 32 : 1;
        break;
    }

    return tmp;
}

bool PPU::RegisterWrite(uint16_t addr, uint8_t data) {
    PPU* ppu = this;
    addr %= 8;
    switch (addr) {
    case 0: // control
        ppu->control = data;
        set_loopy_register(&ppu->tram_addr,
            (ppu->control & PPU_CTRL_NAMETBL_SELECT_X) >> 0,
            LOOPY_NAMETBL_X);
        set_loopy_register(&ppu->tram_addr,
            (ppu->control & PPU_CTRL_NAMETBL_SELECT_Y) >> 1,
            LOOPY_NAMETBL_Y);
        break;
    case 1: // mask
        ppu->mask = data;
        break;
    case 2: // status
        break;
    case 3: // OAM address
        ppu->oam_addr = data;
        break;
    case 4: // OAM data
        // FIXME: OLC DOES NOT INCREMENT, BUT TEST ROMS DO
        // NOBODY EVER DOES THIS ANYWAY, SO THIS WORKING
        // IS NOT A HUGE DEAL
        reinterpret_cast<uint8_t*>(oam)[ppu->oam_addr++] = data;
        break;
    case 5: // scroll
        if (ppu->addr_latch == 0) {
            ppu->fine_x = data & 0x07;
            set_loopy_register(&ppu->tram_addr, data >> 3, LOOPY_COARSE_X);
            ppu->addr_latch = 1;
        }
        else {
            set_loopy_register(&ppu->tram_addr, data & 0x07, LOOPY_FINE_Y);
            set_loopy_register(&ppu->tram_addr, data >> 3, LOOPY_COARSE_Y);
            ppu->addr_latch = 0;
        }
        break;
    case 6: // PPU address
        // sets hi byte then lo byte
        if (ppu->addr_latch == 0) {
            ppu->tram_addr = (ppu->tram_addr & 0x00ff)
                | ((data & 0x3f) << 8);
            ppu->addr_latch = 1;
        }
        else {
            ppu->tram_addr = (ppu->tram_addr & 0xff00) | data;
            ppu->vram_addr = ppu->tram_addr;
            ppu->addr_latch = 0;
        }
        break;
    case 7: // ppu data
        // write to address with auto increment
        // FIXME: MIGHT ALSO HAVE THE DELAY LIEK READ????
        Write(ppu->vram_addr, data);
        // recall the nametable is 32x32, so based on increment mode we either
        // increment by one row (32) or one pixel/col (1)
        ppu->vram_addr += (ppu->control & PPU_CTRL_INCREMENT_MODE) ? 32 : 1;
        break;
    }

    return true;
}

uint8_t PPU::RegisterInspect(uint16_t addr) {

    PPU* ppu = this;
    uint8_t tmp = 0xff;
    addr %= 8;

    switch (addr) {
    case 0: // control
        tmp = ppu->control;
        break;
    case 1: // mask
        tmp = ppu->mask;
        break;
    case 2: // status
        // only read certain bits
        tmp = (ppu->status & 0xe0) | (ppu->data_buffer & 0x1f);
        // reading the status register clears vblank and resets addr_latch
        //ppu->status &= ~PPU_STATUS_VBLANK;
        //ppu->addr_latch = 0;
        break;
    case 3: // OAM address (OAM = SPRITE)
        break;
    case 4: // OAM data
        tmp = reinterpret_cast<uint8_t*>(oam)[ppu->oam_addr];
        break;
    case 5: // scroll
        break;
    case 6: // ppu address
        break;
    case 7: // ppu data
        tmp = ppu->data_buffer;
        //ppu->data_buffer = PPU_Read(ppu, ppu->vram_addr);

        // Palette memory isn't delayed by one cycle like the rest of memory
        if (ppu->vram_addr >= 0x3f00)
            tmp = Read(ppu->vram_addr);
        //ppu->vram_addr += (ppu->control & PPU_CTRL_INCREMENT_MODE) ? 32 : 1;
        break;
    }

    return tmp;
}

bool PPU::GetNMIStatus() {
    return nmi;
}

void PPU::ClearNMIStatus() {
    nmi = false;
}

void PPU::WriteOAM(uint8_t addr, uint8_t data) {
    auto oam_ptr = reinterpret_cast<uint8_t*>(oam);
    oam_ptr[addr] = data;
}

bool PPU::GetFrameComplete() {
    return frame_complete;
}

void PPU::ClearFrameComplete() {
    frame_complete = false;
}

uint32_t* PPU::GetFramebuffer() {
    return frame_buffer;
}

void to_json(nlohmann::json& j, const PPU& ppu) {
    j = nlohmann::json {
        // Save space and time by not saving these
        // user won't be able to tell about a 1 frame mess up
        // {"screen", ppu.screen},
        // {"frame_buffer", ppu.frame_buffer},
        {"nametbl0", ppu.nametbl[0]},
        {"nametbl1", ppu.nametbl[1]},
        {"palette", ppu.palette},
        {"oam", ppu.oam},
        {"oam_addr", ppu.oam_addr},
        {"spr_scanline", ppu.spr_scanline},
        {"spr_count", ppu.spr_count},
        {"spr_shifter_pattern_lo", ppu.spr_shifter_pattern_lo},
        {"spr_shifter_pattern_hi", ppu.spr_shifter_pattern_hi},
        {"spr0_can_hit", ppu.spr0_can_hit},
        {"spr0_rendering", ppu.spr0_rendering},

        // not saving spr patterntbl as of now since it gets overwritten every frame
        // and will not affect the ppu internals at all, and it would take forever to save
        // given how this thing won't serialize multidimensional arrays

        {"scanline", ppu.scanline},
        {"cycle", ppu.cycle},
        {"status", ppu.status},
        {"mask", ppu.mask},
        {"control", ppu.control},
        {"vram_addr", ppu.vram_addr},
        {"tram_addr", ppu.tram_addr},
        {"fine_x", ppu.fine_x},
        {"addr_latch", ppu.addr_latch},
        {"data_buffer", ppu.data_buffer},

        {"bg_next_tile_id", ppu.bg_next_tile_id},
        {"bg_next_tile_attr", ppu.bg_next_tile_attr},
        {"bg_next_tile_lsb", ppu.bg_next_tile_lsb},
        {"bg_next_tile_msb", ppu.bg_next_tile_msb},

        {"bg_shifter_pattern_lo", ppu.bg_shifter_pattern_lo},
        {"bg_shifter_pattern_hi", ppu.bg_shifter_pattern_hi},

        {"bg_shifter_attr_lo", ppu.bg_shifter_attr_lo},
        {"bg_shifter_attr_hi", ppu.bg_shifter_attr_hi},

        {"nmi", ppu.nmi},
        {"frame_complete", ppu.frame_complete},
    };
}

void from_json(const nlohmann::json& j, PPU& ppu) {
    // j.at("screen").get_to(ppu.screen);
    // j.at("frame_buffer").get_to(ppu.frame_buffer);
    j.at("nametbl0").get_to(ppu.nametbl[0]);
    j.at("nametbl1").get_to(ppu.nametbl[1]);
    j.at("palette").get_to(ppu.palette);
    j.at("oam").get_to(ppu.oam);
    j.at("oam_addr").get_to(ppu.oam_addr);
    j.at("spr_scanline").get_to(ppu.spr_scanline);
    j.at("spr_count").get_to(ppu.spr_count);
    j.at("spr_shifter_pattern_lo").get_to(ppu.spr_shifter_pattern_lo);
    j.at("spr_shifter_pattern_hi").get_to(ppu.spr_shifter_pattern_hi);
    j.at("spr0_can_hit").get_to(ppu.spr0_can_hit);
    j.at("spr0_rendering").get_to(ppu.spr0_rendering);
    j.at("scanline").get_to(ppu.scanline);
    j.at("cycle").get_to(ppu.cycle);
    j.at("status").get_to(ppu.status);
    j.at("mask").get_to(ppu.mask);
    j.at("control").get_to(ppu.control);
    j.at("vram_addr").get_to(ppu.vram_addr);
    j.at("tram_addr").get_to(ppu.tram_addr);
    j.at("fine_x").get_to(ppu.fine_x);
    j.at("addr_latch").get_to(ppu.addr_latch);
    j.at("data_buffer").get_to(ppu.data_buffer);
    j.at("bg_next_tile_id").get_to(ppu.bg_next_tile_id);
    j.at("bg_next_tile_attr").get_to(ppu.bg_next_tile_attr);
    j.at("bg_next_tile_lsb").get_to(ppu.bg_next_tile_lsb);
    j.at("bg_next_tile_msb").get_to(ppu.bg_next_tile_msb);
    j.at("bg_shifter_pattern_lo").get_to(ppu.bg_shifter_pattern_lo);
    j.at("bg_shifter_pattern_hi").get_to(ppu.bg_shifter_pattern_hi);
    j.at("bg_shifter_attr_lo").get_to(ppu.bg_shifter_attr_lo);
    j.at("bg_shifter_attr_hi").get_to(ppu.bg_shifter_attr_hi);

    j.at("nmi").get_to(ppu.nmi);
    j.at("frame_complete").get_to(ppu.frame_complete);

}
}
