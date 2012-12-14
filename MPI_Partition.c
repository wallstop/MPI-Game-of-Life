#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>

#include "GeometrySplitter.h"

int identity;
int actualPartitions;
struct partition *partitionArray;
char* localBoard;
char* nextGenBoard;
char* masterBoard;

bool isActiveProcess;
//Used for masterBoard specifications
int masterBoard_columns;
int masterBoard_rows;

//Used for each process to determine the size of their piece of board
struct partition myCoords;
int numberOfGenerations;
int myNeighborIDs[8];
MPI_Status lastStatus;
MPI_Request lastRequest;

int localBoard_Size;

MPI_Comm updatedComm;
MPI_Group workingProcesses;

void** allocatedMemory;
int numberOfMemoryAllocations;

//struct partition * partitionArray;

typedef enum
{
    NW_UPDATE,
    N_UPDATE,
    NE_UPDATE,
    W_UPDATE,
    E_UPDATE,
    SW_UPDATE,
    S_UPDATE,
    SE_UPDATE,
    BOUNDS_MESSAGE,
    GENERATION_MESSAGE,
    NEIGHBORLIST_MESSAGE,
    BOARD_MESSAGE,
    PARTITION_MESSAGE
} tagType;

bool isAlive(int x, int y); //Prototype
void swapBoards();

//Treats the local board as if it's a two dimensional array
char getArray(int x, int y)
{
    return localBoard[x + (y * (myCoords.lengthX + 2))];
}

void setNextArray(int x, int y, int value)
{
    nextGenBoard[x + (y * (myCoords.lengthX + 2))] = value;
}

/* Frees all known allocated memory */
void freeMemory()
{
    for(int i = 0; i < numberOfMemoryAllocations; i++)
        free(allocatedMemory[i]);
}

/* Parses the input file character by character. Assumes proper file format of ITERATIONS\bCOLUMNS\bROWS\bARRAY_STUFF. Sets the resulting data to *currentGenBoard */
void parseFile(int argc, char ** argv)
{

    int numberOfProcesses;

    //Keeps track of the total number of *s and .s in the file
    int counter;

    //Keeps track of the current character being read from the file
    char currentChar;

    MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);
    counter = 0;

    FILE * filePtr = fopen(argv[1], "r");

    //Breaks if the user pointed to a file that doesn't exist
    if(filePtr == NULL)
    {
        printf("Could not find file! Please restart and retry.");
        exit(1);
    }

    //Reads in the generations, rows, and columns to the global variables
    fscanf(filePtr, "%d", &numberOfGenerations);
    fscanf(filePtr, "%d", &masterBoard_columns);
    fscanf(filePtr, "%d", &masterBoard_rows);

    actualPartitions = numberOfProcesses;

    //Allocates some memory based on the number of rows and columns found in the input file
    masterBoard = malloc(masterBoard_rows * masterBoard_columns * sizeof(char));

    //Cycles through the contents of the file character by character. Breaks when the assumed total number of symbols has been found
    while(counter < (masterBoard_rows * masterBoard_columns))
    {
        //Checks to see if there aren't enough relevant characters in the file
        if(fscanf(filePtr, "%c", &currentChar) == EOF)
        {
            printf("Your file specification's jacked up, might want to check it out.");
            exit(1);
        }

        if(currentChar == 42)   //Writes the character "1" if the value of the current character is a "*"
        {
            masterBoard[counter] = 1;
            counter++;
        }
        else if(currentChar == 46)   //Writes the character "0" if the value of the current character is a "."
        {
            masterBoard[counter] = 0;
            counter++;
        }
    }

    fclose(filePtr);
}

