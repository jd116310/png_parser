#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} png_Type_Callback;

unsigned int swap32(unsigned int val) 
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val >> 8) & 0xFF00) | ((val >> 24) & 0xFF);
}

void process_IHDR(png_Chunk *chunk)
{
	printf("Hello from IHDR\n");
}
void process_IDAT(png_Chunk *chunk)
{
	printf("Hello from IDAT\n");
}
void process_IEND(png_Chunk *chunk)
{
	printf("Hello from IEND\n");
}

#define REGISTER_TYPE(x) {#x,*process_##x}
static png_Type_Callback callbacks[] = 
{
	REGISTER_TYPE(IHDR),
	REGISTER_TYPE(IDAT),
	REGISTER_TYPE(IEND)
};

void process_chunk(png_Chunk *chunk)
{
	int len = sizeof(callbacks) / sizeof(callbacks[0]);
	int i;
	for(i = 0; i < len; ++i)
	{
		if(callbacks[i].type_as_int == chunk->type_as_int)
		{
			(*callbacks[i].callback)(chunk);
			break;
		}
	}
	
	if(i == len)
		printf("Unregistered chunk type %.4s\n", chunk->type);
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
		chunk.checksum = swap32(chunk.checksum);
		
		// Print it out
		//printf("%d %.4s\n", chunk.length, chunk.type);
		
		// Process it
		process_chunk(&chunk);
		
		// Clean it up
		free(chunk.data);
	} 
	fclose(png_file);
	
	system("pause");
	return 0;
}