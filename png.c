#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "debug.h"

typedef struct pixel
{
	unsigned char r, g, b;
} pixel;

typedef struct png_File
{
	// make sure these are first
	int width, height;
	unsigned char bit_depth, color_type, compression, filter, interlace;
	
	FILE *file;
	pixel *palette;
	
	unsigned char *image;
} png_File;

png_File pfile;

typedef struct png_Chunk
{
	unsigned int length;
	union
	{
		unsigned char type[4];
		unsigned int type_as_int;
	};
	unsigned char *data;
	unsigned int checksum;
} png_Chunk;

typedef struct png_Type_Callback
{
	union
	{
		unsigned char type[4];
		unsigned int type_as_int;
	};
	void (*callback)(png_Chunk *chunk);
	int count;
} png_Type_Callback;

unsigned int swap32(unsigned int val) 
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val >> 8) & 0xFF00) | ((val >> 24) & 0xFF);
}

#define PNG_IHDR_PALETTE_USED	1
#define PNG_IHDR_COLOR_USED		2
#define PNG_IHDR_APLHA_USED		4
#define PNG_IHDR_SIZE			13

void process_IHDR(png_Chunk *chunk)
{
	// Ensure the chunk is 13 bytes
	debug_if(chunk->length != PNG_IHDR_SIZE, ERROR, "IHDR size != %d\n", PNG_IHDR_SIZE);
	
	// Copy the data
	memcpy(&pfile, chunk->data, PNG_IHDR_SIZE);
	
	// Swap the endianess
	pfile.width = swap32(pfile.width);
	pfile.height = swap32(pfile.height);
	
	// Print out statistics
	debug(INFO, "Image stats:\n");
	debug(INFO, "\t%dpx x %dpx\n", pfile.width, pfile.height);
	debug(INFO, "\tBit Depth = %d bits per sample\n", pfile.bit_depth);
	debug(INFO, "\tColor type:\n");
	if(pfile.color_type & PNG_IHDR_PALETTE_USED) 	debug(INFO, "\t\tPalette\n");
	if(pfile.color_type & PNG_IHDR_COLOR_USED)	 	debug(INFO, "\t\tColor\n");
	if(pfile.color_type & PNG_IHDR_APLHA_USED)	 	debug(INFO, "\t\tAlpha\n");
	if(pfile.compression == 0) 						debug(INFO, "\tCompression: method 0\n");
	if(pfile.filter == 0) 							debug(INFO, "\tFilter: method 0\n");
	debug(INFO, "\tInterlace: %s\n", pfile.interlace == 0 ? "none" : "Adam7");
}

#define FILTER_TYPE_NONE	0
#define FILTER_TYPE_SUB		1
#define FILTER_TYPE_UP		2
#define FILTER_TYPE_AVG		3
#define FILTER_TYPE_PAETH	4

unsigned char paeth(int a, int b, int c)
{
	int p, pa, pb, pc;
	p = a + b - c;
	pa = abs(p - a);
	pb = abs(p - b);
	pc = abs(p - c);
	if(pa <= pb && pa <= pc)
		return a;
	else if(pb <= pc)
		return b;
	else
		return c;
}