/* Called when the final board configurations have been calculated, all processes submit their sections for gather */
void finalizeBoard()
{
    int size;

    if(!identity)
    {
        char* incomingBoard;

        for(int i = 0; i < actualPartitions; i++)
        {
            size = (partitionArray[i].lengthX + 2) * (partitionArray[i].lengthY + 2);
            incomingBoard = malloc(sizeof(char) * size);
            int masterArrayPosition = partitionArray[i].lengthX + partitionArray[i].startY * masterBoard_columns;

            MPI_Recv(incomingBoard, size, MPI_CHAR, i, BOARD_MESSAGE, MPI_COMM_WORLD, &lastStatus);    //UPDATED COMM

            for(int ip = 0; ip < size; ip++)
            {
                if(incomingBoard[ip] == 1)
                    printf("*");
                else
                    printf(".");
                if((ip + 1) % (partitionArray[i].lengthX+2) == 0)
                    printf("\n");
            }

            for(int k = 0; k < partitionArray[i].lengthY; k++)
            {
                for(int j = 0; j < partitionArray[i].lengthX; j++)
                {
                    int clientEquivLocation = ( (k+1) * (partitionArray[i].lengthX+2) + (j+1) );

                    masterBoard[ j + partitionArray[i].startX + ( ( k+partitionArray[i].startY ) * masterBoard_columns )] = incomingBoard[clientEquivLocation];
                }
            }

            free(incomingBoard);
        }

        printf("Final board configuration: \n");

        for(int i = 0; i < masterBoard_columns * masterBoard_rows; i++)
        {
            if(masterBoard[i] == 1)
                printf("*");
            else
                printf(".");
            if((i + 1) % (masterBoard_columns) == 0)
                printf("\n");
        }
        MPI_Barrier(MPI_COMM_WORLD);   //UPDATED COMM
    }
    else
    {
        MPI_Barrier(MPI_COMM_WORLD);   //UPDATED COMM
    }

    freeMemory();   //Frees all malloc'd memory

    MPI_Finalize();
}

void initializeBoard(int argc, char ** argv)
{
    int * curLoopNeighbors;
    int curLoopBoard_Size;
    char * curLoopBoard;
    int l;

    parseFile(argc, argv);

    partitionArray = generateBoard(masterBoard_columns, masterBoard_rows, &actualPartitions);

    numberOfMemoryAllocations = actualPartitions;
    allocatedMemory = malloc(sizeof(char*) * (numberOfMemoryAllocations));

    for(int i = 0; i < actualPartitions; i++)
    {
        curLoopNeighbors = neighborList(i);//Find the neighbors of the tile
        MPI_Isend(&partitionArray[i], sizeof(struct partition), MPI_BYTE, i, BOUNDS_MESSAGE, MPI_COMM_WORLD, &lastRequest);
        MPI_Isend(&numberOfGenerations, 1, MPI_INT, i, GENERATION_MESSAGE, MPI_COMM_WORLD ,&lastRequest);
        MPI_Isend(curLoopNeighbors, 8, MPI_INT, i, NEIGHBORLIST_MESSAGE, MPI_COMM_WORLD, &lastRequest);

        curLoopBoard_Size = (partitionArray[i].lengthX+2)*(partitionArray[i].lengthY+2);
        curLoopBoard = malloc(sizeof(char) *curLoopBoard_Size);

        l = 0;//position in the board to be sent to clients

        for(int k = partitionArray[i].startY - 1; k <= (partitionArray[i].startY + partitionArray[i].lengthY); k++)
        {
            for(int j = partitionArray[i].startX - 1; j <= (partitionArray[i].startX + partitionArray[i].lengthX); j++)
            {
                if((k < 0) || (j < 0) || (k >= masterBoard_rows) || (j >= masterBoard_columns))
                    curLoopBoard[l++] = 0;
                else
                    curLoopBoard[l++] = masterBoard[j + (k * masterBoard_columns)];
            }
        }

        allocatedMemory[i] = (void*)curLoopBoard;

        MPI_Isend(curLoopBoard, curLoopBoard_Size, MPI_CHAR, i, BOARD_MESSAGE, MPI_COMM_WORLD, &lastRequest);
    }

    printf("Initial board: \n\n");

    for(int i = 0; i < masterBoard_columns * masterBoard_rows; i++)
    {
        if(masterBoard[i] == 1)
            printf("*");
        else
            printf(".");
        if((i + 1) % (masterBoard_columns) == 0)
            printf("\n");
    }
    for(int i = 0; i < actualPartitions; i++)
        MPI_Isend(&actualPartitions, 1, MPI_INT, i, PARTITION_MESSAGE, MPI_COMM_WORLD, &lastRequest);
}

