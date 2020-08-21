// Cpu2Efb methods.

// From GXEnum.h

// Compare function.

enum class GXCompare
{
    GX_NEVER,
    GX_LESS,
    GX_EQUAL,
    GX_LEQUAL,
    GX_GREATER,
    GX_NEQUAL,
    GX_GEQUAL,
    GX_ALWAYS
};

// Determines value of alpha read from a frame buffer with no alpha channel.
// GX_READ_00 or GX_READ_FF returns 0x00 or 0xFF respectedly as alpha value when you call GXPeekARGB. GX_READ_NONE will be used if you want to get alpha value from the EFB. If the EFB doesn't have alpha plane, the alpha value is undefined.

enum class GXAlphaReadMode
{
    GX_READ_00,
    GX_READ_FF,
    GX_READ_NONE
};

// Blending mode.

enum class GXBlendMode
{
    GX_BM_NONE,
    GX_BM_BLEND, 			// dst_pix_clr = src_pix_clr * src_factor + dst_pix_clr * dst_factor
    GX_BM_LOGIC,			// blended using logical bitwise operations
    GX_BM_SUBTRACT, 		// dst_pix_clr = dst_pix_clr - src_pix_clr [clamped to zero]
    GX_MAX_BLENDMODE
};

enum class GXBlendFactor
{
    GX_BL_ZERO, 			// 0.0
    GX_BL_ONE, 			// 1.0
    GX_BL_SRCCLR, 		// source color
    GX_BL_INVSRCCLR, 	// 1.0 - (source color)
    GX_BL_SRCALPHA, 		// source alpha
    GX_BL_INVSRCALPHA, 		// 1.0 - (source alpha)
    GX_BL_DSTALPHA, 		// frame buffer alpha
    GX_BL_INVDSTALPHA, 		// 1.0 - (frame buffer alpha)
    GX_BL_DSTCLR = GX_BL_SRCCLR,		// frame buffer color
    GX_BL_INVDSTCLR = GX_BL_INVSRCCLR 		// 1.0 - (frame buffer color)
};

enum class GXLogicOp
{
    GX_LO_CLEAR, 		// dst = 0
    GX_LO_AND, 			// dst = src & dst
    GX_LO_REVAND,		// dst = src & ~dst
    GX_LO_COPY,			// dst = src
    GX_LO_INVAND, 		// dst = ~src & dst
    GX_LO_NOOP, 		// dst = dst
    GX_LO_XOR, 			// dst = src ^ dst
    GX_LO_OR, 			// dst = src | dst
    GX_LO_NOR, 			// dst = ~(src | dst)
    GX_LO_EQUIV, 		// dst = ~(src ^ dst)
    GX_LO_INV,			// dst = ~dst
    GX_LO_REVOR, 		// dst = src | ~dst
    GX_LO_INVCOPY, 		// dst = ~src
    GX_LO_INVOR,		// dst = ~src | dest
    GX_LO_NAND,			// dst = ~(src & dst)
    GX_LO_SET 			// dst = 1
};

// From GXInit.c

volatile uint16_t * __peReg;        // OSPhysicalToUncached (0x0C001000)

#define PE_POKE_ZMODE_ID 0
#define PE_POKE_CMODE0_ID 1
#define PE_POKE_CMODE1_ID 2
#define PE_POKE_AMODE0_ID 3
#define PE_POKE_AMODE1_ID 4

#define PI_MEMSP_EFB    0x08000000

// This function sets a threshold which is compared to the alpha of pixels written to the Embedded Frame Buffer (EFB) using the GXPoke* functions. The GXPoke* functions allow the CPU to write directly to the EFB.  The compare function order is:
// src_alpha  func  threshold
// for example: src_alpha > 0x80
// The alpha compare function can be used to conditionally write pixels to the EFB using the source alpha channel as a template. If the compare function is true, the source color will be written to the EFB based on the result of the Z compare (see GXPokeZMode).
// If the alpha compare function is false, the source color is not written to the EFB.
// The alpha compare test happens before the Z compare and before blending (see GXPokeBlendMode).

void GXPokeAlphaMode(GXCompare func, uint8_t threshold)
{
	__peReg[PE_POKE_AMODE0_ID] = ((uint16_t)threshold << 8) | (func & 7);
}

// This function determines what value of alpha will be read from the Embedded Frame Buffer (EFB).The mode only applies to GXPeek* functions.  The GXPeek* functions allow the CPU to directly read the EFB.
// Note that this feature works no matter what pixel type (see GXSetPixelFmt) you are using. If you are using the EFB with alpha plane, it is recommended that you should use GX_READ_NONE so that you can 
// read correct alpha value from the EFB. If you are using the EFB with no alpha, you should set either of GX_READ_00 or GX_READ_FF in order to get certain value.

