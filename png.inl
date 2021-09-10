//
// Embedded-friendly PNG Encoder
//
// Copyright (c) 2000-2021 BitBank Software, Inc.
// Written by Larry Bank
// Project started 12/9/2000
//
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

// Macro to simplify writing a big-endian 32-bit value on any CPU
#define WRITEMOTO32(p, o, val) {uint32_t l = val; p[o] = (unsigned char)(l >> 24); p[o+1] = (unsigned char)(l >> 16); p[o+2] = (unsigned char)(l >> 8); p[o+3] = (unsigned char)l;}

unsigned char PNGFindFilter(uint8_t *pCurr, uint8_t *pPrev, int iPitch, int iStride);
void PNGFilter(uint8_t ucFilter, uint8_t *pOut, uint8_t *pCurr, uint8_t *pPrev, int iStride, int iPitch);

static const uint32_t crcTable[] = {
/* 0x00 */ 0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
/* 0x08 */ 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
/* 0x10 */ 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
/* 0x18 */ 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
/* 0x20 */ 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
/* 0x28 */ 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
/* 0x30 */ 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
/* 0x38 */ 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
/* 0x40 */ 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
/* 0x48 */ 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
/* 0x50 */ 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
/* 0x58 */ 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
/* 0x60 */ 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
/* 0x68 */ 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
/* 0x70 */ 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
/* 0x78 */ 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
/* 0x80 */ 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
/* 0x88 */ 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
/* 0x90 */ 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
/* 0x98 */ 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
/* 0xa0 */ 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
/* 0xa8 */ 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
/* 0xb0 */ 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
/* 0xb8 */ 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
/* 0xc0 */ 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
/* 0xc8 */ 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
/* 0xd0 */ 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
/* 0xd8 */ 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
/* 0xe0 */ 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
/* 0xe8 */ 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
/* 0xf0 */ 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
/* 0xf8 */ 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};
  
//
// Calculate the PNG-style CRC value for a block of data
//
uint32_t PNGCalcCRC(unsigned char *buf, int len, uint32_t u32_start)
{
/* Table of CRCs of all 8-bit messages. */
    uint32_t crc = u32_start; //0xffffffff;
   int n;

     for (n = 0; n < len; n++)
     {
         crc = crcTable[(crc ^ buf[n]) & 0xff] ^ (crc >> 8);
     }

  return crc ^ 0xffffffffL;

} /* PNGCalcCRC() */

uint32_t PNGCalcCRCNoFlip(unsigned char *buf, int len, uint32_t u32_start)
{
/* Table of CRCs of all 8-bit messages. */
    uint32_t crc = u32_start; //0xffffffff;
   int n;

     for (n = 0; n < len; n++)
     {
         crc = crcTable[(crc ^ buf[n]) & 0xff] ^ (crc >> 8);
     }

  return crc;

} 

