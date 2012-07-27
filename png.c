#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	FILE *png_file;
	unsigned int c;
	unsigned char buffer[100];
	
	png_file = fopen("test.png", "rb");
	
	if(png_file == NULL)
	{
		printf("Error: cannot open file\n");
		return 1;
	}
	
	for(int i = 0; i < 8 ; ++i)
	{
		c = fgetc(png_file);
		printf("%02x ", c);
	}
	printf("\n");
	fclose(png_file);
	return 0;
}