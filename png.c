/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * bitmap.c - PSPLINK kernel module bitmap code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/trunk/psplinkusb/psplink/bitmap.c $
 * $Id: bitmap.c 2026 2006-10-14 13:09:48Z tyranid $
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "PNGenc.h"

#include "png.inl"

/*
static int fixed_write(int fd, void *data, int len)
{
	int writelen = 0;

	while(writelen < len)
	{
		int ret;

		ret = sceIoWrite(fd, data + writelen, len - writelen);
		if(ret <= 0)
		{
			writelen = -1;
			break;
		}
		writelen += ret;
	}

	return writelen;
}

int write_8888_data(void *frame, void *pixel_data)
{
	uint8_t *line;
	uint8_t *p;
	int i;
	int h;

	line = pixel_data;
	for(h = 271; h >= 0; h--)
	{
		p = frame + (h*512*4);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = p[i*4];
			line[(i*3) + 1] = p[(i*4) + 1];
			line[(i*3) + 0] = p[(i*4) + 2];
		}
		line += 480 * 3;
	}

	return 0;
}

int write_5551_data(void *frame, void *pixel_data)
{
	uint8_t *line;
	uint16_t *p;
	int i;
	int h;

	line = pixel_data;
	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0x1F) << 3;
			line[(i*3) + 1] = ((p[i] >> 5) & 0x1F) << 3;
			line[(i*3) + 0] = ((p[i] >> 10) & 0x1F) << 3;
		}
		line += 480*3;
	}

	return 0;
}

int write_565_data(void *frame, void *pixel_data)
{
	uint8_t *line;
	uint16_t *p;
	int i;
	int h;

	line = pixel_data;
	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0x1F) << 3;
			line[(i*3) + 1] = ((p[i] >> 5) & 0x3F) << 2;
			line[(i*3) + 0] = ((p[i] >> 11) & 0x1F) << 3;
		}
		line += 480*3;
	}

	return 0;
}

int write_4444_data(void *frame, void *pixel_data)
{
	uint8_t *line;
	uint16_t *p;
	int i;
	int h;

	line = pixel_data;
	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0xF) << 4;
			line[(i*3) + 1] = ((p[i] >> 4) & 0xF) << 4;
			line[(i*3) + 0] = ((p[i] >> 8) & 0xF) << 4;
		}
		line += 480*3;
	}

	return 0;
}
*/

int32_t pngRGBA8ToRGB565(PNGIMAGE *png, uint8_t *pBuf, int frame_width)
{
	int x, y;
	for(y = 0; y < 272; y++)
	{
		for(x = 0; x < 480; x++)
		{
			uint16_t pixel = 0;
			pixel |= (pBuf[(x * 4) + 0] >> 3) & 0x1F;
			pixel <<= 6;
			pixel |= (pBuf[(x * 4) + 1] >> 2) & 0x3F;
			pixel <<= 5;
			pixel |= (pBuf[(x * 4) + 2] >> 3) & 0x1F;
			
			png->usLowColorBuffer[y * 480 + x] = pixel;
		}
		pBuf += frame_width * 4;
	}
	return 0;
}

int32_t pngFileRead(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
	int fd = pFile->fHandle;
	int readlen = 0;

	while(readlen < iLen)
	{
		int ret;

		ret = sceIoRead(fd, pBuf + readlen, iLen - readlen);
		if(ret <= 0)
		{
			readlen = -1;
			break;
		}
		readlen += ret;
	}
	return readlen;
}

int32_t pngFileWrite(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
	int fd = pFile->fHandle;
	int writelen = 0;

	while(writelen < iLen)
	{
		int ret;

		ret = sceIoWrite(fd, pBuf + writelen, iLen - writelen);
		if(ret <= 0)
		{
			writelen = -1;
			break;
		}
		writelen += ret;
	}

	return writelen;
}

int32_t pngFileSeek(PNGFILE *pFile, int32_t iPosition)
{
	int fd = pFile->fHandle;
	return sceIoLseek(fd, iPosition, SEEK_SET);
}

