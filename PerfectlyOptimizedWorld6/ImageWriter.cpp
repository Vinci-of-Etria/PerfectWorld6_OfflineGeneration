
#pragma warning( disable : 6385 )

#include "ImageWriter.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "MapEnums.h"


// --- BMP Layout -------------------------------------------------------------

static uint8 const formatHeader[] = {
    0,0,     // signature
    0,0,0,0, // image file size in bytes
    0,0,0,0, // reserved
    0,0,0,0, // start of pixel array
};

static uint8 const infoHeader[] = {
    0,0,0,0, // header size
    0,0,0,0, // image width
    0,0,0,0, // image height
    0,0,     // number of color planes
    0,0,     // bits per pixel
    0,0,0,0, // compression
    0,0,0,0, // image size
    0,0,0,0, // horizontal resolution
    0,0,0,0, // vertical resolution
    0,0,0,0, // colors in color table
    0,0,0,0, // important color count
};

static uint8 const pixel[] = {
    0, 0, 0 // blue, green, red
};

static uint32 const FORMAT_SIZE = sizeof formatHeader;
static uint32 const INFO_SIZE = sizeof infoHeader;
static uint32 const PIXEL_SIZE = sizeof pixel;

struct BMPHeader
{
    // Format
    uint8 sig[2];
    uint8 fileSize[4];
    uint8 __reserved0[4];
    uint8 dataOffset[4];
    // Info
    uint8 infoSize[4];
    uint8 width[4];
    uint8 height[4];
    uint8 numPlanes[2];
    uint8 bitsPerPixel[2];
    uint8 compression[4];
    uint8 imageSize[4];
    uint8 hRes[4];
    uint8 vRes[4];
    uint8 colorsInTable[4];
    uint8 importantColorCount[4];
};
STATIC_ASSERT(sizeof BMPHeader == FORMAT_SIZE + INFO_SIZE);

// "*(uint32*)" assumes little endian, change if having issues
static void setBitmapFormatHeader(BMPHeader* header, uint32 stride, uint32 height)
{
    memset(header, 0, sizeof *header);
    uint32 fileSize = FORMAT_SIZE + INFO_SIZE + (stride * height);

    header->sig[0] = 'B';
    header->sig[1] = 'M';
    *(uint32*)header->fileSize = fileSize;
    *(uint32*)header->dataOffset = FORMAT_SIZE + INFO_SIZE;
}

static void setBitmapInfoHeader(BMPHeader* header, uint32 width, uint32 height)
{
    *(uint32*)header->infoSize = INFO_SIZE;
    *(uint32*)header->width = width;
    *(uint32*)header->height = height;
    *(uint16*)header->numPlanes = 1;
    *(uint16*)header->bitsPerPixel = PIXEL_SIZE * 8;
}

static void writeBMPFile(char const* filename, BMPHeader* header, uint8* pixelBytes,
    uint32 width, uint32 byteWidth, uint32 byteLen, uint32 padding)
{
    FILE* bmp = fopen(filename, "wb");
    if (!bmp)
    {
        printf("Couldn't open bmp file with the name %s\n", filename);
        return;
    }

    fwrite((uint8*)header, 1, sizeof *header, bmp);

    uint8* line = pixelBytes;
    uint8* end = pixelBytes + byteLen;

    for (; line < end; line += byteWidth)
    {
        fwrite(line, PIXEL_SIZE, width, bmp);
        fwrite(pixel, 1, padding, bmp);
    }
    assert(line == pixelBytes + byteLen);

    fclose(bmp);
}


// --- Globals ----------------------------------------------------------------

// general properties
uint32 width;
uint32 height;
bool wrapX;
bool wrapY;
uint32 len;
// color buffer properties
uint8* buffer;
uint32 bgrByteWidth;
uint32 bgrByteLen;
// x6 of bgrByteLen for stuff operating on hex quad ranges
uint8* bufferExtended;
uint32 bgrByteLenExtended;
// hex properties
uint32 hexWidth;
uint32 hexHeight;
uint32 hexHalfWidth;
uint32 hexBodyHeight;
uint32 hexCapsHeight;
uint32 hexCapHeight;
uint32 hexBodyCapHeight;
uint32 const* hexCapBotOffsets;
uint32 const* hexCapTopOffsets;
// hex buffer properties
uint8* imgBuf;
uint32 pixWidth;
uint32 pixHeight;
uint32 pixLen;
uint32 byteHexWidth;
uint32 byteHexHalfWidth;
uint32 byteWidth;
uint32 byteLen;
uint32 padding;
// bmp properties
BMPHeader header;