void GXPokeAlphaRead(GXAlphaReadMode mode)
{
	ASSERTMSG ( (mode & 3) == 0, "GX Internal: Register field out of range" );

	__peReg[PE_POKE_AMODE1_ID] = (mode & 3) | 4;
}

// This function enables or disables alpha-buffer updates for GXPoke* functions.  The normal rendering state (GXSetAlphaUpdate) is not effected.

void GXPokeAlphaUpdate(bool update_enable)
{
    ASSERTMSG ( (update_enable & 1) == 0, "GX Internal: Register field out of range" );

    uint16_t old = __peReg[PE_POKE_CMODE0_ID];
    __peReg[PE_POKE_CMODE0_ID] = (old & ~0x10) | ((update_enable & 1) << 4);     // PEReg[2].bit11 = update_enable (PowerPC bit-ordering)
}

// This function determines how the source image, written using the GXPoke* functions, is blended with the current Embedded Frame Buffer (EFB).  
// When type is set to GX_CM_NONE, no color data is written to the EFB.  When type is set to GX_CM_BLEND, the source and EFB pixels are blended using the following equation:
// dst_pix_clr = src_pix_clr * src_factor + dst_pix_clr * dst_factor
// The dst_factor can be used only when the frame buffer has GX_PF_RGBA6_Z24 as the pixel format (see GXSetPixelFmt). 
// When type is set to GX_CM_LOGIC, the source and EFB pixels are blended using logical bitwise operations.
// This function does not effect the normal rendering state, GXSetBlendMode.

// HW2 adds a new type: GX_BM_SUBTRACT.    When this type is used, the destination pixel is computed as follows:
// dst_pix_clr = dst_pix_clr - src_pix_clr [clamped to zero]
// Note that src_factor and dst_factor are not part of the equation.

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op)
{
    uint16_t old = __peReg[PE_POKE_CMODE0_ID];

    // Set mode bits

    switch (type)
    {
        case GX_BM_BLEND:
        case GX_BM_SUBTRACT:
            old &= ~2;      // Disable logic_op

            old &= ~1;
            old |= 1;       // Enable blend_mode

            old &= ~(1 << 11);      // Enable bm_subtract
            if (type == GX_BM_SUBTRACT)
            {
                old |= (1 << 11);
            }
            break;

        case GX_BM_LOGIC:
            old &= ~1;      // Disable blend_mode

            old &= ~2;      // Enable logic_op
            old |= 2;
            break;

        case GX_BM_NONE:
            old &= ~3;      // Disable blend_mode, logic_op
            break;
    }

    // Factors

    old &= ~0x0700;
    old |= ((src_factor & 7) << 8);

    old &= ~0x00E0;
    old |= ((dst_factor & 7) << 5);

    // Logic op

    old &= ~0xF000;
    old |= ((op & 0xF) << 12);

    // Writeback

    __peReg[PE_POKE_CMODE0_ID] = old;
}

// This function enables or disables color-buffer updates when writing the Embedded Frame Buffer (EFB) using the GXPoke* functions.  The GXPoke* functions allow direct access the the EFB by the CPU.

void GXPokeColorUpdate(bool update_enable)
{
    ASSERTMSG ( (update_enable & 1) == 0, "GX Internal: Register field out of range" );

    uint16_t old = __peReg[PE_POKE_CMODE0_ID];
    __peReg[PE_POKE_CMODE0_ID] = (old & ~8) | ((update_enable & 1) << 3);     // PEReg[2].bit12 = update_enable (PowerPC bit-ordering)
}

// This function sets a constant alpha value for writing to the Embedded Frame Buffer (EFB) using the GXPoke* functions.  
// The GXPoke* functions allow the CPU direct access to the EFB. The EFB pixel type must have an alpha channel for this function to be effective, see GXSetPixelFmt.   
// The blending operations (see GXPokeBlendMode) still use source alpha but when writing the pixel color, the constant alpha will replace the pixel alpha in the EFB.

void GXPokeDstAlpha(bool enable, uint8_t alpha)
{
    __peReg[PE_POKE_CMODE1_ID] = ((enable & 1) << 8) | alpha;
}

// The dither enable is only valid when the pixel format (see GXSetPixelFmt) is either GX_PF_RGBA6_Z24 or GX_PF_RGB565_Z16.  
// This function enables dithering when writing the Embedded Frame Buffer (EFB) using GXPoke* functions.  The GXPoke* functions allow the CPU to write directly to the EFB.
// A 4x4 Bayer matrix is used for dithering.

