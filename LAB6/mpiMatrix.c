#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARG_ERROR_MESS		"./a.out [matrixA_file] [matrixB_file] [matrixRESULT_file]"


int 		**matrixA = NULL, **matrixB = NULL, **matrixRESULT = NULL;	
FILE		*fileResult = NULL;
long		aH = 1, aW = 0;
long		bH = 1, bW = 0;
long		resultH, resultW;

// initial height, widht, allocate memory for matrix,
// return pointer int* matrix to allocated matrix memory 
int** openMatrix(char *matrix_file, long *height, long *width)
{
  FILE *file = NULL;
  int **matrix = NULL;
  long localH = 1, localW = 0;
  int ch;
  char tmpNumber[1024];		
  if(access(matrix_file, F_OK) < 0)
  {
      fprintf(stderr, "error: input file '%s' does int not exist\n", matrix_file);
      return NULL;
  }
  if((file = fopen(matrix_file, "rb")) < 0)		// open matrix file for read
  {
    perror("fopen: ");
    fprintf(stdout, "error initial from %s\n", matrix_file);
    return NULL;
  }
  while((ch = fgetc(file)) != EOF)
  {
    switch(ch)						// calculate matrix size
    {
      case ' ':
      {
	localW++;
	break;
      }
      case '\n':
      {
	localH++;
	break;
      }
      default:
      {
	break;
      }
    }      
  }  
  localW = (localW + localH)/(localH);	
  *height = localH;
  *width = localW;
  //printf("\n%s W=%ld, H=%ld\n", matrix_file, localW, localH);	// show matrix info
  matrix = (int**)malloc(localH * sizeof(int*));		// allocate memory for matrix
  for(ch = 0; ch < localH; ch++)
    matrix[ch] = (int*)malloc(localW * sizeof(int));		// allocate memory for matrix
  fseek(file, 0, SEEK_SET);
  long y, x, t;
  for(y = 0; y < localH; y++)					// fill matrix
  {
    for(x = 0; x < localW; x++)
    {
      t = 0;
      do
      {
	tmpNumber[t++] = fgetc(file);	
      }while((tmpNumber[t-1] != EOF) && (tmpNumber[t-1] != ' ') && (tmpNumber[t-1] != '\n'));
      tmpNumber[t-1] = '\0';      
      matrix[y][x] = atoi(tmpNumber); 	
      //printf("%d ", matrix[y][x]);				// show element
    }
    //printf("\n");						// next string
  }
  fclose(file);
  return matrix;
}

int init(char *matrixA_file, char *matrixB_file)
{
  long i;
  if((matrixA = openMatrix(matrixA_file, &aH, &aW)) == NULL)	// initial matrixA
    return -1;
  if((matrixB = openMatrix(matrixB_file, &bH, &bW)) == NULL)	// initial matrixB
  {
    for(i = 0; i < aH; i++)
      free(matrixA[i]);
    free(matrixA);
    return -1;
  }
  resultH = aH;
  resultW = bW;
  matrixRESULT = (int**)malloc(resultH * sizeof(int*));		// allocate mem for matrixRESULT
  for(i = 0; i < resultH; i++)
    matrixRESULT[i] = (int*)malloc(resultW * sizeof(int));
  return 0;
}

int multiplyMatrix()
{
  long x, y, aX, aY, bX, bY;
  int tmp;
  for(y = 0; y < resultH; y++)					// multiply matrix
  {
    for(x = 0; x < resultW; x++)
    {
      tmp = 0;
      for(aX = 0; aX < aW; aX++)
	tmp += matrixA[y][aX] * matrixB[aX][x];	
      matrixRESULT[y][x] = tmp;
    }
  }  
  return 0;
}

int saveMatrixFile(char *matrix_file, int **matrix, long height, long width)
{								// write matrix to file
  long x, y, tmpLen;
  char tmp[1024];
  FILE *file = NULL;
  if((file = fopen(matrix_file, "wb")) < 0)
  {
    perror("fopen: ");
    fprintf(stdout, "error saveMatrixFile from %s\n", matrix_file);
    return -1;
  }
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {      
      sprintf(tmp,"%d ",matrix[y][x]);
      fwrite(tmp, strlen(tmp), 1, file);
    }
    fseek(file, -1, SEEK_CUR);
    fputc('\n', file);
  }  
  fseek(file, -1, SEEK_CUR);
  ftruncate(fileno(file), ftell(file));				// crop last byte of file
  fclose(file);
  return 0;
}

void freeMatrixMem()						// free allocated matrix mem
{
  long i;
  for(i = 0; i < aH; i++)
      free(matrixA[i]);
    free(matrixA);
  for(i = 0; i < bH; i++)
      free(matrixB[i]);
    free(matrixB);
  for(i = 0; i < resultH; i++)
      free(matrixRESULT[i]);
    free(matrixRESULT);
}

int main(int argc, char *argv[])
{
  if(argc > 3)
  {
    if(init(argv[1], argv[2]) == -1)				// if error initial
      return -1;      
    else							// if all is ok
    {
      // matrix[string][column] == matrix[y][x]
      if(aW != bH)
      {
	fprintf(stderr, "error: matrix '%s' width and matrix '%s' height doesn't match\n", argv[1], argv[2]);
	return -1;
      }      
      multiplyMatrix();
      saveMatrixFile(argv[3], matrixRESULT, resultH, resultW);
      freeMatrixMem();
    }
  }
  else
  {
    puts(ARG_ERROR_MESS);
    return -1;
  }
  return 0;
}