// --- Writer -----------------------------------------------------------------

void InitImageWriter(uint32 _width, uint32 _height, bool _wrapX, bool _wrapY, uint32 const* hexDef)
{
    if (!_width || !_height)
    {
        printf("Map has no size: w: %u - h: %u\n", _width, _height);
        return;
    }
    if (!hexDef)
    {
        printf("No hex def!\n");
        return;
    }

    // general properties

    width = _width;
    height = _height;
    wrapX = _wrapX;
    wrapY = _wrapY;
    len = width * height;


    // color buffer properties

    bgrByteWidth = PIXEL_SIZE * width;
    bgrByteLen = PIXEL_SIZE * len;
    bgrByteLenExtended = bgrByteLen * 2;

    buffer = (uint8*)malloc(bgrByteLen);
    bufferExtended = (uint8*)malloc(bgrByteLenExtended);

    // hex properties

    hexWidth = hexDef[0];
    hexHeight = hexDef[1];
    hexBodyHeight = hexDef[2];
    hexCapBotOffsets = hexDef + 3;
    // The width and height need to be even so that they are tileable
    assert(hexWidth % 2 == 0 && hexHeight % 2 == 0);

    hexHalfWidth = hexWidth / 2;
    hexCapsHeight = hexHeight - hexBodyHeight;
    hexCapHeight = hexCapsHeight / 2;
    hexBodyCapHeight = hexBodyHeight + hexCapHeight;
    hexCapTopOffsets = hexCapBotOffsets + (hexCapHeight * 2);


    // hex buffer properties

    pixWidth = (width * hexWidth);
    // add for tile overlap from the back and forth of the hexes
    if (height > 1)
        pixWidth += hexHalfWidth;
    pixHeight = ((height / 2) * (hexHeight + hexBodyHeight)) + hexCapHeight;
    // add row if odd number of rows
    if (height % 2)
        pixHeight += hexBodyCapHeight;
    pixLen = pixWidth * pixHeight;

    byteHexWidth = PIXEL_SIZE * hexWidth;
    byteHexHalfWidth = PIXEL_SIZE * hexHalfWidth;
    byteWidth = PIXEL_SIZE * pixWidth;
    byteLen = PIXEL_SIZE * pixLen;

    imgBuf = (uint8*)malloc(byteLen);
    if (!buffer || !bufferExtended || !imgBuf)
    {
        printf("Ran out of memory initializing image writer!\n");
        assert(0);
        return;
    }
    memset(imgBuf, 0xFF, byteLen);


    // bmp header properties

    padding = 4 - (byteWidth % 4);
    uint32 stride = byteWidth + padding;

    setBitmapFormatHeader(&header, stride, pixHeight);
    setBitmapInfoHeader(&header, pixWidth, pixHeight);
}

void ExitImageWriter()
{
    free(imgBuf);
    free(bufferExtended);
    free(buffer);
}


