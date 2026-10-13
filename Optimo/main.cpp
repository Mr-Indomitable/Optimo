#include <iostream>
#include "grid.h"
#include "optimize.cpp"

using namespace std;

// Reads doors or windows from the user and adds them to the room
void readOpenings(Room& room, char type, const char* label)
{
    int count;

    cout << "\nEnter number of " << label << "s (0 for none): ";
    cin >> count;

    if (count < 0)
        count = 0;

    for (int i = 0; i < count; i++)
    {
        char wall;
        int start;
        int length;

        cout << "\n" << label << " " << i + 1 << endl;
        cout << "Which wall? (T = top, B = bottom, L = left, R = right): ";
        cin >> wall;

        cout << "Starting cell along that wall (1 to "
             << ((wall == 'T' || wall == 't' || wall == 'B' || wall == 'b')
                    ? room.cols() : room.rows())
             << "): ";
        cin >> start;

        cout << "Length in cells: ";
        cin >> length;

        if (!room.addOpening(type, wall, start, length))
        {
            cout << "Invalid or overlapping " << label
                 << ". Please enter it again.\n";
            i--;
        }
    }
}

int main()
{
    cout << "---- OPTIMO SPACE OPTIMIZATION ----\n";

    int roomWidth;
    int roomHeight;

    cout << "\nEnter room width: ";
    cin >> roomWidth;

    cout << "Enter room height: ";
    cin >> roomHeight;

    if (roomWidth <= 1 || roomHeight <= 1 ||
        roomWidth > MAX_SIZE || roomHeight > MAX_SIZE)
    {
        cout << "\nInvalid room size.\n";
        return 0;
    }

    Room room(roomWidth, roomHeight);

    // Doors and windows
    readOpenings(room, 'D', "door");
    readOpenings(room, 'W', "window");

    cout << "\n----- EMPTY ROOM -----\n";
    room.display();

    int numberOfFurniture;

    cout << "\nEnter number of furniture items: ";
    cin >> numberOfFurniture;

    if (numberOfFurniture <= 0 ||
        numberOfFurniture > MAX_FURNITURE)
    {
        cout << "\nInvalid number of furniture items.\n";
        return 0;
    }

    Optimizer optimizer(&room);

    for (int i = 0; i < numberOfFurniture; i++)
    {
        Furniture f;

        cout << "\nFurniture " << i + 1 << endl;

        cout << "Enter name: ";
        cin >> f.name;

        if (strcmp(f.name, "bed") == 0)
            f.symbol = 'B';
        else if (strcmp(f.name, "chair") == 0)
            f.symbol = 'C';
        else if (strcmp(f.name, "table") == 0)
            f.symbol = 'T';
        else if (strcmp(f.name, "wardrobe") == 0)
            f.symbol = 'W';
        else if (strcmp(f.name, "bookshelf") == 0)
            f.symbol = 'S';
        else
            f.symbol = 'F';

        cout << "Enter width: ";
        cin >> f.width;

        cout << "Enter height: ";
        cin >> f.height;

        if (f.width <= 0 || f.height <= 0 ||
            f.width > roomWidth ||
            f.height > roomHeight)
        {
            cout << "\nInvalid furniture size.\n";
            return 0;
        }

        optimizer.addFurniture(f);
    }

    cout << "\n----- FURNITURE ENTERED -----\n";

    for (int i = 0; i < optimizer.furnitureCount; i++)
    {
        cout << i + 1 << ". "
             << optimizer.furniture[i].name
             << " [" << optimizer.furniture[i].symbol << "] "
             << optimizer.furniture[i].width
             << " x "
             << optimizer.furniture[i].height
             << endl;
    }

    optimizer.optimize();

    cout << "\n---- OPTIMO COMPLETE ----\n";

    return 0;
}