void GXPokeDither(bool dither )
{
    ASSERTMSG ( (dither & 1) == 0, "GX Internal: Register field out of range" );

    uint16_t old = __peReg[PE_POKE_CMODE0_ID];
    __peReg[PE_POKE_CMODE0_ID] = (old & ~4) | ((dither & 1) << 2);     // PEReg[2].bit13 = dither (PowerPC bit-ordering)
}

// This function sets the Z-buffer compare mode when writing the Embedded Frame Buffer (EFB) using the GXPoke* functions.   The GXPoke* functions allow the CPU to directly write the EFB. 
// The result of the Z compare is used to conditionally write color values to the EFB.   The Z value will be updated according to the result of the compare if Z update is enabled.
// When compare_enable is set to GX_DISABLE, poke Z buffering is disabled and the Z buffer is not updated.
// The func parameter determines the comparison that is performed.  In the comparison function, the poked Z value is on the left while the Z value from the Z buffer is on the right.  If the result of the comparison is false, the poked Z value is discarded.
// The parameter update_enable determines whether or not the Z buffer is updated with the new Z value after a comparison is performed. 
// The normal rendering Z mode (GXSetZMode) is not affected by this function.

void GXPokeZMode(bool compare_enable, GXCompare func, bool update_enable )
{
    __peReg[PE_POKE_ZMODE_ID] = (compare_enable & 1) | ((func & 7) << 1) | ((update_enable & 1) << 4);
}

// This function allows the CPU to write a z value directly to the Embedded Frame Buffer (EFB) at position x,y.    
// The z value can be compared with the current contents of the EFB.  The Z compare fuction is set using GXPokeZMode. The z value should be in the range of 0x00000000 <= z < 0x00FFFFFF in the case of non-antialiased frame buffer.
// For an antialiased frame buffer, the z value should be in the compressed 16-bit format (0x00000000 <= z <= 0x0000FFFF), and the poke will affect all 3 subsamples of a pixel.
// To convert a 24-bit integer value into compressed 16-bit form, you can use the function GXCompressZ16.

// x: x coordinate, in pixels.  0 <= x <= 639.
// y: y coordinate, in lines.  0 <= y <= 527.
// z: Integer Z value to write at position (x,y) in the EFB.  0x00000000 <= z <= 0x00FFFFFF.

void GXPokeZ(uint16_t x, uint16_t y, uint32_t z )
{
    uint32_t addr = OSPhysicalToUncached (PI_MEMSP_EFB);
    addr |= (x << 2) | (y << 12) | 0x00400000;          // Z plane

    *(uint32_t *)addr = z;
}

// This function allows the CPU to read a z value directly from the Embedded Frame Buffer (EFB) at position x,y. The z value is raw integer value from the Z buffer. The value range is 24-bit when reading from non-antialiased frame buffer.
// When reading from an antialiased frame buffer, subsample 0 is read and returned.  The value will be compressed 16-bit form. To convert the compressed value to a plain 24-bit integer, you can use the function GXDecompressZ16.

void GXPeekZ(uint16_t x, uint16_t y, uint32_t* z)
{
    uint32_t addr = OSPhysicalToUncached (PI_MEMSP_EFB);
    addr |= (x << 2) | (y << 12) | 0x00400000;          // Z plane

    *z = *(uint32_t *)addr;
}

// This function allows the CPU to write a color value directly to the Embedded Frame Buffer (EFB) at position x,y.    The color's alpha value can be compared with the current alpha threshold, see GXPokeAlphaMode. 
// The color will be blended into the EFB using the blend mode set by GXPokeBlendMode.
// For an antialiased frame buffer, all 3 subsamples of a pixel are affected by the poke.   The color should be specified as u32 color in ARGB format.

void GXPokeARGB(uint16_t x, uint16_t y, uint32_t color)
{
    uint32_t addr = OSPhysicalToUncached (PI_MEMSP_EFB);
    addr |= (x << 2) | (y << 12) & ~0x00400000;             // Color plane

    *(uint32_t *)addr = color;
}

// This function allows the CPU to read a color value directly from the Embedded Frame Buffer (EFB) at position x,y.
// For an antialiased frame buffer, only subsample 0 of a pixel is read.  The returned color will be in u32 color in ARGB format.

void GXPeekARGB(uint16_t x, uint16_t y, uint32_t* color)
{
    uint32_t addr = OSPhysicalToUncached (PI_MEMSP_EFB);
    addr |= (x << 2) | (y << 12) & ~0x00400000;             // Color plane

    *color = *(uint32_t *)addr;
}