void DrawHexes(void* data, uint32 dataTypeByteWidth, FilterToBGRFn FilterFn)
{
    if (!data || !FilterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", FilterFn ? "exists" : "DNE");
        return;
    }

    // pre-convert pixels
    uint8* it = buffer;
    uint8* end = it + bgrByteLen;
    uint8* src = (uint8*)data;

    for (; it < end; it += PIXEL_SIZE, src += dataTypeByteWidth)
        FilterFn(src, it);


    // prep image buffer
    //memset(imgBuf, 0xFF, byteLen);

    it = (uint8*)data;
    end = it + (len * dataTypeByteWidth);
    uint8* pix = imgBuf;


    // Write hexes

    uint32 offset = 0;
    if (height > 1)
        offset = hexHalfWidth * PIXEL_SIZE;


    // Write bottom

    // Cap
    uint32 const* hexIt = hexCapBotOffsets;
    uint8* bgrRow = buffer;
    uint8* bgrEnd = buffer + bgrByteWidth;

    for (uint32 i = 0; i < hexCapHeight; ++i, hexIt += 2)
    {
        uint32 pixOffset = hexIt[0] * PIXEL_SIZE;
        uint32 pixOffsetEnd = hexIt[1] * PIXEL_SIZE;
        uint32 pixTail = (hexWidth - (hexIt[0] + hexIt[1])) * PIXEL_SIZE;
        uint32 pixJump = pixTail + pixOffset;
        pix += pixOffset;

        for (uint8* bgr = bgrRow; bgr < bgrEnd; bgr += PIXEL_SIZE)
        {
            // get the write range
            uint8* pixEnd = pix + pixOffsetEnd;

            for (; pix < pixEnd; pix += PIXEL_SIZE)
            {
                pix[0] = bgr[0];
                pix[1] = bgr[1];
                pix[2] = bgr[2];
            }

            // jump to the next hex
            pix += pixJump;
        }

        // back off on the last jump then offset to next line
        pix += offset - pixOffset;
    }

    // Body
    uint32 pixHexWidth = hexWidth * PIXEL_SIZE;
    for (uint32 i = 0; i < hexBodyHeight; ++i)
    {
        for (uint8* bgr = bgrRow; bgr < bgrEnd; bgr += PIXEL_SIZE)
        {
            // get the write range
            uint8* pixEnd = pix + pixHexWidth;

            for (; pix < pixEnd; pix += PIXEL_SIZE)
            {
                pix[0] = bgr[0];
                pix[1] = bgr[1];
                pix[2] = bgr[2];
            }
        }

        // offset to next line
        pix += offset;
    }


    // Write center

    // Interlaced caps
    uint32 const* hexRef[] = {
        hexCapBotOffsets,
        hexCapTopOffsets
    };
    uint32 const* last = hexCapBotOffsets;
    uint8* bgrRef[] = {
        buffer + bgrByteWidth,
        buffer
    };
    uint32 alternator = 1;

    for (uint32 h = 1; h < height; ++h, alternator ^= 1)
    {
        hexIt = hexRef[alternator];
        uint32 const* hexItAlt = last;
        bgrEnd = bgrRef[alternator] + bgrByteWidth;

        for (uint32 i = 0; i < hexCapHeight; ++i, hexIt += 2, hexItAlt += 2)
        {
            uint32 pixOffset = hexIt[0] * PIXEL_SIZE;
            uint32 pixOffsetEnd = hexIt[1] * PIXEL_SIZE;
            pix += pixOffset;

            uint32 topOffsetEnd = hexItAlt[1] * PIXEL_SIZE;
            uint32 topTail = (hexWidth - (hexItAlt[0] + hexItAlt[1])) * PIXEL_SIZE;
            uint8* bgrN = bgrRef[alternator ^ 1];

            for (uint8* bgr = bgrRef[alternator]; bgr < bgrEnd; bgr += PIXEL_SIZE, bgrN += PIXEL_SIZE)
            {
                // get the write range
                uint8* pixEnd = pix + pixOffsetEnd;

                for (; pix < pixEnd; pix += PIXEL_SIZE)
                {
                    pix[0] = bgr[0];
                    pix[1] = bgr[1];
                    pix[2] = bgr[2];
                }

                pixEnd = pix + topOffsetEnd;

                for (; pix < pixEnd; pix += PIXEL_SIZE)
                {
                    pix[0] = bgrN[0];
                    pix[1] = bgrN[1];
                    pix[2] = bgrN[2];
                }
            }

            // offset to next line
            pix += topTail;
        }

        last = hexRef[alternator];

        // Body
        bgrRow += bgrByteWidth;
        bgrEnd = bgrRow + bgrByteWidth;
        if (alternator)
            pix += offset;

        for (uint32 i = 0; i < hexBodyHeight; ++i)
        {
            for (uint8* bgr = bgrRow; bgr < bgrEnd; bgr += PIXEL_SIZE)
            {
                // get the write range
                uint8* pixEnd = pix + pixHexWidth;

                for (; pix < pixEnd; pix += PIXEL_SIZE)
                {
                    pix[0] = bgr[0];
                    pix[1] = bgr[1];
                    pix[2] = bgr[2];
                }
            }

            // offset to next line
            pix += offset;
        }

        if (alternator)
            // back off on the last jump
            pix -= offset;

        bgrRef[0] += bgrByteWidth;
        bgrRef[1] += bgrByteWidth;
    }

    // Write top

    // Cap
    hexIt = hexCapTopOffsets;

    // offset the caps on even rows
    if (height % 2 == 0)
        pix += offset;

    for (uint32 i = 0; i < hexCapHeight; ++i, hexIt += 2)
    {
        uint32 pixOffset = hexIt[0] * PIXEL_SIZE;
        uint32 pixOffsetEnd = hexIt[1] * PIXEL_SIZE;
        uint32 pixTail = (hexWidth - (hexIt[0] + hexIt[1])) * PIXEL_SIZE;
        uint32 pixJump = pixTail + pixOffset;
        pix += pixOffset;

        for (uint8* bgr = bgrRow; bgr < bgrEnd; bgr += PIXEL_SIZE)
        {
            // get the write range
            uint8* pixEnd = pix + pixOffsetEnd;

            for (; pix < pixEnd; pix += PIXEL_SIZE)
            {
                pix[0] = bgr[0];
                pix[1] = bgr[1];
                pix[2] = bgr[2];
            }

            // jump to the next hex
            pix += pixJump;
        }

        // back off on the last jump then offset to next line
        pix += offset - pixOffset;
    }

    // verify iteration
    if (height % 2 == 0)
        pix -= offset;
    uint32 diff = pix - imgBuf;
    assert(diff == byteLen);
}

