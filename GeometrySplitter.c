#include "GeometrySplitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
//#include <conio.h>
//#include "ezdib.h"

int widthX; //Width of board
int widthY; //Length of board

int smallestSide;   //Smallest side of the board
int largestSide;    //Largest side of the board (not used)

int numXDivisions;  //Number of partitions along the x axis (typical case)
int numYDivisions;  //Number of partitions along the y axis (typical case)

int extraPartitions;    //Number of extra partitions that need to be dealt with after factoring

bool isSmallestSideVertical;    //Which axis is the smallest side

int numberOfPartitions; //Number of actual partitions

struct partition * partitions;    //Holds information about completed partition layout

void compareSides(void);    //Function prototyping

/* Given two factors, determines how to divide the board by them */
void setDivisions(int factor1, int factor2)
{
    bool inPlace;
    int tempFactor;

    //If one side evenly divides, that's a good thing!
    if(((largestSide % factor1 == 0) || (smallestSide % factor2 == 0)) && (factor1 <= largestSide && factor2 <= smallestSide))
        inPlace = true;
    else if(((smallestSide % factor1 == 0) || (largestSide % factor2 == 0)) && (factor1 <= smallestSide && factor2 <= largestSide))
        inPlace = false;
    else
        inPlace = true;

    //Swaps factors around in the case of a bad initial assignment
    if((isSmallestSideVertical && !inPlace) || (!isSmallestSideVertical && inPlace))
    {
        tempFactor = factor1;
        factor1 = factor2;
        factor2 = tempFactor;
    }

    numXDivisions = factor1;
    numYDivisions = factor2;
}

/* Given the number of partitions, determines the two closest factors after partition size is (potentially) massaged upwards to create a nicely divisible partition number */
void determineFactors(void)
{
    int i;
    int root;
    int total;
    int highestFactor;
    int otherFactor;

    total = numberOfPartitions + extraPartitions;

    root = sqrt(total);

    for(i = 1; i <= root; i++)
        if(total % i == 0)
            highestFactor = i;

    otherFactor = total / highestFactor;

    //If these factors are unable to map to the sides
    if((highestFactor > largestSide || otherFactor > smallestSide) && (highestFactor > smallestSide || otherFactor > largestSide))
    {
        if(smallestSide > total/smallestSide)
            setDivisions(smallestSide, total/smallestSide);
        else
            setDivisions(total/smallestSide, smallestSide);
    }
    else if(otherFactor > numberOfPartitions)
    {
        setDivisions(numberOfPartitions, 1);
    }
    else
        setDivisions(highestFactor, otherFactor);
}

/* Allocates memory for the board array */
void allocatepartitions()
{
    partitions = malloc(sizeof(struct partition) * numberOfPartitions);
}

/* Frees allocated memory */
void deallocatepartitions()
{
    free(partitions);
}

/* Given some x coordinate (at the end of the array), determines the amount of space in the y direction that needs to be filled */
int findMissingYSpace(int row)
{
    int i;
    int result;

    result = 0;

    for(i = 0; (row - numXDivisions * i) >= 0; i++)
    {
        //printf("Row * i: %d, Value: %d\n", row - numXDivisions * i, partitions[row - numXDivisions * i].lengthY);
        result += partitions[row - numXDivisions * i].lengthY;
    }

    result = widthY - result;

    return result;
}

int findNumberOfCellsInRow(int row)
{
    int i;
    int result;

    result = 0;

    for(i = 0; (row - numXDivisions * i) >= 0; i++)
    {
        result++;
    }

    return result;
}

