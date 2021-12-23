
#pragma warning( disable : 6385 )

#include "ImageWriter.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// BMP setup

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

static void writeBMPFile(char const* filename, uint32 width, uint32 height, uint8* pixelBytes)
{
    uint32 byteWidth = width * PIXEL_SIZE;
    uint32 byteLen = byteWidth * height;

    uint32 padding = 4 - (byteWidth % 4);
    uint32 stride = byteWidth + padding;

    FILE* bmp = fopen(filename, "wb");
    if (!bmp)
    {
        printf("Couldn't open bmp file with the name %s\n", filename);
        return;
    }

    BMPHeader header;
    setBitmapFormatHeader(&header, stride, height);
    setBitmapInfoHeader(&header, width, height);
    fwrite((uint8*)&header, 1, sizeof header, bmp);

    uint8* line = pixelBytes;
    uint8* end = pixelBytes + byteLen;

    for (; line < end; line += byteWidth)
    {
        fwrite(line, PIXEL_SIZE, width, bmp);
        fwrite(pixel, 1, padding, bmp);
    }

    fclose(bmp);
}

void WriteHexMapToFile(char const* filename, uint32 const* hexDef, uint32 width, uint32 height,
    void* data, uint32 dataTypeByteWidth, FilterToBGRFn filterFn)
{
    if (!hexDef || !width || !height)
    {
        printf("No hexmap to write - def: %s - w: %u - h: %u\n", 
            hexDef ? "exists" : "DNE", width, height);
        return;
    }

    uint32 len = width * height;

    uint32 hexWidth = hexDef[0];
    uint32 hexHeight = hexDef[1];
    uint32 hexBodyHeight = hexDef[2];
    uint32 const* hexOffsets = hexDef + 3;
    // The width and height need to be even so that they are tileable
    assert(hexWidth % 2 == 0 && hexHeight % 2 == 0);

    uint32 halfHexWidth = hexWidth / 2;
    uint32 hexPairHeight = hexHeight + hexBodyHeight;
    uint32 hexCapsHeight = hexHeight - hexBodyHeight;
    uint32 hexCapHeight = hexCapsHeight / 2;
    uint32 hexBodyCapHeight = hexBodyHeight + hexCapHeight;
    uint32 const* hexOffsetsTop = hexOffsets + (hexCapHeight * 2);

    uint32 pixWidth = (width * hexWidth);
    // add for tile overlap from the back and forth of the hexes
    if (height > 1)
        pixWidth += halfHexWidth;
    uint32 byteWidth = pixWidth * PIXEL_SIZE;

    uint32 pixHeight = ((height / 2) * (hexHeight + hexBodyHeight)) + hexCapHeight;
    // add row if odd number of rows
    if (height % 2)
        pixHeight += hexBodyCapHeight;

    uint32 byteLen = byteWidth * pixHeight;

    // pre convert pixels
    uint32 pixLen = len * PIXEL_SIZE;
    uint8* mappedBGR = (uint8*)malloc(pixLen);

    uint8* it = mappedBGR;
    uint8* end = it + pixLen;
    uint8* src = (uint8*)data;

    for (; it < end; it += PIXEL_SIZE, src += dataTypeByteWidth)
        filterFn(src, it);


    // create image buffer
    uint8* image = (uint8*)malloc(byteLen * sizeof *image);
    memset(image, 0xFF, byteLen * sizeof * image);

    uint8* pix = image;
    it = (uint8*)data;
    end = it + (len * dataTypeByteWidth);


    // Write hexes

    uint32 offset = 0;
    if (height > 1)
        offset = halfHexWidth * PIXEL_SIZE;


    // Write bottom

    // Cap
    uint32 const* hexIt = hexOffsets;
    uint32 bgrWidth = width * PIXEL_SIZE;
    uint8* bgrRow = mappedBGR;
    uint8* bgrEnd = mappedBGR + bgrWidth;

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
        hexOffsets,
        hexOffsetsTop
    };
    uint32 const* last = hexOffsets;
    uint8 * bgrRef[] = {
        mappedBGR + bgrWidth,
        mappedBGR
    };
    uint32 alternator = 1;

    for (uint32 h = 1; h < height; ++h, alternator ^= 1)
    {
        hexIt = hexRef[alternator];
        uint32 const* hexItAlt = last;
        bgrEnd = bgrRef[alternator] + bgrWidth;

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
        bgrRow += bgrWidth;
        bgrEnd = bgrRow + bgrWidth;
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

        bgrRef[0] += bgrWidth;
        bgrRef[1] += bgrWidth;
    }

    // Write top

    // Cap
    hexIt = hexOffsets + (hexCapHeight * 2);

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

    writeBMPFile(filename, pixWidth, pixHeight, image);

    free(image);
    free(mappedBGR);
}