void filter_scanline(unsigned char filter_type, unsigned char *filt, int scanline_num)
{
	for(int i = 0; i < pfile.width * 3; i += 3)
	{
		unsigned char a_r = (i == 0 ? 0 : pfile.image[scanline_num * pfile.width * 3 + i - 3]);
		unsigned char a_g = (i == 0 ? 0 : pfile.image[scanline_num * pfile.width * 3 + i + 1 - 3]);
		unsigned char a_b = (i == 0 ? 0 : pfile.image[scanline_num * pfile.width * 3 + i + 2 - 3]);
		
		unsigned char b_r = (scanline_num == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i]);
		unsigned char b_g = (scanline_num == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i + 1]);
		unsigned char b_b = (scanline_num == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i + 2]);
		
		unsigned char c_r = (scanline_num == 0 || i == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i - 3]);
		unsigned char c_g = (scanline_num == 0 || i == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i + 1 - 3]);
		unsigned char c_b = (scanline_num == 0 || i == 0 ? 0 : pfile.image[(scanline_num - 1) * pfile.width * 3 + i + 2 - 3]);
		
		unsigned char r = filt[i];
		unsigned char g = filt[i+1];
		unsigned char b = filt[i+2];
		
		switch(filter_type)
		{
			case FILTER_TYPE_NONE:
				break;
			case FILTER_TYPE_SUB:
				r += a_r;
				g += a_g;
				b += a_b;
				break;
			case FILTER_TYPE_UP:
				r += b_r;
				g += b_g;
				b += b_b;
				break;
			case FILTER_TYPE_AVG:
				r += ((unsigned int)a_r + b_r) / 2;
				g += ((unsigned int)a_g + b_g) / 2;
				b += ((unsigned int)a_b + b_b) / 2;
				break;
			case FILTER_TYPE_PAETH:
				// r = g = b = 0;
				r += paeth(a_r, b_r, c_r);
				g += paeth(a_g, b_g, c_g);
				b += paeth(a_b, b_b, c_b);
				break;
			default:
				debug(ERROR, "Uknown filter type %d", filter_type);
		}
		
		pfile.image[scanline_num * pfile.width * 3 + i] = r;
		pfile.image[scanline_num * pfile.width * 3 + i + 1] = g;
		pfile.image[scanline_num * pfile.width * 3 + i + 2] = b;
	}
}

#define ZLIB_CHUNK_SIZE 32768
void process_IDAT(png_Chunk *chunk)
{
	z_stream strm;
	int num_pixels = pfile.width * pfile.height;
    unsigned char output_buffer[ZLIB_CHUNK_SIZE];
	
	static int idat_count = 0;
	
	int ret;
	int count = 0;
	int left_over = 0;
	static int bytes_processed = 0;
	static int bytes_output = 0;
	
	idat_count++;
	
	pfile.image = (unsigned char *) malloc(num_pixels * 3);
	
	if(idat_count == 1)
	{
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
    
		ret = inflateInit(&strm);
		
		debug_if(ret != Z_OK, ERROR, "inflateInit() failed\n");
	}
	
	// length of 0 is legal
	if (chunk->length == 0) return;
	
	do // while we still have input
	{
		strm.avail_in = chunk->length - bytes_processed;
		strm.next_in = chunk->data + bytes_processed;
		
		do // while we still have input but the ouput is full
		{
			strm.avail_out = ZLIB_CHUNK_SIZE - left_over;
			strm.next_out = output_buffer + left_over;
			
			// Inflate the stream
			ret = inflate(&strm, Z_NO_FLUSH);
			
			// Check for errors
			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;     /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					debug(ERROR, "inflate() error: %s.\n", strm.msg);
					inflateEnd(&strm);
					return;
			}
			int have = ZLIB_CHUNK_SIZE - strm.avail_out;
			
			int full_lines = have / (pfile.width * 3 + 1);
			int i = 0;
			while(full_lines--)
			{
				filter_scanline(output_buffer[i], &output_buffer[i + 1], count++);
				i += (pfile.width * 3 + 1);
			}
			
			left_over = have - i;
			memmove(output_buffer, &output_buffer[i], left_over);
			//memcpy(&pfile.image[bytes_output], output_buffer, have);
			
			bytes_output += (have - left_over);
			printf("inflate()\n");
		} while (strm.avail_out == 0);
		printf("more input!\n");
		// TODO: fix this
		//bytes_processed = chunk->length - strm.avail_in;
	} while (ret != Z_STREAM_END);
	
	FILE *ofile = fopen("out.pxl", "wb");
	fwrite(pfile.image, 1, num_pixels * 3, ofile);
	fclose(ofile);
	
	
	debug(INFO, "Inflated %d to %d bits\n", chunk->length, bytes_output);
	inflateEnd(&strm);
	
	free(pfile.image);
	
	return;
}
void process_IEND(png_Chunk *chunk)
{
}

void process_tEXt(png_Chunk *chunk)
{
	debug(INFO, "%s - %s\n", chunk->data, &chunk->data[strlen(chunk->data) + 1]);
}

void process_gAMA(png_Chunk *chunk)
{
	unsigned int gamma;
	memcpy(&gamma, chunk->data, 4);
	
	gamma = swap32(gamma);
	
	debug(INFO, "Gamma: %d\n", gamma);
}