/* Expands uniformly created cells to fit the board configuration */
void massageCells()
{
    int offset;
    int totalNumber;
    int extraYSpace;
    int i;
    int j;
    int startingPoint;
    int tempSpace;
    int massageSpace;
    int numberOfCells;
    int oddCells;
    int normalSubtractor;


    offset = numberOfPartitions % numXDivisions;
    totalNumber = numXDivisions - offset;

    ///*
    for(i = 0; i < numXDivisions; i++)
    {
        startingPoint = numberOfPartitions - 1 - i;

        extraYSpace = findMissingYSpace(startingPoint);
        tempSpace = extraYSpace;

        numberOfCells = findNumberOfCellsInRow(startingPoint);

        massageSpace = (extraYSpace / numberOfCells) + 1;

        oddCells = numberOfCells + (extraYSpace - massageSpace * numberOfCells);

        normalSubtractor = massageSpace - 1;

        /*
        printf("Number Of Cells: %d\n", numberOfCells);
        printf("Odd number of cells: %d\n", oddCells);
        printf("Normal Subtractor: %d\n", normalSubtractor);
        printf("Extra Subtractor: %d\n", massageSpace);
        */

        for(j = 0; j < numberOfCells; j++)
        {
            if(j < oddCells)
            {
                tempSpace -= massageSpace;
                partitions[startingPoint - numXDivisions * j].startY += tempSpace;
                partitions[startingPoint - numXDivisions * j].lengthY += massageSpace;
            }
            else
            {
                tempSpace -= normalSubtractor;
                partitions[startingPoint - numXDivisions * j].startY += tempSpace;
                partitions[startingPoint - numXDivisions * j].lengthY += normalSubtractor;
            }
        }
    }
    //*/


    //printf("Offset: %d\n", offset);

    /*
    for(i = 0; i < numXDivisions; i++)
        partitions[numberOfPartitions - 1 - i].lengthY += findMissingYSpace(numberOfPartitions - 1 - i);
    */


}

/* Determines board geometry */
void splitGeometry(void)
{
    int i;
    int xLength;
    int yLength;
    int extraX;
    int extraY;
    int tempExtraX;
    int totalX;

    allocatepartitions();

    compareSides();

    extraPartitions = 0; //Range of 0 - (smallestSide - 1)

    if(numberOfPartitions > largestSide)
    {
        while((numberOfPartitions % smallestSide != 0) && (numberOfPartitions % largestSide != 0))
            numberOfPartitions--;
    }

    determineFactors();

    xLength = widthX / numXDivisions;
    yLength = widthY / numYDivisions;

    extraX = (widthX % numXDivisions);
    extraY = (widthY % numYDivisions);

    tempExtraX = extraX;

    /*
    printf("Number of xDivisions: %d of width: %d\n", numXDivisions, xLength);
    printf("Number of yDivisions: %d of width: %d\n", numYDivisions, yLength);
    printf("Extra partitions: %d\n", extraPartitions);
    */

    totalX = 0;

    for(i = 0; i < numberOfPartitions ; i++)
    {
        if(i == 0)
        {
            partitions[i].startX = 0;
            partitions[i].startY = 0;

        }
        else if(totalX % widthX == 0)
        {
            tempExtraX = extraX;
            partitions[i].startX = 0;
            partitions[i].startY = partitions[i-1].lengthY + partitions[i-1].startY;

            if(extraY > 0)
            {
                //partitions[i].startY += 1;
                extraY--;
            }
        }
        else
        {
            partitions[i].startX = partitions[i-1].startX + partitions[i-1].lengthX;
            partitions[i].startY = partitions[i-1].startY;
        }

        totalX += xLength;


        partitions[i].lengthX = xLength;
        partitions[i].lengthY = yLength;

        if(extraY > 0)
            partitions[i].lengthY += 1;

        if(tempExtraX > 0)
        {
            partitions[i].lengthX += 1;
            totalX++;
            tempExtraX--;
        }
    }

    //massageCells();   //No longer needed :(

}

