#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#define TAG_BLOCK_SIZE    2
#define TAG_MATRIX_FIRST  3
#define TAG_MATRIX_RESULT 4
#define outputFolder "./output/"
#define hostResultFile "./output/host.txt"
#define mpiResultFile "./output/mpi.txt"

#define ARG_ERROR_MESS	"[RUN]:\nmpiexec -n X ./a.out matrixA_file matrixB_file [-async]\nmpiexec -n X ./a.out matrixA_file matrixB_file [-coll] Y [-r]\n[ARGS:]\nX: number of process to run\n-async: enable async mode\n-coll: use collective operatons\nY: number of process groups\n-r: generate random process count"
#define debug(format, ...) fprintf(stderr, "[%d]: ", globalRank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);
#define error(format, ...) fprintf(stderr, "[%d]: ", globalRank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);

int globalRank, globalSize, groupNumber, async = 0;
int localGroupRank, localGroupSize;


typedef struct 
{
    double *a;
    int width, height;
} matrix_t;


matrix_t A, B, C, C_self;

int matrixMul(matrix_t *A, matrix_t  *B, matrix_t *C) 
{
    if (A->width != B->height) 
    {
        error("Matrix sizes doesn't match A=%p, width=%d, height=%d, B=%p, width=%d, height=%d\n", A, A->width, A->height, B, B->width, B->height);
        return -1;
    }

    C->height = A->height;
    C->width = B->width;

    C->a = (double*) calloc(C->width*C->height, sizeof(double));

    for (int row = 0; row < A->height; row++) 
    { //row
        for (int column = 0; column < B->width; column++) 
	{ //collumn
            C->a[row * C->width + column] = 0;
            for (int k = 0; k < B->height; k++) 
	    { //sum
                C->a[row * C->width + column] += A->a[row * A->width + k] * B->a[column + k*B->width];
            }
        }
    }
}

int matrixCmp(matrix_t *m1, matrix_t *m2) {
    if (m1->width != m2->width || m1->height != m2->height)
        return 1;
    return memcmp(m1->a, m2->a, m1->width * m1->height * sizeof(double));
}

// calculate how many strings (rows) process need to process by processRank
int calculateRowsForPeer(const int processRank, int matrixHeight, int procCount) 
{
    int a_row_count = (int) ceil( (double)matrixHeight / (procCount));

    if (processRank * a_row_count <= matrixHeight) 
    {
        if ((processRank+1) * a_row_count <= matrixHeight)
            return  a_row_count;
        else if((processRank+1) * a_row_count >= matrixHeight)
            return matrixHeight - (processRank * a_row_count);
    }
    else
        return 0;
}

int matrixPrint(matrix_t* m)
{
    if (m == NULL)
        return -1;
    printf("____________________________________________________\n");
    printf("Matrix width: %d, height: %d\n", m->width, m->height);

    for (int i=0; i<m->height; i++) {
        for (int j=0; j<m->width; j++) {
            printf("%5.2lf ", m->a[i*m->width + j]);
        }
        printf("\n");
    }
    printf("====================================================\n");
}

// initial height, widht, allocate memory for matrix,
// return pointer int* matrix to allocated matrix memory 
double* openMatrix(char *matrix_file, matrix_t* m)
{
  FILE *file = NULL;
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
  m->width = localW;
  m->height= localH;
  //printf("\n%s W=%ld, H=%ld\n", matrix_file, localW, localH);	// show matrix info
  m->a = (double*) malloc(m->width * m->height * sizeof(double));
  fseek(file, 0, SEEK_SET);
  long y, x, t;
  double v;
  for (int i=0; i<m->height; i++) 
  {
        for (int j=0; j<m->width; j++) 
	{
            fscanf(file, "%lf", &v);
            m->a[i*m->width + j] = v;
        }
  }
  fclose(file);
  return m->a;
}

int loadFromFile(char *matrixA_file, char *matrixB_file) 
{
  if((openMatrix(matrixA_file, &A)) == NULL)	// initial matrixA
    return -1;
  if((openMatrix(matrixB_file, &B)) == NULL)	// initial matrixB
  {
    free(A.a);
    return -1;
  }
    //matrixPrint(&A);
    //matrixPrint(&B);

    C.height = A.height;
    C.width = B.width;
    C.a = (double*) malloc(C.width * C.height * sizeof(double));
    return 0;
}