/* Updates the board */
void calculateBoard()
{
    char * sendEdge;
    int size;
    int tag;

    char * recvEdge;
    int currentNeighbor;

    while(numberOfGenerations-- > 0)
    {
        /* Send */
        if(identity < actualPartitions)
        {
            for(int j = 0; j < 8; j++)
            {
                if(myNeighborIDs[j] > -1)
                {
                    switch(j)
                    {
                    case 0:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[myCoords.lengthX + 3];
                        size = 1;
                        tag = SE_UPDATE;
                        break;
                    case 1:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthX);
                        for(int k = 0; k < myCoords.lengthX; k++)
                            sendEdge[k] = localBoard[myCoords.lengthX + 3 + k];
                        size = myCoords.lengthX;
                        tag = S_UPDATE;
                        break;
                    case 2:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[myCoords.lengthX + 2 + myCoords.lengthX];
                        size = 1;
                        tag = SW_UPDATE;
                        break;
                    case 3:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthY);
                        for(int k = 0; k < myCoords.lengthY; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 3) + k * (myCoords.lengthX + 2)];
                        size = myCoords.lengthY;
                        tag = E_UPDATE;
                        break;
                    case 4:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthY);
                        for(int k = 0; k < myCoords.lengthY; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 2 + myCoords.lengthX) + k * (myCoords.lengthX + 2)];
                        size = myCoords.lengthY;
                        tag = W_UPDATE;
                        break;
                    case 5:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[(myCoords.lengthX + 2) * myCoords.lengthY + 1];
                        size = 1;
                        tag = NE_UPDATE;
                        break;
                    case 6:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthX);
                        for(int k = 0; k < myCoords.lengthX; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 2) * myCoords.lengthY + 1 + k];
                        size = myCoords.lengthX;
                        tag = N_UPDATE;
                        break;
                    case 7:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[(myCoords.lengthX + 2) * myCoords.lengthY + myCoords.lengthX];
                        size = 1;
                        tag = NW_UPDATE;
                        break;
                    }

                    allocatedMemory[numberOfMemoryAllocations++] = (void*)sendEdge;

                    MPI_Isend(sendEdge, size, MPI_CHAR, myNeighborIDs[j], tag, MPI_COMM_WORLD, &lastRequest);
                }
            }

            /* Receive */
            for(int j = 0; j < 8; j++)
            {
                currentNeighbor = myNeighborIDs[j];

                //Sets up the buffers and information that is going to be received
                if(currentNeighbor >= 0)
                {
                    switch(j)
                    {
                    case 0:
                        recvEdge = malloc(sizeof(char) * 1);
                        size = 1;
                        tag = NW_UPDATE;
                        break;
                    case 1:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthX);
                        size = myCoords.lengthX;
                        tag = N_UPDATE;
                        break;
                    case 2:
                        recvEdge = malloc(sizeof(char) * 1);
                        size = 1;
                        tag = NE_UPDATE;
                        break;
                    case 3:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthY);
                        size = myCoords.lengthY;
                        tag = W_UPDATE;
                        break;
                    case 4:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthY);
                        size = myCoords.lengthY;
                        tag = E_UPDATE;
                        break;
                    case 5:
                        recvEdge = malloc(sizeof(char) * 1);
                        size = 1;
                        tag = SW_UPDATE;
                        break;
                    case 6:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthX);
                        size = myCoords.lengthX;
                        tag = S_UPDATE;
                        break;
                    case 7:
                        recvEdge = malloc(sizeof(char) * 1);
                        size = 1;
                        tag = SE_UPDATE;
                        break;
                    }

                    MPI_Recv(recvEdge, size, MPI_CHAR, currentNeighbor, tag, MPI_COMM_WORLD, &lastStatus);

                    //Figures out what to do with the information
                    switch(tag)
                    {
                    case NW_UPDATE:
                        localBoard[0] = recvEdge[0];
                        break;
                    case N_UPDATE:
                        for(int i = 0; i < myCoords.lengthX; i++)
                            localBoard[1 + i] = recvEdge[i];
                        break;
                    case NE_UPDATE:
                        localBoard[myCoords.lengthX + 1] = recvEdge[0];
                        break;
                    case W_UPDATE:
                        for(int i = 0; i < myCoords.lengthY; i++)
                            localBoard[myCoords.lengthX + 2 + (i * myCoords.lengthX + 2)] = recvEdge[i];
                        break;
                    case E_UPDATE:
                        for(int i = 0; i < myCoords.lengthY; i++)
                            localBoard[myCoords.lengthX + 3 + myCoords.lengthX + (i * myCoords.lengthX + 2)] = recvEdge[i];
                        break;
                    case SW_UPDATE:
                        localBoard[(myCoords.lengthX + 2) * (myCoords.lengthY + 1)] = recvEdge[0];
                        break;
                    case S_UPDATE:
                        for(int i = 0; i < myCoords.lengthX; i++)
                            localBoard[(myCoords.lengthX + 2) * (myCoords.lengthY + 1) + 1 + i] = recvEdge[i];
                        break;
                    case SE_UPDATE:
                        localBoard[(myCoords.lengthX + 2) * (myCoords.lengthY + 2) - 1] = recvEdge[0];
                        break;
                    }

                    free(recvEdge);
                }
            }

            /* Does actual liveliness calculations */
            for(int j = 0; j < (myCoords.lengthY + 2); j++)
            {
                for(int i = 0; i < (myCoords.lengthX + 2); i++)
                {
                    if(isAlive(i, j))
                        setNextArray(i, j, 1);
                    else
                        setNextArray(i, j, 0);
                }
            }

            swapBoards();
        }

        MPI_Barrier(MPI_COMM_WORLD);   //Once done, chill out till everyon else is done.   //UPDATED COMM
    }

    MPI_Barrier(MPI_COMM_WORLD); //UPDATED COMM

    if(identity < actualPartitions)
        MPI_Isend(localBoard, (myCoords.lengthX + 2) * (myCoords.lengthY + 2), MPI_CHAR, 0, BOARD_MESSAGE, MPI_COMM_WORLD, &lastRequest);  //MPI_COMM

    MPI_Barrier(MPI_COMM_WORLD); //UPDATED COMM
}