/* Prints out the array */
/*
void printArray(void)
{
    int i;
    HEZDIMAGE image;
    char * fileString;

    fileString = malloc(sizeof(char) * 40);

    sprintf(fileString, "%d %d %d - PartitionOutput.bmp", widthX, widthY, numberOfPartitions);

    for(i = 0; i < numberOfPartitions; i++)
    {
        //printf("%d: Upper corner: %d,%d Lower corner: %d,%d\n", i, partitions[i].startX, partitions[i].startY, partitions[i].startX + partitions[i].lengthX, partitions[i].startY + partitions[i].lengthY);
    }

    image = ezd_create(640, -480, 24, 0);

    ezd_fill(image, 0x606060);

    for(i = 0; i < numberOfPartitions; i++)
    {
        ezd_rect(image, partitions[i].startX * 20, partitions[i].startY * 20, (partitions[i].startX + partitions[i].lengthX) * 20, (partitions[i].startY + partitions[i].lengthY) * 20, 0x00ff00);
    }

    ezd_save(image, fileString);

    free(fileString);

    ezd_destroy(image);

    //getch();
    //closegraph();

}
*/


/* Determines the smallest side */
void compareSides(void)
{
    int smallResult;
    int largeResult;

    if(widthX < widthY)
    {
        smallResult = widthX;
        largeResult = widthY;
        isSmallestSideVertical = false;
    }
    else
    {
        smallResult = widthY;
        largeResult = widthX;
        isSmallestSideVertical = true;
    }

    smallestSide = smallResult;
    largestSide = largeResult;
}

/* Stolen from Stack-Overflow, should provide evenly distributed random numbers between MIN and MAX */
int random_in_range (unsigned int min, unsigned int max)
{
    int base_random = rand(); /* in [0, RAND_MAX] */
    if (RAND_MAX == base_random) return random_in_range(min, max);
    /* now guaranteed to be in [0, RAND_MAX) */
    int range       = max - min,
                      remainder   = RAND_MAX % range,
                                    bucket      = RAND_MAX / range;
    /* There are range buckets, plus one smaller interval
       within remainder of RAND_MAX */
    if (base_random < RAND_MAX - remainder)
    {
        return min + base_random/bucket;
    }
    else
    {
        return random_in_range (min, max);
    }
}

/*
void generateRandomGrids()
{
    int i;

    for(i = 0; i < 100; i++)
    {
        widthX = random_in_range(1, 32);
        widthY = random_in_range(1, 24);
        numberOfPartitions = random_in_range(1, widthX * widthY);

        compareSides();
        splitGeometry();
        printArray();
        deallocatepartitions();
    }


}
*/

/* Determines the neighbors of a given partition, returned in format NW N NE W E SW S SE in an int array of size 8 */
int * neighborList(int partitionNumber)
{
    int * list;
    int positionX;
    int positionY;
    int currentPosition;

    currentPosition = 0;

    list = malloc(sizeof(int) * 8);

    positionX = partitionNumber % numXDivisions;
    positionY = partitionNumber / numXDivisions;

    for(int j = -1; j <= 1; j++)
        for(int i = -1; i <= 1; i++)
        {
            if(i != 0 || j != 0)
            {
                if((positionX + i < 0) || (positionX + i >= numXDivisions) || (positionY + j < 0) || (positionY + j >= numYDivisions))
                    list[currentPosition] = -1;
                else
                    list[currentPosition] = positionX + i + ((positionY + j) * numXDivisions);

                currentPosition++;
            }
        }

    //printf("%d: posX: %d posY: %d ", partitionNumber, positionX, positionY);

    //for(int i = 0; i < 8; i++)
    //{
    //    printf("%d ", list[i]);
    //}

    //printf("\n");

    return list;
}

/* Partitions the board and returns a struct array of the partitions. Modifies processes as it sees fit */
struct partition *generateBoard(int width, int length, int *processes)
{
    widthX = width;
    widthY = length;
    if(*processes > (width * length))
        *processes = width * length;
    numberOfPartitions = *processes;

    compareSides();

    splitGeometry();

    *processes = numberOfPartitions;

    return partitions;
}

/*
void main(void)
{

    widthX = 3;
    widthY = 3;
    numberOfPartitions = 9;

    compareSides();

    splitGeometry();

    //printArray();

    printf("Number of partitions: %d\n", numberOfPartitions);

    for(int i = 0; i < numberOfPartitions; i++)
    {
        neighborList(i);
    }

    deallocatepartitions();


    //generateRandomGrids();
}
*/