static void ApplyStamp(uint8* pos, int8 const* stamp, uint8 const rgb[3])
{
    int8 yOffset = stamp[0];
    int8 rows = stamp[1];
    int8 const* it = stamp + 2;

    uint8* row = pos + (byteWidth * yOffset);

    for (uint8 r = 0; r < rows; ++r, row += byteWidth)
    {
        int8 pixelNum = *it;
        ++it;
        int8 const* pixEnd = it + pixelNum;

        for (; it < pixEnd; ++it)
        {
            uint32 diff = it - stamp;
            uint32 offset = *it * PIXEL_SIZE;

            // dumb as hell fix because compiler things I'm accessing out of
            //   bounds when yOffset is negative
            int32 access = row - imgBuf;
            (imgBuf + access)[offset + 0] = rgb[2];
            (imgBuf + access)[offset + 1] = rgb[1];
            (imgBuf + access)[offset + 2] = rgb[0];
        }
    }
}

void AddStamps(void* data, uint32 dataTypeByteWidth, FilterStampsFn FilterFn)
{
    if (!data || !FilterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", FilterFn ? "exists" : "DNE");
        return;
    }

    uint8* it = (uint8*)data;
    uint8* rowRef = imgBuf;
    uint32 rowJump = byteWidth * hexBodyCapHeight;
    uint32 dblWidth = 2 * dataTypeByteWidth;

    for (uint32 y = 0; y < height; ++y)
    {
        uint8* pos = rowRef;
        if (y % 2)
            pos += byteHexHalfWidth;

        for (uint32 x = 0; x < width; ++x, it += dataTypeByteWidth, pos += byteHexWidth)
        {
            StampSet stamps = FilterFn(it);

            switch (stamps.elevation)
            {
            case esHills:
                ApplyStamp(pos, hillStamp, stampBlack);
                break;
            case esMountains:
                // TODO: make mnt/volcano icons cohesive
                if (stamps.feature != fVOLCANO)
                    ApplyStamp(pos, mtnStamp, stampBlack);
                break;
            default:
                break;
            }

            switch (stamps.feature)
            {
            case fFOREST:
                ApplyStamp(pos, forestStamp, stampBlack);
                break;
            case fJUNGLE:
                ApplyStamp(pos, jungleStamp, stampBlack);
                break;
            case fMARSH:
                ApplyStamp(pos, marshStamp, stampBlack);
                break;
            case fFLOODPLAINS:
            case fFLOODPLAINS_GRASSLAND:
            case fFLOODPLAINS_PLAINS:
                ApplyStamp(pos, floodplainStamp, stampBlack);
                break;
                // features that dominate the tile
            case fVOLCANO:
                ApplyStamp(pos, volcanoStamp, stampBlack);
                break;
            case fICE:
                ApplyStamp(pos, iceStamp, stampBlack);
                break;
            case fOASIS:
                ApplyStamp(pos, oasisStamp, stampBlack);
                break;
            case fREEF:
                ApplyStamp(pos, reefStamp, stampBlack);
                break;
                // pretty sure these two aren't on the map initially
            case fVOLCANIC_SOIL:
                ApplyStamp(pos, volcanicSoilStamp, stampBlack);
                break;
            case fGEOTHERMAL_FISSURE:
                ApplyStamp(pos, geothermalStamp, stampBlack);
                break;
            default:
                if (stamps.feature >= fWondersStart)
                    ApplyStamp(pos, naturalWonderStamp, stampBlack);
                break;
            }

            switch (stamps.resource)
            {
            case rBANANAS:
            case rCATTLE:
            case rCOPPER:
            case rCRABS:
            case rDEER:
            case rFISH:
            case rRICE:
            case rSHEEP:
            case rSTONE:
            case rWHEAT:
            case rMAIZE:
                ApplyStamp(pos, bonusStamp, stampBlack);
                break;
            case rCITRUS:
            case rCOCOA:
            case rCOFFEE:
            case rCOTTON:
            case rDYES:
            case rDIAMONDS:
            case rFURS:
            case rGYPSUM:
            case rINCENSE:
            case rIVORY:
            case rJADE:
            case rMARBLE:
            case rMERCURY:
            case rPEARLS:
            case rSALT:
            case rSILK:
            case rSILVER:
            case rSPICES:
            case rSUGAR:
            case rTEA:
            case rTOBACCO:
            case rTRUFFLES:
            case rWHALES:
            case rWINE:
            case rAMBER:
            case rOLIVES:
            case rTURTLES:
            case rHONEY:
            case rP0K_PAPYRUS:
            case rP0K_PENGUINS:
            case rP0K_PLUMS:
            case rCVS_POMEGRANATES:
            case rP0K_MAPLE:
            case rP0K_OPAL:
                ApplyStamp(pos, luxuryStamp, stampBlack);
                break;
            case rHORSES:
            case rIRON:
            case rNITER:
            case rCOAL:
            case rOIL:
            case rALUMINUM:
            case rURANIUM:
                ApplyStamp(pos, strategicStamp, stampBlack);
                break;
            default:
                break;
            }
        }

        rowRef += rowJump;
    }
}