static unsigned char PAETH(unsigned char a, unsigned char b, unsigned char c)
{
    int pa, pb, pc;
#ifdef SLOW_WAY
    int p;
#endif // SLOW_WAY
    
#ifndef SLOW_WAY
    pc = c;
    pa = b - pc;
    pb = a - pc;
    pc = pa + pb;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
#else
    p = a + b - c; // initial estimate
    pa = abs(p - a); // distances to a, b, c
    pb = abs(p - b);
    pc = abs(p - c);
#endif
    // return nearest of a,b,c,
    // breaking ties in order a,b,c.
    if (pa <= pb && pa <= pc)
        return a;
    else if (pb <= pc)
        return b;
    else return c;
    
} /* PAETH() */
//
// Write the PNG file header and, if needed, a color palette chunk
//
static int PNGStartFile(PNGIMAGE *pImage)
{
    int iError = PNG_SUCCESS;
    unsigned char *p;
    int iSize, i, iLen;
    uint32_t ulCRC;
        
    p = pImage->ucFileBuf;
    iSize = 0; // output data size
    WRITEMOTO32(p, iSize, 0x89504e47); // PNG File header
    iSize += 4;
    WRITEMOTO32(p, iSize, 0x0d0a1a0a);
    iSize += 4;
    // IHDR contains 13 data bytes
    WRITEMOTO32(p, iSize, 0x0000000d); // IHDR length
    iSize += 4;
    WRITEMOTO32(p, iSize, 0x49484452); // IHDR marker
    iSize += 4;
    WRITEMOTO32(p, iSize, pImage->iWidth); // Image Width
    iSize += 4;
    WRITEMOTO32(p, iSize, pImage->iHeight); // Image Height
    iSize += 4;
    p[iSize++] = (pImage->ucBpp > 8) ? 8:pImage->ucBpp; // Bit depth
    p[iSize++] = pImage->ucPixelType;
    p[iSize++] = 0; // compression method 0
    p[iSize++] = 0; // filter type 0
    p[iSize++] = 0; // interlace = no
    ulCRC = PNGCalcCRC(&p[iSize-17], 17, 0xffffffff); // store CRC for IHDR chunk
    WRITEMOTO32(p, iSize, ulCRC);
    iSize += 4;
    
    //sBIT chunk
    switch(pImage->ucPixelType)
    {
        default: break; //TODO: implement other pixelTypes
        case PNG_PIXEL_TRUECOLOR:
            WRITEMOTO32(p, iSize, 0x00000003); // sBIT length
            iSize += 4;
            WRITEMOTO32(p, iSize, 0x73424954); // sBIT marker
            iSize += 4;
            switch(pImage->ucSourceFormat)
            {
                case PSP_DISPLAY_PIXEL_FORMAT_565:
                    p[iSize++] = 5; p[iSize++] = 6; p[iSize++] = 5;
                    break;
                case PSP_DISPLAY_PIXEL_FORMAT_5551:
                    p[iSize++] = 5; p[iSize++] = 5; p[iSize++] = 5;
                    break;
                case PSP_DISPLAY_PIXEL_FORMAT_4444:
                    p[iSize++] = 4; p[iSize++] = 4; p[iSize++] = 4;
                    break;
                case PSP_DISPLAY_PIXEL_FORMAT_8888:
                    p[iSize++] = 5; p[iSize++] = 6; p[iSize++] = 5;
                    break;
            }
            ulCRC = PNGCalcCRC(&p[iSize-7], 7, 0xffffffff); // store CRC for sBIT chunk
            WRITEMOTO32(p, iSize, ulCRC);
            iSize += 4;
            break;
    }

    if (pImage->ucPixelType == PNG_PIXEL_INDEXED)
	   {
           // Write the palette
           iLen = (1 << pImage->ucBpp); // palette length
           WRITEMOTO32(p, iSize, iLen*3); // 3 bytes per entry
           iSize += 4;
           WRITEMOTO32(p, iSize, 0x504c5445/*'PLTE'*/);
           iSize += 4;
           for (i=0; i<iLen; i++)
           {
               p[iSize++] = pImage->ucPalette[i*3+2]; // red
               p[iSize++] = pImage->ucPalette[i*3+1]; // green
               p[iSize++] = pImage->ucPalette[i*3+0]; // blue
           }
           ulCRC = PNGCalcCRC(&p[iSize-(iLen*3)-4], 4+(iLen*3), 0xffffffff); // store CRC for PLTE chunk
           WRITEMOTO32(p, iSize, ulCRC);
           iSize += 4;
           if (pImage->iTransparent >= 0 || pImage->ucHasAlphaPalette) // add transparency chunk
           {
               if (pImage->ucPixelType == PNG_PIXEL_INDEXED) { // a set of palette alpha values
                    iLen = (1 << pImage->ucBpp); // palette length
               } else if (pImage->ucPixelType == PNG_PIXEL_GRAYSCALE) {
                   iLen = 2;
               } else {
                   iLen = 6; // truecolor single transparent color
               }
               WRITEMOTO32(p, iSize, iLen); // 1 byte per palette alpha entry
               iSize += 4;
               WRITEMOTO32(p, iSize, 0x74524e53 /*'tRNS'*/);
               iSize += 4;
               switch (iLen) {
                   case 2: // grayscale
                       p[iSize++] = 0; // 16-bit value (big endian)
                       p[iSize++] = (uint8_t)pImage->iTransparent;
                       break;
                   case 6: // truecolor
                       p[iSize++] = 0; // 16-bit value (big endian for color stimulus)
                       p[iSize++] = (uint8_t)pImage->iTransparent & 0xff;
                       p[iSize++] = 0;
                       p[iSize++] = (uint8_t)((pImage->iTransparent >> 8) & 0xff);
                       p[iSize++] = 0;
                       p[iSize++] = (uint8_t)((pImage->iTransparent >> 16) & 0xff);
                       p[iSize++] = 0;
                       break;
                   default: // palette colors
                       for (i = 0; i<iLen; i++) // write n alpha values to accompany the palette
                       {
                           p[iSize++] = pImage->ucPalette[768+i];
                       }
                       break;
               } // switch
               ulCRC = PNGCalcCRC(&p[iSize - iLen - 4], 4 + iLen, 0xffffffff); // store CRC for tRNS chunk
               WRITEMOTO32(p, iSize, ulCRC);
               iSize += 4;
           }
       }
    // IDAT
    WRITEMOTO32(p, iSize, 0/*iCompressedSize*/); // IDAT length
    iSize += 4;
    WRITEMOTO32(p, iSize, 0x49444154); // IDAT marker
    iSize += 4;
    pImage->iCompressedSize = 0;
    pImage->iHeaderSize = iSize; // keep the PNG header size for later
    if (pImage->pOutput) { // copy to ram?
        memcpy(pImage->pOutput, pImage->ucFileBuf, iSize);
    } else { // write it to the file
        (*pImage->pfnWrite)(&pImage->PNGFile, pImage->ucFileBuf, iSize);
    }
    return iError;
    
} /* PNGStartFile() */
//
// Finish PNG file data (updates IDAT chunk size+crc & writes END chunk)
//
int PNGEndFile(PNGIMAGE *pImage)
{
    int iSize=0;
    uint8_t *p;
    uint32_t ulCRC;
    
    if (pImage->pOutput) { // output buffer = easy to wrap up
        p = pImage->pOutput;
        iSize = pImage->iHeaderSize;
        WRITEMOTO32(p, iSize-8, pImage->iCompressedSize); // write IDAT chunk size
        iSize += pImage->iCompressedSize;
        ulCRC = PNGCalcCRC(&p[iSize-pImage->iCompressedSize-4], pImage->iCompressedSize+4, 0xffffffff); // store CRC for IDAT chunk
        WRITEMOTO32(p, iSize, ulCRC);
        iSize += 4;
        // Write the IEND chunk
        WRITEMOTO32(p, iSize, 0);
        iSize += 4;
        WRITEMOTO32(p, iSize, 0x49454e44/*'IEND'*/);
        iSize += 4;
        WRITEMOTO32(p, iSize, 0xae426082); // same CRC every time
        iSize += 4;
    } else { // file mode = not so easy
        uint32_t pu32[4];
        uint8_t *p;
        int i, iReadSize;
        
        p = (uint8_t *)&pu32[0];
        iSize = pImage->iHeaderSize;
        ulCRC = 0xffffffff;
        (*pImage->pfnSeek)(&pImage->PNGFile, iSize-8); // seek to compressed size
        WRITEMOTO32(p, 0, pImage->iCompressedSize); // save the actual IDAT size
        (*pImage->pfnWrite)(&pImage->PNGFile, (uint8_t *)pu32, 4);
        // From this point forward, we need to calculate the CRC of the IDAT chunk
        // and unfortunately that means reading back all of the compressed data
        (*pImage->pfnSeek)(&pImage->PNGFile, iSize-4);
        i = pImage->iCompressedSize+4; // IDAT marker + data length
        while (i) {
            iReadSize = i;
            if (iReadSize > PNG_FILE_BUF_SIZE)
                iReadSize = PNG_FILE_BUF_SIZE;
            (*pImage->pfnRead)(&pImage->PNGFile, pImage->ucFileBuf, iReadSize);
            ulCRC = PNGCalcCRCNoFlip(pImage->ucFileBuf, iReadSize, ulCRC);
            i -= iReadSize;
        }
        ulCRC = ~ulCRC;
        //(*pImage->pfnSeek)(&pImage->PNGFile, iSize + pImage->iCompressedSize);
        WRITEMOTO32(p, 0, ulCRC); // now write the CRC
        iSize += pImage->iCompressedSize + 4;
        // Write the IEND chunk
        WRITEMOTO32(p, 4, 0);
        iSize += 4;
        WRITEMOTO32(p, 8, 0x49454e44/*'IEND'*/);
        iSize += 4;
        WRITEMOTO32(p, 12, 0xae426082); // same CRC every time
        iSize += 4;
        // write the final data to the file
        (*pImage->pfnWrite)(&pImage->PNGFile, (uint8_t *)p, 16);
    }
    return iSize;
} /* PNGEndFile() */
//
// My internal alloc/free functions to work on simple embedded systems
//
voidpf ZLIB_INTERNAL myalloc (voidpf opaque, unsigned int items, unsigned int size)
{
    PNGIMAGE *pImage = (PNGIMAGE *)opaque;
    // allocate from our internal pool
    int iSize = items * size;
    void *p = &pImage->ucMemPool[pImage->iMemPool];
    pImage->iMemPool += iSize;
    return p;
} /* myalloc() */

