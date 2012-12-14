#include <stdio.h>
#include <mpi.h>


void initMPI(int argc, char* argv[])
{
    int numprocs, rank, namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];



    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);

    printf("Process %d on %s out of %d\n", rank, processor_name, numprocs);

    MPI_Finalize();
}

int main(int argc, char *argv[])
{
    //int d = 4;
    //char * argp[4] = {"mpirun", "-np", "8",  "MPI Game of Life"};
    printf("Max processors: %d\n", argc);

    initMPI(argc, argv);
}
