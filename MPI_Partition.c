#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>

#include "GeometrySplitter.h"

//Project 3
//Christopher Parish and Eli Pinkerton
//MPI version of game of life FTW
//Please read the README for instructions on usage

int identity;
int actualPartitions;
struct partition *partitionArray;
char* localBoard;
char* nextGenBoard;
char* masterBoard;

//Used for masterBoard specifications
int masterBoard_columns;
int masterBoard_rows;

//Used for each process to determine the size of their piece of board
struct partition myCoords;

int numberOfGenerations;
int myNeighborIDs[8];
MPI_Status lastStatus; //Handles to async call status messages
MPI_Request lastRequest;

int localBoard_Size;

char** allocatedMemory; //Array that collects pointers to memory to be free after each generation
int numberOfMemoryAllocations;

//Message types
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

bool isAlive(int x, int y); //Prototypes
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

    if(!identity) //If we are the master (process 0)
    {
        char* incomingBoard;

        for(int i = 0; i < actualPartitions; i++)//For each slave process
        {
            size = (partitionArray[i].lengthX + 2) * (partitionArray[i].lengthY + 2);
            incomingBoard = malloc(sizeof(char) * size);

            MPI_Recv(incomingBoard, size, MPI_CHAR, i, BOARD_MESSAGE, MPI_COMM_WORLD, &lastStatus);//Recieve their board

            for(int k = 0; k < partitionArray[i].lengthY; k++)
            {
                for(int j = 0; j < partitionArray[i].lengthX; j++)
                {
                    int clientEquivLocation = ( (k+1) * (partitionArray[i].lengthX+2) + (j+1) ); //Compute the location in the slave board we are interested in

                    masterBoard[ j + partitionArray[i].startX + ( ( k+partitionArray[i].startY ) * masterBoard_columns )] = incomingBoard[clientEquivLocation]; //compute and store that slave location in the appropriate master board location
                }
            }

            free(incomingBoard);
        }

        printf("\nFinal board configuration: \n");

        for(int i = 0; i < masterBoard_columns * masterBoard_rows; i++)//Print out the board
        {
            if(masterBoard[i] == 1)
                printf("*");
            else
                printf(".");
            if((i + 1) % (masterBoard_columns) == 0)
                printf("\n");
        }

        freeMemory();

        MPI_Barrier(MPI_COMM_WORLD); //wait here until everyone is done
    }
    else
    {
        MPI_Barrier(MPI_COMM_WORLD); //If we are a slave, barrier until the master is done
    }


    MPI_Finalize();//Godbye world!
}

/* Initializes the board and commucates the important parts to slaves */
void initializeBoard(int argc, char ** argv)
{
    int * curLoopNeighbors;
    int curLoopBoard_Size;
    char * curLoopBoard;
    int l;
    int numberOfProcessors;

    parseFile(argc, argv);

    partitionArray = generateBoard(masterBoard_columns, masterBoard_rows, &actualPartitions); //Parse the file

    MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcessors);

    printf("Forcing %d partitions\n", actualPartitions);

    numberOfMemoryAllocations = actualPartitions;
    allocatedMemory = malloc(sizeof(char*) * (numberOfMemoryAllocations + 8));

    for(int i = 0; i < actualPartitions; i++)
    {
        curLoopNeighbors = neighborList(i);//Find the neighbors of the tile
        MPI_Isend(&partitionArray[i], sizeof(struct partition), MPI_BYTE, i, BOUNDS_MESSAGE, MPI_COMM_WORLD, &lastRequest); //Send the structure containing their coords
        MPI_Isend(curLoopNeighbors, 8, MPI_INT, i, NEIGHBORLIST_MESSAGE, MPI_COMM_WORLD, &lastRequest); //Send them their neighbor list so the slave knows who to contact

        curLoopBoard_Size = (partitionArray[i].lengthX+2)*(partitionArray[i].lengthY+2);
        curLoopBoard = malloc(sizeof(char) *curLoopBoard_Size);

        l = 0;//position in the board to be sent to clients

	//Build the slave board from the relevant positions in the master board
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

        allocatedMemory[i] = curLoopBoard;

        MPI_Isend(curLoopBoard, curLoopBoard_Size, MPI_CHAR, i, BOARD_MESSAGE, MPI_COMM_WORLD, &lastRequest);//Send the board
    }

    printf("\nInitial board: \n");

    for(int i = 0; i < masterBoard_columns * masterBoard_rows; i++)
    {
        if(masterBoard[i] == 1)
            printf("*");
        else
            printf(".");
        if((i + 1) % (masterBoard_columns) == 0)
            printf("\n");
    }

    //Send EVERYONE, even those with no work, the number of generations and processors so the barriers work
    for(int i = 0; i < numberOfProcessors; i++)
        MPI_Isend(&actualPartitions, 1, MPI_INT, i, PARTITION_MESSAGE, MPI_COMM_WORLD, &lastRequest);

    for(int i = 0; i < numberOfProcessors; i++)
        MPI_Isend(&numberOfGenerations, 1, MPI_INT, i, GENERATION_MESSAGE, MPI_COMM_WORLD ,&lastRequest);
}

