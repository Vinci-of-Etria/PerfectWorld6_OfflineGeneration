
#pragma warning( disable : 6385 )

#include "ImageWriter.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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

    fclose(bmp);
}


// --- Globals ----------------------------------------------------------------

// general properties
uint32 width;
uint32 height;
uint32 len;
// color buffer properties
uint8* mappedBGRBuf;
uint32 bgrByteWidth;
uint32 bgrByteLen;
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
uint32 byteWidth;
uint32 byteLen;
uint32 padding;
// bmp properties
BMPHeader header;


// --- Writer -----------------------------------------------------------------

void InitImageWriter(uint32 _width, uint32 _height, uint32 const* hexDef)
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
    len = width * height;


    // color buffer properties

    bgrByteWidth = PIXEL_SIZE * width;
    bgrByteLen = PIXEL_SIZE * len;

    mappedBGRBuf = (uint8*)malloc(bgrByteWidth);


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

    byteWidth = PIXEL_SIZE * pixWidth;
    byteLen = PIXEL_SIZE * pixLen;

    imgBuf = (uint8*)malloc(byteLen);
    if (!mappedBGRBuf || !imgBuf)
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
    free(mappedBGRBuf);
}

void DrawHexes(void* data, uint32 dataTypeByteWidth, FilterToBGRFn filterFn)
{
    if (!data || !filterFn)
    {
        printf("No data to write - data: %s - fn: %s\n",
            data ? "exists" : "DNE", filterFn ? "exists" : "DNE");
        return;
    }

    // pre-convert pixels
    uint8* it = mappedBGRBuf;
    uint8* end = it + bgrByteLen;
    uint8* src = (uint8*)data;

    for (; it < end; it += PIXEL_SIZE, src += dataTypeByteWidth)
        filterFn(src, it);


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
    uint8* bgrRow = mappedBGRBuf;
    uint8* bgrEnd = mappedBGRBuf + bgrByteWidth;

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
        mappedBGRBuf + bgrByteWidth,
        mappedBGRBuf
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

void SaveMap(char const* filename)
{
    writeBMPFile(filename, &header, imgBuf, pixWidth, byteWidth, byteLen, padding);
}
