/*
 * generate matrix with random elements with values = [randMinValue; randMaxValue]
 * and save matrix to file 'filename'
 * all matrix parameters is initial from command-line arguments
 * 
 * [run]:
 * ./a.out matrixHeight matrixWidth randMinValue randMaxValue filename
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARG_ERROR_MESS		"./a.out matrixHeight matrixWidth randMinValue randMaxValue filename"

int matrixGenerator(long matrixHeight, long matrixWidth, long randMin, long randMax, char *fileName)
{  
  FILE *file = NULL;
  if((file = fopen(fileName, "wb")) < 0)		// open matrix file for read
  {
    perror("fopen: ");
    fprintf(stdout, "error open '%s' wb mode\n", fileName);
    return -1;
  } 
  long i, j;
  double v;
  for (i = 0; i < matrixHeight; i++) 
  {
        for (j = 0; j < matrixWidth; j++) 
	{
	    v = randMin + rand() % (randMax - randMin);
            fprintf(file, "%ld ", (long)v);
        }
        fseek(file, -1, SEEK_CUR);
	fputc('\n', file);
  }
  fseek(file, -1, SEEK_CUR);
  ftruncate(fileno(file), ftell(file));				// crop last byte of file
  fclose(file);
  return 0;
}

int main(int argc, char *argv[]) 
{
    srand(time(NULL));
    if (argc != 6) 
    {
        puts(ARG_ERROR_MESS);
        return -1;
    }
    matrixGenerator(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), argv[5]);
    return ;
}