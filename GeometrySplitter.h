#ifndef GEOMETRYSPLITTER_H_INCLUDED
#define GEOMETRYSPLITTER_H_INCLUDED

struct partition
{
    int startX;
    int startY;
    int lengthX;
    int lengthY;
} ;

struct partition * generateBoard(int width, int length, int* processes);

int * neighborList(int);

#endif // GEOMETRYSPLITTER_H_INCLUDED