int pngWrite(void *frame_addr, void *png_image, int format, const char *file, int frame_width)
{
	PNGIMAGE *png = png_image;
	int fd;
	uint32_t y;
    uint16_t * pPixels;
	
    memset(png, 0, sizeof(PNGIMAGE));
    png->iTransparent = -1;
    png->pfnRead = pngFileRead;
    png->pfnWrite = pngFileWrite;
    png->pfnSeek = pngFileSeek;
	fd = sceIoOpen(file, PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	png->PNGFile.fHandle = fd;
	png->iWidth = 480;
	png->iHeight = 272;
	png->ucPixelType = PNG_PIXEL_TRUECOLOR;
	png->ucBpp = 24;
	png->ucCompLevel = 3;
	png->ucSourceFormat = format;
	
	if(fd < 0) {
		return -1;
	}

	switch(format)
	{
		case PSP_DISPLAY_PIXEL_FORMAT_565: 
			for(y = 0; y < 272; y++)
				memcpy(png->usLowColorBuffer + y * 480, frame_addr + y * frame_width * 2, 480 * 2);
			pPixels = png->usLowColorBuffer;
			for(y = 0; y < png->iHeight; y++)
			{
				uint32_t x;
				for(x = 0; x < 480; x++)
				{
					uint16_t pixel = *pPixels++;
					png->ucLineBuff[(x*3) + 0] = (((pixel & 0xF800) >> 11) * 255 + 15) / 31;
					png->ucLineBuff[(x*3) + 1] = (((pixel & 0x07E0) >> 5) * 255 + 31) / 63;
					png->ucLineBuff[(x*3) + 2] = ((pixel & 0x001F) * 255 + 15) / 31;
				}
				PNGAddLine(png, png->ucLineBuff, y);
			}
			break;
		case PSP_DISPLAY_PIXEL_FORMAT_5551: 
			for(y = 0; y < 272; y++)
				memcpy(png->usLowColorBuffer + y * 480, frame_addr + y * frame_width * 2, 480 * 2);
			pPixels = png->usLowColorBuffer;
			for(y = 0; y < png->iHeight; y++)
			{
				uint32_t x;
				for(x = 0; x < 480; x++)
				{
					uint16_t pixel = *pPixels++;
					png->ucLineBuff[(x*3) + 2] = (((pixel & 0x7C00) >> 10) * 255 + 15) / 31;
					png->ucLineBuff[(x*3) + 1] = (((pixel & 0x03E0) >> 5) * 255 + 15) / 31;
					png->ucLineBuff[(x*3) + 0] = ((pixel & 0x001F) * 255 + 15) / 31;
				}
				PNGAddLine(png, png->ucLineBuff, y);
			}
            break;
		case PSP_DISPLAY_PIXEL_FORMAT_4444:
			for(y = 0; y < 272; y++)
				memcpy(png->usLowColorBuffer + y * 480, frame_addr + y * frame_width * 2, 480 * 2);
			pPixels = png->usLowColorBuffer;
			for(y = 0; y < png->iHeight; y++)
			{
				uint32_t x;
				for(x = 0; x < 480; x++)
				{
					uint16_t pixel = *pPixels++;
					png->ucLineBuff[(x*3) + 0] = (((pixel & 0xF00) >> 8) * 255 + 7) / 15;
					png->ucLineBuff[(x*3) + 1] = (((pixel & 0x0F0) >> 4) * 255 + 7) / 15;
					png->ucLineBuff[(x*3) + 2] = ((pixel & 0x00F) * 255 + 7) / 15;
				}
				PNGAddLine(png, png->ucLineBuff, y);
			}
            break;
		case PSP_DISPLAY_PIXEL_FORMAT_8888: 
			pngRGBA8ToRGB565(png, frame_addr, frame_width);
			pPixels = png->usLowColorBuffer;
			for(y = 0; y < png->iHeight; y++)
			{
				uint32_t x;
				for(x = 0; x < 480; x++)
				{
					uint16_t pixel = *pPixels++;
					png->ucLineBuff[(x*3) + 0] = (((pixel & 0xF800) >> 11) * 255 + 15) / 31;
					png->ucLineBuff[(x*3) + 1] = (((pixel & 0x07E0) >> 5) * 255 + 31) / 63;
					png->ucLineBuff[(x*3) + 2] = ((pixel & 0x001F) * 255 + 15) / 31;
				}
				PNGAddLine(png, png->ucLineBuff, y);
			}
			break;
	};
	
	//fixed_write(fd, tmp_buf, bmp->filesize);
	sceIoClose(fd);
	return 0;
}
