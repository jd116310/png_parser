#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

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
	printf("process_IHDR()\n");
	
	if(chunk->length != PNG_IHDR_SIZE)
	{
		printf("Error: IHDR size != %d\n", PNG_IHDR_SIZE);
		return;
	}
	
	memcpy(&pfile, chunk->data, PNG_IHDR_SIZE);
	
	pfile.width = swap32(pfile.width);
	pfile.height = swap32(pfile.height);
	
	printf("\tImage stats:\n");
	printf("\t\t%dpx x %dpx\n", pfile.width, pfile.height);
	printf("\t\tBit Depth = %d bits per sample\n", pfile.bit_depth);
	printf("\t\tColor type:\n");
	if(pfile.color_type & PNG_IHDR_PALETTE_USED) printf("\t\t\tPalette\n");
	if(pfile.color_type & PNG_IHDR_COLOR_USED)	printf("\t\t\tColor\n");
	if(pfile.color_type & PNG_IHDR_APLHA_USED)	printf("\t\t\tAlpha\n");
	if(pfile.compression == 0) printf("\t\tCompression: method 0\n");
	if(pfile.filter == 0) printf("\t\tFilter: method 0\n");
	printf("\t\tInterlace: %s\n", pfile.interlace == 0 ? "none" : "Adam7");
}
#define ZLIB_CHUNK_SIZE 32768
#define min(a,b) (((a) < (b)) ? (a) : (b))
void process_IDAT(png_Chunk *chunk)
{
    static z_stream strm;
    static unsigned char out[ZLIB_CHUNK_SIZE];
	
	static int idat_count = 0;
	
	int ret;
	int bytes_processed = 0;
	int bytes_output = 0;
	unsigned char *in;
	
	idat_count++;
	
	printf("process_IDAT()\n");
	if(idat_count == 1)
	{
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
    
		ret = inflateInit(&strm);
		if (ret != Z_OK)
			printf("inflateInit() failed\n");
	}
	if (chunk->length == 0)
		return;
	do	
	{
		strm.avail_in = min(chunk->length - bytes_processed, ZLIB_CHUNK_SIZE);
		strm.next_in = chunk->data + bytes_processed;
		bytes_processed += strm.avail_in;
		
		do
		{
			strm.avail_out = ZLIB_CHUNK_SIZE;
			strm.next_out = out;
			
			ret = inflate(&strm, Z_NO_FLUSH);
			
			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;     /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					printf("inflate() error: %s.\n", strm.msg);
					inflateEnd(&strm);
					return;
			}
			int have = ZLIB_CHUNK_SIZE - strm.avail_out;
			bytes_output += (ZLIB_CHUNK_SIZE - strm.avail_out);
		} while (strm.avail_out == 0);
	} while (ret != Z_STREAM_END);
	printf("\tInflate %d to %d bits\n", chunk->length, bytes_output);
	inflateEnd(&strm);
	return;
}
void process_IEND(png_Chunk *chunk)
{
	printf("process_IEND()\n");
}

void process_tEXt(png_Chunk *chunk)
{
	printf("process_tEXt() %d\n", chunk->length);
	
	printf("\t%s\n", chunk->data);
	printf("\t%s\n", &chunk->data[strlen(chunk->data) + 1]);
}

#define REGISTER_TYPE(x) {#x, *process_##x, 0}
static png_Type_Callback callbacks[] = 
{
	REGISTER_TYPE(IHDR),
	REGISTER_TYPE(IDAT),
	REGISTER_TYPE(IEND),
	REGISTER_TYPE(tEXt)
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
			if (c & 1)
			c = 0xedb88320L ^ (c >> 1);
			else
			c = c >> 1;
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
	int len = sizeof(callbacks) / sizeof(callbacks[0]);
	int i;
	unsigned long c = CRC_ALL_ONES;
	
	printf("\n==================\n");
	printf("Type    \"%.4s\"\n", chunk->type);
	printf("Length  %d\n", chunk->length);
	printf("CRC     0x%X\n", chunk->checksum);
	printf("==================\n");
	
	// verify the type...not much we can do here
	if(chunk_is_reserved(chunk))
	{
		printf("Chunk type has reserved bit set (i.e. type[2] is upper case)\n");
		return;
	}
	
	// verify the checksum
	c = update_crc(c, chunk->type, sizeof(chunk->type));
	c = update_crc(c, chunk->data, chunk->length);
	c ^= CRC_ALL_ONES;
	
	if(c != chunk->checksum)
	{
		printf("Error, checksum does not match!\n");
		if(chunk_is_critical(chunk))
		{
			// abort since it is critical
			// return;
		}
		printf("Calculated checksum as %x\n", c);
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
	
	if(i == len)
		printf("Unregistered chunk type %.4s\n", chunk->type);
}

int main(void)
{
	const unsigned char header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	unsigned int c;
	unsigned char buffer[8];
	png_Chunk chunk;
	
	pfile.file = fopen("test.png", "rb");
	printf("%d\n", sizeof(pixel));
	
	if(pfile.file == NULL)
	{
		printf("Error: cannot open file\n");
		return 1;
	}
	
	fread(buffer, 1, 8, pfile.file);
	if(!memcmp(header, buffer, 8))
		printf("Magic header found\n");
	
	while(!feof(pfile.file))
	{
		// Read in chunk
		fread(buffer, 1, 8, pfile.file);
		
		if(feof(pfile.file)) break;
		
		memcpy(&chunk, buffer, 8);
		
		chunk.length = swap32(chunk.length);
		if(chunk.length > 0);
			chunk.data = (unsigned char *) malloc(chunk.length);
		fread(chunk.data, 1, chunk.length, pfile.file);
		fread(&chunk.checksum, 1, 4, pfile.file);
		chunk.checksum = swap32(chunk.checksum);
		
		// Print it out
		//printf("%d %.4s\n", chunk.length, chunk.type);
		
		// Process it
		process_chunk(&chunk);
		
		// Clean it up
		free(chunk.data);
	} 
	fclose(pfile.file);
	
	system("pause");
	return 0;
}