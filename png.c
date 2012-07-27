#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct png_Chunk
{
	unsigned int length;
	unsigned char type[4];
	unsigned char *data;
	unsigned int checksum;
} png_Chunk;

unsigned int swap32(unsigned int val) 
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val >> 8) & 0xFF00) | ((val >> 24) & 0xFF);
}

void process_chunk(png_Chunk *chunk)
{
}

int main(void)
{
	png_Chunk chunk;
	FILE *png_file;
	const unsigned char header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	unsigned int c;
	unsigned char buffer[8];
	
	png_file = fopen("test.png", "rb");
	
	if(png_file == NULL)
	{
		printf("Error: cannot open file\n");
		return 1;
	}
	
	fread(buffer, 1, 8, png_file);
	if(!memcmp(header, buffer, 8))
		printf("Magic header found\n");
	
	while(!feof(png_file))
	{
		// Read in chunk
		fread(buffer, 1, 8, png_file);
		if(feof(png_file)) break;
		memcpy(&chunk, buffer, 8);
		chunk.length = swap32(chunk.length);
		chunk.data = (unsigned char *) malloc(chunk.length);
		fread(chunk.data, 1, chunk.length, png_file);
		fread(&chunk.checksum, 1, 4, png_file);
		chunk.checksum = swap(chunk.checksum);
		
		// Print it out
		printf("%d %.4s\n", chunk.length, chunk.type);
		
		// Clean it up
		free(chunk.data);
	} 
	fclose(png_file);
	
	system("pause");
	return 0;
}