/* Determines if some cell is alive or dead in the next board generation */
bool isAlive(int x, int y)
{
    bool alive;
    int numNeighbors;

    alive = false;

    numNeighbors = 0;

    for(int j = -1; j <= 1; j++)
        for(int i = -1; i <= 1; i++)
        {
            if(!(((x + i) < 0) || ((x + i) >= (myCoords.lengthX + 2)) || ((y + j) < 0) || ((y + j) >= (myCoords.lengthY + 2)) || (i == 0 && j == 0)))
                numNeighbors += getArray(x + i, y + j);
        }

    //Only two cases in which a cell will live
    if((getArray(x,y) == 1) && (numNeighbors == 2 || numNeighbors == 3))
        alive = true;
    else if((getArray(x,y) == 0) && (numNeighbors == 3))
        alive = true;

    return alive;
}

//Swaps the current gen board with the next gen board to save from mallocing tons of boards
void swapBoards(void)
{
    char* tempBoard;

    tempBoard = localBoard;
    localBoard = nextGenBoard;
    nextGenBoard = tempBoard;
}

void initMPI(int argc, char ** argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &identity);

    numberOfMemoryAllocations = 0;

    if(!identity)
    {
        isActiveProcess = true;
        MPI_Comm_size(MPI_COMM_WORLD, &actualPartitions);
        initializeBoard(argc, argv);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Recv(&actualPartitions, 1, MPI_INT, 0, PARTITION_MESSAGE, MPI_COMM_WORLD, &lastStatus);

    if(identity < actualPartitions)
    {
        MPI_Recv(&myCoords, sizeof(struct partition), MPI_BYTE, 0, BOUNDS_MESSAGE, MPI_COMM_WORLD, &lastStatus);
        MPI_Recv(&numberOfGenerations, 1, MPI_INT, 0, GENERATION_MESSAGE, MPI_COMM_WORLD, &lastStatus);
        MPI_Recv(myNeighborIDs, 8, MPI_INT, 0, NEIGHBORLIST_MESSAGE, MPI_COMM_WORLD, &lastStatus);
        localBoard_Size = (myCoords.lengthX + 2) * (myCoords.lengthY + 2);
        localBoard = malloc(sizeof(char) * localBoard_Size);
        MPI_Recv(localBoard, localBoard_Size, MPI_CHAR, 0, BOARD_MESSAGE,MPI_COMM_WORLD, &lastStatus);
        nextGenBoard = malloc(sizeof(char) * localBoard_Size);

    }
    //Non-working processes do nothing.
}

void main(int argc, char ** argv)
{
    initMPI(argc, argv);

    calculateBoard();

    finalizeBoard();

}

