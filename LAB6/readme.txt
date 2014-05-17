Multiply 2 matrix using MPI.

[compile]:
mpicc ./mpiMatrix.c -std=gnu99 -lm

[RUN]:
mpiexec -n <X> ./a.out matrixA_file matrixB_file [-async]
mpiexec -n <X> ./a.out matrixA_file matrixB_file [-coll] <Y> [-r]
[ARGS:]
<X>: number of process to run
-async: enable async mode
-coll: use collective operatons
<Y>: number of process groups
-r: generate random process count"

[RUN_EXAMPLE]:
mpiexec -n 5 ./a.out ./input/a.txt ./input/b.txt
mpiexec -n 7 ./a.out ./input/a.txt ./input/b.txt -async
mpiexec -n 10 ./a.out ./input/a.txt ./input/b.txt -coll 2 [-r]
mpiexec -n 14 ./a.out ./input/a.txt ./input/b.txt -coll 3 -r
mpiexec --host 192.168.11.135,192.168.11.134 -n 110 /mnt/studpublic/mpi/a.out /mnt/studpublic/mpi/input/a.txt /mnt/studpublic/mpi/input/b.txt -coll 2 -r

