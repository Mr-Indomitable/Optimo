#ifndef GRID_H
#define GRID_H

#include <iostream>
#include <cstring>
using namespace std;

const int MAX_SIZE = 20;
const int MAX_FURNITURE = 20;

// Public places (restaurant, library) may hold many tables. Gaps are measured
// in empty cells between two items.
//   Table to table: chair (1) + pull-out space (1) on each side = 4
//   Library: shelf to shelf / shelf to table = a 2 cell aisle
const int MIN_TABLE_GAP = 4;
const int LIBRARY_AISLE = 2;

class Furniture
{
public:
    char name[20];
    char symbol;
    int width;
    int height;

    Furniture()
    {
        strcpy(name, "");
        symbol = '?';
        width = 0;
        height = 0;
    }
};

class Room
{
public:
    int width;
    int height;
    char space[MAX_SIZE][MAX_SIZE];

    // Openings on each wall: ' ' = plain wall, 'D' = door, 'W' = window
    char topWall[MAX_SIZE];     // indexed by column
    char bottomWall[MAX_SIZE];  // indexed by column
    char leftWall[MAX_SIZE];    // indexed by row
    char rightWall[MAX_SIZE];   // indexed by row

    // Cells in front of a door (2 deep) - kept free as walking space
    bool blocked[MAX_SIZE][MAX_SIZE];

    // Room type: 0 = general, 1 = bedroom, 2 = drawing room, 3 = study,
    //            4 = dining room, 5 = restaurant, 6 = library
    int type;

    Room(int w, int h)
    {
        width = w;
        height = h;
        type = 0;
        createEmptyGrid();
    }

    const char* typeName()
    {
        if (type == 1) return "Bedroom";
        if (type == 2) return "Drawing room";
        if (type == 3) return "Study";
        if (type == 4) return "Dining room";
        if (type == 5) return "Restaurant";
        if (type == 6) return "Library";
        return "General room";
    }

    // Restaurants and libraries may have many tables
    bool isPublic() { return type == 5 || type == 6; }

    int cols() { return width; }
    int rows() { return height; }

    void createEmptyGrid()
    {
        for (int i = 0; i < MAX_SIZE; i++)
        {
            topWall[i] = ' ';
            bottomWall[i] = ' ';
            leftWall[i] = ' ';
            rightWall[i] = ' ';

            for (int j = 0; j < MAX_SIZE; j++)
            {
                space[i][j] = '.';
                blocked[i][j] = false;
            }
        }
    }

    // type: 'D' (door) or 'W' (window)
    // wall: 'T' top, 'B' bottom, 'L' left, 'R' right
    // start: 1-based cell number along that wall, length: number of cells
    bool addOpening(char type, char wall, int start, int length)
    {
        if (wall >= 'a' && wall <= 'z')
            wall = wall - 'a' + 'A';

        if (type != 'D' && type != 'W')
            return false;

        char* arr;
        int limit;

        if (wall == 'T')      { arr = topWall;    limit = cols(); }
        else if (wall == 'B') { arr = bottomWall; limit = cols(); }
        else if (wall == 'L') { arr = leftWall;   limit = rows(); }
        else if (wall == 'R') { arr = rightWall;  limit = rows(); }
        else return false;

        int s = start - 1;

        if (s < 0 || length <= 0 || s + length > limit)
            return false;

        // Do not overlap an existing door/window
        for (int k = s; k < s + length; k++)
        {
            if (arr[k] != ' ')
                return false;
        }

        for (int k = s; k < s + length; k++)
        {
            arr[k] = type;

            if (type == 'D')
            {
                // Keep the 2 cells in front of the door free (walking space)
                if (wall == 'T')
                {
                    blocked[0][k] = true;
                    if (rows() > 1) blocked[1][k] = true;
                }
                else if (wall == 'B')
                {
                    blocked[rows() - 1][k] = true;
                    if (rows() > 1) blocked[rows() - 2][k] = true;
                }
                else if (wall == 'L')
                {
                    blocked[k][0] = true;
                    if (cols() > 1) blocked[k][1] = true;
                }
                else
                {
                    blocked[k][cols() - 1] = true;
                    if (cols() > 1) blocked[k][cols() - 2] = true;
                }
            }
        }

        return true;
    }

    bool canPlace(Furniture furniture, int x, int y)
    {
        if (x < 0 || y < 0)
            return false;

        if (x + furniture.width > width)
            return false;

        if (y + furniture.height > height)
            return false;

        for (int i = y; i < y + furniture.height; i++)
        {
            for (int j = x; j < x + furniture.width; j++)
            {
                if (space[i][j] != '.')
                    return false;

                if (blocked[i][j])
                    return false;
            }
        }

        return true;
    }

    void placeFurniture(Furniture furniture, int x, int y)
    {
        for (int i = y; i < y + furniture.height; i++)
        {
            for (int j = x; j < x + furniture.width; j++)
            {
                space[i][j] = furniture.symbol;
            }
        }
    }

    void removeFurniture(Furniture furniture, int x, int y)
    {
        for (int i = y; i < y + furniture.height; i++)
        {
            for (int j = x; j < x + furniture.width; j++)
            {
                space[i][j] = '.';
            }
        }
    }

    // Horizontal wall segment (3 chars wide)
    const char* horizontalSegment(char opening)
    {
        if (opening == 'D') return " D ";
        if (opening == 'W') return "== ";
        return "---";
    }

    // Vertical wall character
    char verticalChar(char opening)
    {
        if (opening == 'D') return 'D';
        if (opening == 'W') return '=';
        return '|';
    }

    void display()
    {
        cout << endl;

        int c = cols();
        int r = rows();

        for (int i = 0; i < r; i++)
        {
            // Horizontal line above this row
            for (int j = 0; j < c; j++)
            {
                cout << ".";

                if (i == 0)
                    cout << horizontalSegment(topWall[j]);
                else
                    cout << "---";
            }
            cout << "." << endl;

            // Row contents
            for (int j = 0; j < c; j++)
            {
                if (j == 0)
                    cout << verticalChar(leftWall[i]);
                else
                    cout << "|";

                cout << " ";

                if (space[i][j] != '.')
                    cout << space[i][j];
                else
                    cout << " ";

                cout << " ";
            }

            cout << verticalChar(rightWall[i]) << endl;
        }

        // Bottom wall
        for (int j = 0; j < c; j++)
        {
            cout << "." << horizontalSegment(bottomWall[j]);
        }

        cout << "." << endl;

        cout << "\nLegend: D = door, == = window\n";
    }
};

#endif