void ZLIB_INTERNAL myfree (voidpf opaque, voidpf ptr)
{
    (void)opaque;
    (void)ptr; // doesn't do anything since the memory is from an internal pool
} /* myfree() */
//
// Compress one line of image at a time and write the compressed data
// incrementally to the output file. This allows the system to not need an
// input nor output buffer larger than 2 lines of image data
//
static int PNGAddLine(PNGIMAGE *pImage, uint8_t *pSrc, int y)
{
    unsigned char ucFilter; // filter type
    unsigned char *pOut;
    int iStride;
    int err;
    int iPitch;
    
    iStride = pImage->ucBpp >> 3; // bytes per pixel
    iPitch = (pImage->iWidth * pImage->ucBpp) >> 3;
    if (iStride < 1)
        iStride = 1; // 1,4 bpp
    pOut = pImage->ucCurrLine;
    ucFilter = PNGFindFilter(pSrc, (y == 0) ? NULL : pImage->ucPrevLine, iPitch, iStride); // find best filter
    *pOut++ = ucFilter; // store filter byte
    PNGFilter(ucFilter, pOut, pSrc, pImage->ucPrevLine, iStride, iPitch); // filter the current line of image data and store
    memcpy(pImage->ucPrevLine, pSrc, iPitch);
    // Compress the filtered image data
    if (y == 0) // first block, initialize zlib
    {
        PNGStartFile(pImage);
        memset(&pImage->c_stream, 0, sizeof(z_stream));
        pImage->c_stream.zalloc = myalloc; // use internal alloc/free
        pImage->c_stream.zfree = myfree; // to use our memory pool
        pImage->c_stream.opaque = (voidpf)pImage;
        pImage->iMemPool = 0;
        // ZLIB compression levels: 1 = fastest, 9 = most compressed (slowest)
//        err = deflateInit(&pImage->c_stream, pImage->ucCompLevel); // might as well use max compression
        err = deflateInit2_(&pImage->c_stream, pImage->ucCompLevel, Z_DEFLATED, MAX_WBITS-MEM_SHRINK, DEF_MEM_LEVEL-MEM_SHRINK, Z_DEFAULT_STRATEGY, ZLIB_VERSION, (int)sizeof(z_stream)); // might as well use max compression
        pImage->c_stream.total_out = 0;
        pImage->c_stream.next_out = pImage->ucFileBuf;
        pImage->c_stream.avail_out = PNG_FILE_BUF_SIZE;
    }
    pImage->c_stream.next_in  = (Bytef*)pImage->ucCurrLine;
    pImage->c_stream.total_in = 0;
    pImage->c_stream.avail_in = iPitch+1; // compress entire buffer in 1 shot
    err = deflate(&pImage->c_stream, Z_SYNC_FLUSH);
    if (err != Z_OK) { // something went wrong with the data compression, stop
        pImage->iError = PNG_ENCODE_ERROR;
        return PNG_ENCODE_ERROR;
    }
    if (y == pImage->iHeight - 1) // last line, clean up
    {
        err = deflate(&pImage->c_stream, Z_FINISH);
        err = deflateEnd(&pImage->c_stream);
    }
    // Write the data to memory or a file
    //
    // A bunch of extra logic has been added below to minimize the total number
    // of calls to 'write'. Each compressed scanline might generate only a few
    // bytes of flate output and calling write() for a few bytes at a time can
    // slow things to a crawl.
    if (pImage->c_stream.total_out >= PNG_FILE_HIGHWATER) {
        if (pImage->pOutput) { // memory
            if ((pImage->iHeaderSize + pImage->iCompressedSize + pImage->c_stream.total_out) > pImage->iBufferSize) {
                // output buffer not large enough
                pImage->iError = PNG_MEM_ERROR;
                return PNG_MEM_ERROR;
            }
            memcpy(&pImage->pOutput[pImage->iHeaderSize + pImage->iCompressedSize], pImage->ucFileBuf, pImage->c_stream.total_out);
        } else { // file
            (*pImage->pfnWrite)(&pImage->PNGFile, pImage->ucFileBuf, (int)pImage->c_stream.total_out);
        }
        pImage->iCompressedSize += (int)pImage->c_stream.total_out;
        // reset zlib output buffer to start
        pImage->c_stream.total_out = 0;
        pImage->c_stream.next_out = pImage->ucFileBuf;
        pImage->c_stream.avail_out = PNG_FILE_BUF_SIZE;
    } // highwater hit
    if (y == pImage->iHeight -1) { // last line, finish file
        // if any remaining data in output buffer, write it
        if (pImage->c_stream.total_out > 0) {
            if (pImage->pOutput) { // memory
                if ((pImage->iHeaderSize + pImage->iCompressedSize + pImage->c_stream.total_out) > pImage->iBufferSize) {
                    // output buffer not large enough
                    pImage->iError = PNG_MEM_ERROR;
                    return PNG_MEM_ERROR;
                }
                memcpy(&pImage->pOutput[pImage->iHeaderSize + pImage->iCompressedSize], pImage->ucFileBuf, pImage->c_stream.total_out);
            } else { // file
                (*pImage->pfnWrite)(&pImage->PNGFile, pImage->ucFileBuf, (int)pImage->c_stream.total_out);
            }
            pImage->iCompressedSize += (int)pImage->c_stream.total_out;
        }
        pImage->iDataSize = PNGEndFile(pImage);
    }    
    return PNG_SUCCESS; // DEBUG
    
} /* PNGAddLine() */
//
// Find the best filter method for the given scanline
// Try each filter algorithm in turn and use SAD (sum of absolute differences)
// to choose the one with the lowest sum (a reasonable proxy for entropy)
//
unsigned char PNGFindFilter(uint8_t *pCurr, uint8_t *pPrev, int iPitch, int iStride)
{
int i;
unsigned char a, b, c, ucDiff, ucFilter;
uint32_t ulSum[5]  = {0,0,0,0,0}; // individual sums for the 4 types of filters
uint32_t ulMin;

    ucFilter = 0;
    for (i=0; i<iPitch; i++)
    {
       ucDiff = pCurr[i]; // no filter
       ulSum[0] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       // Sub
       if (i >= iStride)
       {
          ucDiff = pCurr[i]-pCurr[i-iStride];
          ulSum[1] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       }
       else
       {
           ucDiff = pCurr[i];
           ulSum[1] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       }
       // Up
       if (pPrev)
       {
          ucDiff = pCurr[i]-pPrev[i];
          ulSum[2] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       }
       else // not available
       {
           ucDiff = pCurr[i];
           ulSum[2] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       }
       // Average
       if (!pPrev || i < iStride)
       {
          if (!pPrev)
          {
             if (i < iStride)
                a = 0;
             else
                a = pCurr[i-iStride]>>1;
          }
          else
             a = pPrev[i]>>1;
       }
       else
       {
          a = (pCurr[i-iStride] + pPrev[i])>>1;
       }
       ucDiff = pCurr[i] - a;
       ulSum[3] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       // Paeth
       if (i < iStride)
          a = 0;
       else
          a = pCurr[i-iStride]; // left
       if (pPrev == NULL)
          b = 0; // above doesn't exist
       else
          b = pPrev[i];
       if (!pPrev || i < iStride)
          c = 0;
       else
          c = pPrev[i-iStride]; // above left
       ucDiff = pCurr[i] - PAETH(a,b,c);
       ulSum[4] += (ucDiff < 128) ? ucDiff: 256 - ucDiff;
       } // for i
       // Pick the best filter (or NONE if they're all bad)
       ulMin = iPitch * 255; // max value
       for (a=0; a<5; a++)
       {
          if (ulSum[a] < ulMin)
          {
             ulMin = ulSum[a];
             ucFilter = a; // current winner
          }
       } // for
       return ucFilter;

} /* PNGFindFilter() */
//
// Apply the given filter algorithm to a line of image data
//
void PNGFilter(uint8_t ucFilter, uint8_t *pOut, uint8_t *pCurr, uint8_t *pPrev, int iStride, int iPitch)
{
int j;

   switch (ucFilter)
      {
      case 0: // no filter, just copy
         memcpy(pOut, pCurr, iPitch);
         break;
      case 1: // sub
         j = 0;
         while (j < iStride)
         {
             pOut[j] = pCurr[j];
             j++;
         }
         while (j < (int)iPitch)
         {
            pOut[j] = pCurr[j]-pCurr[j-iStride];
            j++;
         }
         break;
      case 2: // up
         if (pPrev)
         {
            for (j=0;j<iPitch;j++)
            {
               pOut[j] = pCurr[j]-pPrev[j];
            }
         }
         else
            memcpy(pOut, pCurr, iPitch);
         break;
      case 3: // average
         for (j=0; j<iPitch; j++)
        {
            int a;
            if (!pPrev || j < iStride)
            {
               if (!pPrev)
               {
                  if (j < iStride)
                     a = 0;
                  else
                     a = pCurr[j-iStride]>>1;
               }
               else
                  a = pPrev[j]>>1;
            }
            else
            {
               a = (pCurr[j-iStride] + pPrev[j])>>1;
            }
            pOut[j] = (uint8_t)(pCurr[j] - a);
         }
         break;
      case 4: // paeth
         for (j=0; j<iPitch; j++)
         {
            uint8_t a,b,c;
            if (j < iStride)
               a = 0;
            else
               a = pCurr[j-iStride]; // left
            if (!pPrev)
               b = 0; // above doesn't exist
            else
               b = pPrev[j]; // above
            if (!pPrev || j < iStride)
               c = 0;
            else
               c = pPrev[j-iStride]; // above left
            pOut[j] = pCurr[j] - PAETH(a,b,c);
         }
         break;
      } // switch
} /* PNGFilter() */