void masterRankController() 
{
    int workers_count = globalSize - 1;
    int real_workers_count = 0;

    
    
    //send matrix B to all peers
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    //send block_size to all peers
    if (async == 0) 
    {
        for (int i = 0; i < workers_count; i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);
            MPI_Send(&current_worker_block_size, 1, MPI_INT, i + 1, TAG_BLOCK_SIZE, MPI_COMM_WORLD);
        }

    } 
    else 
    {
        MPI_Request *requests = calloc(workers_count, sizeof(MPI_Request));
        MPI_Status *statuses = calloc(workers_count, sizeof(MPI_Status));

        for (int i = 0; i < workers_count; i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);
            MPI_Isend(&current_worker_block_size, 1, MPI_INT, i + 1, TAG_BLOCK_SIZE, MPI_COMM_WORLD, &requests[i]);
        }

        //debug("Wait all peers (%i) received computing block size\n", workers_count);
        MPI_Waitall(workers_count, requests, statuses);
        free(requests);
        free(statuses);
    }

    //send rows of matrix A to peers
    //debug("Sending matrix A to all peers\n");
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    if (async == 0) 
    {
        for (int i = 0; i < workers_count && calculateRowsForPeer(i,A.height,workers_count); i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);
            //debug("A row count for %i: %i\n", i+1, current_worker_block_size);
            MPI_Send(A.a + (A.width * i * calculateRowsForPeer(i-1,A.height,workers_count)),
		     current_worker_block_size * A.width, MPI_DOUBLE, i + 1,
                     TAG_MATRIX_FIRST, MPI_COMM_WORLD);
            real_workers_count++;
        }
    } 
    else 
    {
        MPI_Request *requests = calloc(workers_count, sizeof(MPI_Request));
        MPI_Status *statuses = calloc(workers_count, sizeof(MPI_Status));

        for (int i = 0; i < workers_count && calculateRowsForPeer(i,A.height,workers_count); i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);

            MPI_Isend(A.a + (A.width * i *  calculateRowsForPeer(i-1,A.height,workers_count)), current_worker_block_size * A.width, MPI_DOUBLE, i + 1,
                      TAG_MATRIX_FIRST, MPI_COMM_WORLD, &requests[i]);
            real_workers_count++;
        }

        //debug("Waiting while all (%i) peer received matrix A\n", real_workers_count);
        MPI_Waitall(real_workers_count, requests, statuses);
        free(requests);
        free(statuses);
    }

    //receive rows result matrix C
    //debug("Trying to compose result matrix. Waiting data from peers.\n");
    if (async == 0) 
    {
        for (int i = 0; i < real_workers_count; i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);
            MPI_Recv(C.a +(C.width * i * current_worker_block_size), current_worker_block_size * C.width,
                     MPI_DOUBLE, i + 1, TAG_MATRIX_RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } 
    else 
    {
        MPI_Request *requests = calloc(real_workers_count, sizeof(MPI_Request));
        MPI_Status *statuses = calloc(real_workers_count, sizeof(MPI_Status));

        for (int i = 0; i < real_workers_count; i++) 
	{
            int current_worker_block_size = calculateRowsForPeer(i,A.height,workers_count);
            MPI_Irecv(C.a +(C.width * i * current_worker_block_size), current_worker_block_size * C.width,
                      MPI_DOUBLE, i + 1, TAG_MATRIX_RESULT, MPI_COMM_WORLD, &requests[i]);
        }
        //debug("Waiting while all (%i) peer submit result matrix\n", real_workers_count);
        MPI_Waitall(real_workers_count, requests, statuses);
        free(requests);
        free(statuses);
        //debug("Root done");
    }
}