/* Updates the board */
void calculateBoard()
{
    char *memoryArray[8];
    int memoryUsed;

    char * sendEdge;
    int sendSize;
    int recvSize;
    int tag;

    char * recvEdge;
    int currentNeighbor;

    while(numberOfGenerations-- > 0)
    {
        memoryUsed = 0;

        /* Send */
        if(identity < actualPartitions)//If we are a board doing work
        {
            for(int j = 0; j < 8; j++)
            {
                if(myNeighborIDs[j] > -1)
                {
		    //Do the calculations to find out what cells need to be transported to the neighbors
                    switch(j)
                    {
                    case 0://For Example
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[myCoords.lengthX + 3];//The NW cell in my array should be located from the over provisioned region
                        sendSize = 1;//It is of size one
                        tag = SE_UPDATE;//This is a SE update from my neighbor's perspective
                        break;
                    case 1:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthX);
                        for(int k = 0; k < myCoords.lengthX; k++)
                            sendEdge[k] = localBoard[myCoords.lengthX + 3 + k];
                        sendSize = myCoords.lengthX;
                        tag = S_UPDATE;
                        break;
                    case 2:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[myCoords.lengthX + 2 + myCoords.lengthX];
                        sendSize = 1;
                        tag = SW_UPDATE;
                        break;
                    case 3:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthY);
                        for(int k = 0; k < myCoords.lengthY; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 3) + k * (myCoords.lengthX + 2)];
                        sendSize = myCoords.lengthY;
                        tag = E_UPDATE;
                        break;
                    case 4:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthY);
                        for(int k = 0; k < myCoords.lengthY; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 2 + myCoords.lengthX) + k * (myCoords.lengthX + 2)];
                        sendSize = myCoords.lengthY;
                        tag = W_UPDATE;
                        break;
                    case 5:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[((myCoords.lengthX + 2) * myCoords.lengthY) + 1];
                        sendSize = 1;
                        tag = NE_UPDATE;
                        break;
                    case 6:
                        sendEdge = malloc(sizeof(char) * myCoords.lengthX);
                        for(int k = 0; k < myCoords.lengthX; k++)
                            sendEdge[k] = localBoard[(myCoords.lengthX + 2) * myCoords.lengthY + 1 + k];
                        sendSize = myCoords.lengthX;
                        tag = N_UPDATE;
                        break;
                    case 7:
                        sendEdge = malloc(sizeof(char) * 1);
                        sendEdge[0] = localBoard[(myCoords.lengthX + 2) * myCoords.lengthY + myCoords.lengthX];
                        sendSize = 1;
                        tag = NW_UPDATE;
                        break;
                    }

                    memoryArray[memoryUsed++] = sendEdge;

                    MPI_Isend(sendEdge, sendSize, MPI_CHAR, myNeighborIDs[j], tag, MPI_COMM_WORLD, &lastRequest);//Send data
                }
            }

            /* Receive */
            for(int j = 0; j < 8; j++)
            {
                currentNeighbor = myNeighborIDs[j];

                //Sets up the buffers and information that is going to be received in a similar way but in the oppotite direction
                if(currentNeighbor > -1)
                {
                    switch(j)
                    {
                    case 0:
                        recvEdge = malloc(sizeof(char) * 1);
                        recvSize = 1;
                        tag = NW_UPDATE;
                        break;
                    case 1:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthX);
                        recvSize = myCoords.lengthX;
                        tag = N_UPDATE;
                        break;
                    case 2:
                        recvEdge = malloc(sizeof(char) * 1);
                        recvSize = 1;
                        tag = NE_UPDATE;
                        break;
                    case 3:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthY);
                        recvSize = myCoords.lengthY;
                        tag = W_UPDATE;
                        break;
                    case 4:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthY);
                        recvSize = myCoords.lengthY;
                        tag = E_UPDATE;
                        break;
                    case 5:
                        recvEdge = malloc(sizeof(char) * 1);
                        recvSize = 1;
                        tag = SW_UPDATE;
                        break;
                    case 6:
                        recvEdge = malloc(sizeof(char) * myCoords.lengthX);
                        recvSize = myCoords.lengthX;
                        tag = S_UPDATE;
                        break;
                    case 7:
                        recvEdge = malloc(sizeof(char) * 1);
                        recvSize = 1;
                        tag = SE_UPDATE;
                        break;
                    }

                    MPI_Recv(recvEdge, recvSize, MPI_CHAR, currentNeighbor, tag, MPI_COMM_WORLD, &lastStatus);//Sync recv

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
                            localBoard[myCoords.lengthX + 2 + (i * (myCoords.lengthX + 2))] = recvEdge[i];
                        break;
                    case E_UPDATE:
                        for(int i = 0; i < myCoords.lengthY; i++)
                            localBoard[myCoords.lengthX + 3 + myCoords.lengthX + (i * (myCoords.lengthX + 2))] = recvEdge[i];
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

            for(int i = 0; i < memoryUsed; i++)
                free(memoryArray[i]);
        }



        MPI_Barrier(MPI_COMM_WORLD);   //Once done, chill out till everyon else is done.
    }

    MPI_Barrier(MPI_COMM_WORLD); //Wait here after all generations are done

    if(identity < actualPartitions)
        MPI_Isend(localBoard, (myCoords.lengthX + 2) * (myCoords.lengthY + 2), MPI_CHAR, 0, BOARD_MESSAGE, MPI_COMM_WORLD, &lastRequest); //send the board back to the master

    MPI_Barrier(MPI_COMM_WORLD); //wait here until everybody sends their data
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
            if(!(((x + i) < 0) || ((x + i) >= (myCoords.lengthX + 2)) || ((y + j) < 0) || ((y + j) >= (myCoords.lengthY + 2)) || (i == 0 && j == 0)))//If the cell is in-bounds
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

    numberOfMemoryAllocations = 0;//Used for our garbage collection stuff

    if(!identity)//If we are the master
    {
        MPI_Comm_size(MPI_COMM_WORLD, &actualPartitions);
        initializeBoard(argc, argv);
    }
    else
        allocatedMemory = malloc(sizeof(char*) * 8);

    MPI_Barrier(MPI_COMM_WORLD);//Wait here for eveybody (specifically the master)

    MPI_Recv(&actualPartitions, 1, MPI_INT, 0, PARTITION_MESSAGE, MPI_COMM_WORLD, &lastStatus);//Get the previously send data about the number of partitions and generations

    MPI_Recv(&numberOfGenerations, 1, MPI_INT, 0, GENERATION_MESSAGE, MPI_COMM_WORLD, &lastStatus);

    if(identity < actualPartitions)//If we are a process with work to do
    {
        MPI_Recv(&myCoords, sizeof(struct partition), MPI_BYTE, 0, BOUNDS_MESSAGE, MPI_COMM_WORLD, &lastStatus);//Recv the info we will need to do the work
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