#define REGISTER_TYPE(x) {#x, *process_##x, 0}
static png_Type_Callback callbacks[] = 
{
	REGISTER_TYPE(IHDR),
	REGISTER_TYPE(IDAT),
	REGISTER_TYPE(IEND),
	REGISTER_TYPE(tEXt),
	REGISTER_TYPE(gAMA)
};

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++)
	{
		c = (unsigned long) n;
		for (k = 0; k < 8; k++)
		{
			if (c & 1) c = 0xedb88320L ^ (c >> 1);
			else c = c >> 1;
		}
		crc_table[n] = c;
	}
}


/* Update a running CRC with the bytes buf[0..len-1]--the CRC
should be initialized to all 1's, and the transmitted value
is the 1's complement of the final running CRC (see the
crc() routine below). */

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
	static int crc_table_computed = 0;
	unsigned long c = crc;
	int n;

	if (!crc_table_computed)
	{
		make_crc_table();
		crc_table_computed = 1;
	}
	
	for (n = 0; n < len; n++)
		c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	return c;
}

#define CRC_ALL_ONES 0xffffffffL
int chunk_is_critical(png_Chunk *chunk)
{
	return (chunk->type[0] & 32) == 0;
}
int chunk_is_privaite(png_Chunk *chunk)
{
	return (chunk->type[1] & 32) == 0;
}
int chunk_is_reserved(png_Chunk *chunk)
{
	return (chunk->type[2] & 32) == 1;
}
int chunk_is_unsafe_to_copy(png_Chunk *chunk)
{
	return (chunk->type[3] & 32) == 0;
}

void process_chunk(png_Chunk *chunk)
{
	static int len = sizeof(callbacks) / sizeof(callbacks[0]);
	int i;
	unsigned long c;
	
	debug(INFO, "\n==================\n");
	debug(INFO, "Type    \"%.4s\"\n", chunk->type);
	debug(INFO, "Length  %d\n", chunk->length);
	debug(INFO, "CRC     0x%X\n", chunk->checksum);
	debug(INFO, "==================\n");
	
	// verify the type...not much we can do here
	debug_if(chunk_is_reserved(chunk), ERROR, "Chunk type has reserved bit set (i.e. type[2] is upper case)\n");
	
	// verify the checksum
	c = CRC_ALL_ONES;
	c = update_crc(c, chunk->type, sizeof(chunk->type));
	c = update_crc(c, chunk->data, chunk->length);
	c ^= CRC_ALL_ONES;
	
	if(c != chunk->checksum)
	{
		debug(WARN, "Mismatched checksum, calculated as %X\n", c);
		debug_if(chunk_is_critical(chunk), ERROR, "Critical chunk has wrong checksum\n");
		return;
	}
	
	for(i = 0; i < len; ++i)
	{
		if(callbacks[i].type_as_int == chunk->type_as_int)
		{
			callbacks[i].count++;
			(*callbacks[i].callback)(chunk);
			break;
		}
	}
	
	debug_if(i == len, WARN, "Unregistered chunk type %.4s\n", chunk->type);
}

int main(void)
{
	const unsigned char header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	unsigned int c;
	unsigned char buffer[8];
	png_Chunk chunk;
	
	debug_level = INFO;
	
	pfile.file = fopen("test.png", "rb");
	
	debug_if(pfile.file == NULL, ERROR, "cannot open file\n");
	
	fread(buffer, 1, 8, pfile.file);
	
	debug_if(memcmp(header, buffer, 8), ERROR, "header not indicitive of a png file\n");
	
	do
	{
		chunk.data = NULL;
		
		// Read in chunk
		fread(buffer, 1, 8, pfile.file);
		
		if(feof(pfile.file)) break;
		
		memcpy(&chunk, buffer, 8);
		
		chunk.length = swap32(chunk.length);
		if(chunk.length > 0)
			chunk.data = (unsigned char *) malloc(chunk.length);
		fread(chunk.data, 1, chunk.length, pfile.file);
		fread(&chunk.checksum, 1, 4, pfile.file);
		chunk.checksum = swap32(chunk.checksum);
		
		// Process the chunk
		process_chunk(&chunk);
		
		// Clean it up
		if(chunk.data) free(chunk.data);
	} while(!feof(pfile.file));
	
	fclose(pfile.file);
	
	system("pause");
	return 0;
}