int slaveRankExecute() 
{
    //receiving full B matix from master-peer
    int a_row_count;
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    B.a = (double *) malloc(B.width * B.height * sizeof(double));
    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    //debug("Matrix B (%p) has been received: width=%d, height=%d\n", B.a, B.width, B.height);

    if (async == 0 || 1) 
    {
        MPI_Recv(&a_row_count, 1, MPI_INT, 0, TAG_BLOCK_SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } 
    else 
    {
        MPI_Request request;
        MPI_Status status;
        MPI_Irecv(&a_row_count, 1, MPI_INT, 0, TAG_BLOCK_SIZE, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }

    //debug("block_size has been received: %d\n", a_row_count);
    if (a_row_count == 0) 
    {
        //debug("Nothing to do. Waiting for another peers.\n");
        return 0;
    }
    A.height = a_row_count;

    //**********************************************************
    //receiving matrix A
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    size_t peer_mem = A.height * A.width * sizeof(double);
    //debug("mem allocated: %d\n", peer_mem);
    A.a = (double*) malloc(peer_mem);

    if (async == 0) 
    {
        MPI_Recv(A.a, A.height * A.width, MPI_DOUBLE, 0, TAG_MATRIX_FIRST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } 
    else 
    {
        MPI_Request request;
        MPI_Status status;
        MPI_Irecv(A.a, A.height * A.width, MPI_DOUBLE, 0, TAG_MATRIX_FIRST, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }
    //debug("Matrix A(%p) has been received from main peer: width=%d, height=%d\n", A.a, A.width, A.height);
    //matrixPrint(&A);

    //************************************************************
    matrixMul(&A, &B, &C);
    //debug("Matrix multiple found\n");
    //matrixPrint(&C);

    if (async == 0) 
    {
        MPI_Send(C.a, C.height * C.width, MPI_DOUBLE, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD);
    } 
    else 
    {
        MPI_Request request;
        MPI_Status status;
        MPI_Isend(C.a, C.height * C.width, MPI_DOUBLE, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }
    //debug("Peer computing done. Results synced.\n");

    return 0;
}

int saveMatrixFile(char *matrix_file, matrix_t* m)
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
  double v;
  for (int i=0; i<m->height; i++) 
  {
        for (int j=0; j<m->width; j++) 
	{
	    v = m->a[i*m->width + j];
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



int groupOperations(MPI_Comm comm) 
{
    matrix_t pA, pC;

    //receiving A.height by scalability reasons
    MPI_Bcast(&A.height, 1, MPI_INTEGER, 0, comm);
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, comm);
    pA.width = A.width;

    int a_row_count = calculateRowsForPeer(localGroupRank,A.height,localGroupSize);
    pA.height = a_row_count;
    //printf("--[%d] calculateRowsForPeer %d, localGroupRank %d, localGroupSize: %d. w: %d, h: %d\n", 
    //	groupNumber, calculateRowsForPeer(localGroupRank,A.height,localGroupSize), localGroupRank, localGroupSize, pA.width, a_row_count);
    pA.a = (double*) calloc(pA.height * pA.width, sizeof(double));

    //receiving full B matix from master-peer
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, comm);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, comm);

    if (localGroupRank != 0)
        B.a = (double *) malloc(B.width * B.height * sizeof(double));

    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, comm);

    //printf("[%d][%d] localGroupRank: %d, localGroupSize: %d, pA: w=%i, h=%i; B: w=%i, h=%i\n", 
	   //localGroupRank, groupNumber, localGroupRank, localGroupSize, pA.width, pA.height, B.width, B.height);

    int sendcounts[localGroupSize];
    int displ[localGroupSize];

    if (localGroupRank == 0) 
    {
        memset(sendcounts, 0x0, sizeof(sendcounts));
        memset(displ, 0x0, sizeof(displ));
        for (int i=0; i < localGroupSize; i++) 
	{
            sendcounts[i] = calculateRowsForPeer(i,A.height,localGroupSize) * A.width;
            displ[i] = sendcounts[i] == 0  ? 0 : i * calculateRowsForPeer(i-1,A.height,localGroupSize) * A.width;
            //debug("Scatterv %i %i\n", sendcounts[i], displ[i]);
        }
    }

    MPI_Scatterv(A.a, sendcounts, displ, MPI_DOUBLE,
                 pA.a, pA.width * pA.height, MPI_DOUBLE,
                 0, comm);

    //matrixPrint(&pA);
    matrixMul(&pA, &B, &pC);
    //matrixPrint(&pC);

    int recvcount[localGroupSize];
    int rdispl[localGroupSize];

    if (localGroupRank == 0) 
    {
        for (int i=0; i<localGroupSize; i++)
	{
            //We should use C.width instead of B.widht there, but I wan't to bcast C.width to all peers
            recvcount[i] = calculateRowsForPeer(i,A.height,localGroupSize) * B.width;
            rdispl[i] = calculateRowsForPeer(i-1,A.height,localGroupSize) * B.width * i;
            //debug("Gatherv %i %i\n", recvcount[i], rdispl[i]);
        }
    }

    MPI_Gatherv(pC.a, pC.width * pC.height, MPI_DOUBLE,
                C.a, recvcount, rdispl, MPI_DOUBLE,
                0, comm);

    free(pA.a);
    free(pC.a);

    return 0;
}

int main(int argc, char *argv[]) 
{
    srand(time(NULL));
    if (argc < 3) 
    {
        puts(ARG_ERROR_MESS);
        return -1;
    }
    if(argc == 4)
    {
      if(!strcmp(argv[3], "-async"))
	async = 1;
      else
      {
	puts(ARG_ERROR_MESS);	
	return -1;
      }
    }    
    int status;
    double start_time, end_time;
    MPI_Init(&argc, &argv);    
    MPI_Comm_rank(MPI_COMM_WORLD, &globalRank);
    MPI_Comm_size(MPI_COMM_WORLD, &globalSize);
    if (globalSize < 2) 
    {
      MPI_Finalize();
      error("minimum 2 peers required\n");
      return -1;
    }   
    if(argc > 4 && !strcmp(argv[3], "-coll"))					// collective operations on
    { 
       int groupCount = atoi(argv[4]);
           //groupNumbering peers
       if(argc == 6 && !strcmp(argv[5], "-r"))					// random on
       {	 
	 if(!globalRank)
	   groupNumber = rand() % (globalRank+1);
	 else
	   groupNumber = rand() % (globalRank);
	 //printf("%d\n", groupNumber);
	 groupNumber = groupNumber % groupCount;
	 localGroupRank = 0;							// wrong value, bacause impossible to 
										// determite localGroupRank (because random in use)	
       }
       else
       {
	  groupNumber = (globalRank) % groupCount;
	  localGroupRank = globalRank / groupCount;
       }
	
	MPI_Comm comm;
	MPI_Comm_split(MPI_COMM_WORLD, groupNumber, localGroupRank, &comm);
	
	MPI_Comm_rank(comm, &localGroupRank);
	MPI_Comm_size(comm, &localGroupSize);
	//printf("[%ld][%d][%d] groupNumber: %d, globalRank %d, globalSize: %d\n", 
	//       (long)comm, localGroupSize, localGroupRank, groupNumber, globalRank, globalSize);
	
	int status;
	//printf("--[%ld][%d][%d] groupNumber: %d, globalRank %d, globalSize: %d\n", 
	//       (long)comm, localGroupSize, localGroupRank, groupNumber, globalRank, globalSize);
	if(localGroupRank == 0)
	{
	  if(loadFromFile(argv[1], argv[2]) == -1)
	  {
	    MPI_Finalize();
	    return -1; 
	  }
	  if(A.width != B.height)
	  {
	    MPI_Finalize();
	    free(A.a);
	    free(B.a);
	    free(C.a);
	    fprintf(stderr, "error: matrix '%s' width and matrix '%s' height doesn't match\n", argv[1], argv[2]);
	    return -1;
	  }
	  matrixMul(&A, &B, &C_self);
	  start_time = MPI_Wtime();
	}
	groupOperations(comm);
	if(localGroupRank == 0)
	{
	  end_time = MPI_Wtime();
	  char groupFileName[32];
	  sprintf(groupFileName, "%smpi%d.txt" , outputFolder, groupNumber);
	  status = matrixCmp(&C, &C_self);	  
	  printf("[%d]: procCount: %d, matrixCmp: %d, time: %lf\n", groupNumber, localGroupSize, status, end_time - start_time);	  
	  saveMatrixFile(groupFileName, &C);
	  saveMatrixFile(hostResultFile, &C_self);
	  MPI_Comm_free(&comm);
	}
    }
    else								// not collective operations
    {	
      if (globalRank == 0) 
      {        
	  if(loadFromFile(argv[1], argv[2]) == -1)
	    return -1; 
	  if((A.height % 2))
	  {
	    free(A.a);
	    free(B.a);
	    free(C.a);
	    fprintf(stderr, "error: matrix '%s' height must be an even number. (Height %% 2 must be 0)\n", argv[1]);
	    MPI_Abort(MPI_COMM_WORLD, -1);    
	    return -1;
	    
	  }
	  if(A.width != B.height)
	  {
	    free(A.a);
	    free(B.a);
	    free(C.a);
	    fprintf(stderr, "error: matrix '%s' width and matrix '%s' height doesn't match\n", argv[1], argv[2]);
	    MPI_Abort(MPI_COMM_WORLD, -1);
	    return -1;
	  }   
	  start_time = MPI_Wtime();
	  matrixMul(&A, &B, &C_self);
	  end_time = MPI_Wtime();
	  printf("[Self Host time]: %f\n", end_time - start_time);
	  saveMatrixFile(hostResultFile, &C_self);
      
	  start_time = MPI_Wtime();
	  masterRankController();
	  end_time = MPI_Wtime();
	  if(argc == 4)
	    printf("[Mpi Async time]: %f\n", end_time - start_time);
	  else
	    printf("[Mpi  Sync time]: %f\n", end_time - start_time);
	  saveMatrixFile(mpiResultFile, &C);
	  status = matrixCmp(&C, &C_self);
	  if(!status)
	    printf("Self and Mpi results are the same\n");
	  else
	    printf("Self and Mpi results are different\n");
      } 
      else 
	  status = slaveRankExecute(); 
    }
    MPI_Finalize();
    free(A.a);
    free(B.a);
    free(C.a);
    return status;
}