void AddEdges(void* data, uint32 dataTypeByteWidth, FilterEdgeFn FilterFn, uint8 const color[3])
{
    if (!data || !FilterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", FilterFn ? "exists" : "DNE");
        return;
    }


    // pre-convert pixels
    uint8* it = buffer;
    uint8* end = it + len;
    uint8* src = (uint8*)data;

    for (; it < end; ++it, src += dataTypeByteWidth)
        *it = FilterFn(src);


    // Propagate rivers to adjacent tiles
    uint32 wShort = width - 1;

    // handle bottom row
    it = buffer;
    for (uint32 w = 0; w < wShort; ++w, ++it)
        if (*it & EDGE_E)
            *(it + 1) |= EDGE_W;
    // handle the end
    if (wrapX && *it & EDGE_E)
        *(buffer) |= EDGE_W;

    // handle y wrap if it exists
    if (wrapY)
    {
        it = buffer;
        uint8* altRow = buffer + len - width;
        uint8* last = buffer + len - 1;
        // handle first tile
        if (*it & EDGE_SW)
            *(last) |= EDGE_NE;
        if (*it & EDGE_SE)
            *(altRow) |= EDGE_NW;
        ++it;
        ++altRow;

        // handle the rest
        for (uint32 w = 1; w < width; ++w, ++it, ++altRow)
        {
            if (*it & EDGE_SW)
                *(altRow - 1) |= EDGE_NE;
            if (*it & EDGE_SE)
                *(altRow) |= EDGE_NW;
        }
    }

    // handle the intervening rows
    it = buffer + width;
    uint8* altRow = buffer;
    uint8* eWrap = buffer + width;
    uint8* sWrap = buffer;
    for (uint32 h = 1; h < height; ++h)
    {
        // handle odd row

        for (uint32 w = 0; w < wShort; ++w, ++it, ++altRow)
        {
            if (*it & EDGE_SW)
                *(altRow) |= EDGE_NE;
            if (*it & EDGE_SE)
                *(altRow + 1) |= EDGE_NW;
            if (*it & EDGE_E)
                *(it + 1) |= EDGE_W;
        }

        // handle end
        if (*it & EDGE_SW)
            *(altRow) |= EDGE_NE;
        if (*it & EDGE_SE)
            *(sWrap) |= EDGE_NW;
        if (*it & EDGE_E)
            *(eWrap) |= EDGE_W;

        ++it;
        ++altRow;
        eWrap += width;
        sWrap += width;

        ++h;
        // exit early if height is reached
        if (h >= height)
            break;


        // handle even row

        // handle start
        if (*it & EDGE_SW)
            *(sWrap) |= EDGE_NE;
        if (*it & EDGE_SE)
            *(altRow) |= EDGE_NW;
        if (*it & EDGE_E)
            *(it + 1) |= EDGE_W;
        ++it;
        ++altRow;

        for (uint32 w = 1; w < wShort; ++w, ++it, ++altRow)
        {
            if (*it & EDGE_SW)
                *(altRow - 1) |= EDGE_NE;
            if (*it & EDGE_SE)
                *(altRow) |= EDGE_NW;
            if (*it & EDGE_E)
                *(it + 1) |= EDGE_W;
        }

        // handle end
        if (*it & EDGE_SW)
            *(altRow - 1) |= EDGE_NE;
        if (*it & EDGE_SE)
            *(altRow) |= EDGE_NW;
        if (*it & EDGE_E)
            *(eWrap) |= EDGE_W;

        ++it;
        ++altRow;
        eWrap += width;
        sWrap += width;
    }


    // add stamps

    it = (uint8*)data;
    uint8* riverRef = buffer;
    uint8* rowRef = imgBuf;
    uint32 rowJump = byteWidth * hexBodyCapHeight;
    uint32 dblWidth = 2 * dataTypeByteWidth;

    for (uint32 y = 0; y < height; ++y)
    {
        uint8* pos = rowRef;
        if (y % 2)
            pos += byteHexHalfWidth;

        for (uint32 x = 0; x < width; ++x, it += dataTypeByteWidth, pos += byteHexWidth, ++riverRef)
        {
            if (*riverRef & EDGE_E)
                ApplyStamp(pos, eEdgeStamp, color);
            if (*riverRef & EDGE_SE)
                ApplyStamp(pos, seEdgeStamp, color);
            if (*riverRef & EDGE_SW)
                ApplyStamp(pos, swEdgeStamp, color);
            if (*riverRef & EDGE_W)
                ApplyStamp(pos, wEdgeStamp, color);
            if (*riverRef & EDGE_NW)
                ApplyStamp(pos, nwEdgeStamp, color);
            if (*riverRef & EDGE_NE)
                ApplyStamp(pos, neEdgeStamp, color);
        }

        rowRef += rowJump;
    }
}

void AddVerts(void* data, uint32 dataTypeByteWidth, FilterVertFn FilterFn)
{
    if (!data || !FilterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", FilterFn ? "exists" : "DNE");
        return;
    }


    // pre-convert pixels
    uint8* it = buffer;
    uint8* end = it + len;
    uint8* src = (uint8*)data;
    uint8* color = bufferExtended;
    uint32 set = sizeof uint8 * 3 * 2;

    for (; it < end; ++it, color += set, src += dataTypeByteWidth)
        *it = FilterFn(src, color);


    // add stamps

    uint8* hexRef = buffer;
    uint8* colorRef = bufferExtended;
    uint8* rowRef = imgBuf;
    uint32 rowJump = byteWidth * hexBodyCapHeight;
    uint32 dblWidth = 2 * dataTypeByteWidth;

    // Handle first row

    uint8* pos = rowRef;

    // handle non wrap
    for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef, colorRef += set)
    {
        // handle each corner
        if (*hexRef & VERT_N)
            ApplyStamp(pos, nVertStamp, colorRef + (3 * VERT_IND_N));
        if (*hexRef & VERT_S)
            ApplyStamp(pos, sBotVertStamp, colorRef + (3 * VERT_IND_S));
    }

    rowRef += rowJump;

    // Handle body

    uint32 sHeight = height - 1;

    for (uint32 y = 1; y < sHeight; ++y)
    {
        pos = rowRef;
        if (y % 2)
            pos += byteHexHalfWidth;
        uint32 diff = pos - imgBuf;

        for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef, colorRef += set)
        {
            // handle each corner
            if (*hexRef & VERT_N)
                ApplyStamp(pos, nVertStamp,  colorRef + (3 * VERT_IND_N));
            if (*hexRef & VERT_S)
                ApplyStamp(pos, sVertStamp,  colorRef + (3 * VERT_IND_S));
        }

        rowRef += rowJump;
    }

    // Handle last row
    pos = rowRef + byteHexHalfWidth;

    // handle non wrap
    for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef, colorRef += set)
    {
        // handle each corner
        if (*hexRef & VERT_N)
            ApplyStamp(pos, nTopVertStamp, colorRef + (3 * VERT_IND_N));
        if (*hexRef & VERT_S)
            ApplyStamp(pos, sVertStamp, colorRef + (3 * VERT_IND_S));
    }

    // Handle y wrap

    if (wrapY)
    {
        pos = rowRef + rowJump;
        hexRef -= width;
        colorRef -= width * set;

        uint8* basePos = imgBuf - rowJump + byteHexHalfWidth;
        uint8* baseHexRef = buffer;
        uint8* baseColorRef = bufferExtended;

        for (uint32 x = 0; x < width; ++x,
            pos += byteHexWidth, ++hexRef, colorRef += set,
            basePos += byteHexWidth, ++baseHexRef, baseColorRef += set)
        {
            if (*baseHexRef & VERT_S)
                ApplyStamp(pos, sVertStamp, baseColorRef + (3 * VERT_IND_S));
            if (*hexRef & VERT_N)
                ApplyStamp(basePos, nVertStamp, colorRef + (3 * VERT_IND_N));
        }

        // handle corner x wrap
        if (wrapX)
        {
            basePos = imgBuf;

            --hexRef;
            colorRef -= set;
            if (*hexRef & VERT_N)
                ApplyStamp(basePos, swLeftVertStamp, colorRef + (3 * VERT_IND_N));

            pos -= byteHexWidth + rowJump - byteHexHalfWidth;

            baseHexRef = buffer;
            baseColorRef = bufferExtended;
            if (*baseHexRef & VERT_S)
                ApplyStamp(pos, neRightVertStamp, baseColorRef + (3 * VERT_IND_S));
        }
    }

    // Handle x wrap
    if (wrapX)
    {
        uint32 dblWidth = width * 2;
        uint32 dblWidthSet = dblWidth * set;
        uint32 dblJump = rowJump * 2;

        pos = imgBuf;
        hexRef = buffer;
        colorRef = bufferExtended;

        uint8* posRight = imgBuf + rowJump + (byteHexWidth * (width - 1)) + byteHexHalfWidth;
        uint8* hexRefRight = buffer + dblWidth - 1;
        uint8* colorRefRight = bufferExtended + dblWidthSet - set;

        if (*hexRef & VERT_N)
            ApplyStamp(posRight, seRightVertStamp, colorRef + (3 * VERT_IND_N));

        hexRef += dblWidth;
        colorRef += dblWidthSet;

        for (uint32 h = 2; h < height; h += 2,
            hexRef += dblWidth, colorRef += dblWidthSet,
            hexRefRight += dblWidth, colorRefRight += dblWidthSet)
        {
            if (*hexRefRight & VERT_S)
                ApplyStamp(pos, nwLeftVertStamp, colorRefRight + (3 * VERT_IND_S));
            if (*hexRef & VERT_S)
                ApplyStamp(posRight, neRightVertStamp, colorRef + (3 * VERT_IND_S));
            pos += dblJump;
            posRight += dblJump;
            if (*hexRefRight & VERT_N)
                ApplyStamp(pos, swLeftVertStamp, colorRefRight + (3 * VERT_IND_N));
            if (*hexRef & VERT_N)
                ApplyStamp(posRight, seRightVertStamp, colorRef + (3 * VERT_IND_N));
        }

        if (*hexRefRight & VERT_S)
            ApplyStamp(pos, nwLeftVertStamp, colorRefRight + (3 * VERT_IND_S));
    }
}

void AddStampBits(void* data, uint32 dataTypeByteWidth, FilterBitsFn FilterFn, uint8 const color[3])
{
    if (!data || !FilterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", FilterFn ? "exists" : "DNE");
        return;
    }


    // pre-convert pixels
    uint8* it = buffer;
    uint8* end = it + len;
    uint8* src = (uint8*)data;

    for (; it < end; ++it, src += dataTypeByteWidth)
        *it = FilterFn(src);


    // add stamps

    uint8* hexRef = buffer;
    uint8* rowRef = imgBuf;
    uint32 rowJump = byteWidth * hexBodyCapHeight;
    uint32 dblWidth = 2 * dataTypeByteWidth;

    // Handle first row

    uint8* pos = rowRef;

    // handle non wrap
    for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef)
    {
        if (*hexRef & FLOW_SW_S)
            ApplyStamp(pos, swsFlowStamp, color);
        if (*hexRef & FLOW_SE_S)
            ApplyStamp(pos, sesFlowStamp, color);
        if (*hexRef & FLOW_NW_N)
            ApplyStamp(pos, nwnFlowStamp, color);
        if (*hexRef & FLOW_NE_N)
            ApplyStamp(pos, nenFlowStamp, color);
        if (*hexRef & FLOW_N_S)
            ApplyStamp(pos, nsFlowStamp, color);
    }

    rowRef += rowJump;

    // Handle body

    uint32 sHeight = height - 1;

    for (uint32 y = 1; y < sHeight; ++y)
    {
        pos = rowRef;
        if (y % 2)
            pos += byteHexHalfWidth;
        uint32 diff = pos - imgBuf;

        for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef)
        {
            if (*hexRef & FLOW_S_N)
                ApplyStamp(pos, snFlowStamp, color);
            if (*hexRef & FLOW_SW_S)
                ApplyStamp(pos, swsFlowStamp, color);
            if (*hexRef & FLOW_SE_S)
                ApplyStamp(pos, sesFlowStamp, color);
            if (*hexRef & FLOW_NW_N)
                ApplyStamp(pos, nwnFlowStamp, color);
            if (*hexRef & FLOW_NE_N)
                ApplyStamp(pos, nenFlowStamp, color);
            if (*hexRef & FLOW_N_S)
                ApplyStamp(pos, nsFlowStamp, color);
        }

        rowRef += rowJump;
    }

    // Handle last row
    pos = rowRef + byteHexHalfWidth;

    // handle non wrap
    for (uint32 x = 0; x < width; ++x, pos += byteHexWidth, ++hexRef)
    {
        if (*hexRef & FLOW_S_N)
            ApplyStamp(pos, snFlowStamp, color);
        if (*hexRef & FLOW_SW_S)
            ApplyStamp(pos, swsFlowStamp, color);
        if (*hexRef & FLOW_SE_S)
            ApplyStamp(pos, sesFlowStamp, color);
        if (*hexRef & FLOW_NW_N)
            ApplyStamp(pos, nwnFlowStamp, color);
        if (*hexRef & FLOW_NE_N)
            ApplyStamp(pos, nenFlowStamp, color);
    }

    // TODO: x and y wrap
}

void SaveMap(char const* filename)
{
    writeBMPFile(filename, &header, imgBuf, pixWidth, byteWidth, byteLen, padding);
}
