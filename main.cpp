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

// Counts furniture with a given symbol
int countOf(Optimizer& o, char symbol)
{
    int n = 0;
    for (int i = 0; i < o.furnitureCount; i++)
        if (o.furniture[i].symbol == symbol)
            n++;
    return n;
}

// Rules for the number / kind of furniture a room can hold.
// A house room has one dining table, one TV, at most two (single) beds...
// A restaurant or library may have many tables.
bool checkFurnitureRules(Room& room, Optimizer& o)
{
    if (room.type == 0 || room.isPublic())
    {
        if (room.isPublic() &&
            (countOf(o, 'B') > 0 || countOf(o, 'N') > 0))
        {
            cout << "\nInvalid: a " << room.typeName()
                 << " cannot contain a bed or nightstand.\n";
            return false;
        }
        return true;
    }

    // Rooms of a house
    if (room.type != 1 && (countOf(o, 'B') > 0 || countOf(o, 'N') > 0))
    {
        cout << "\nInvalid: a " << room.typeName()
             << " cannot contain a bed or nightstand.\n";
        return false;
    }

    int beds = countOf(o, 'B');
    if (beds > 2)
    {
        cout << "\nInvalid: a bedroom can hold at most 2 beds.\n";
        return false;
    }

    if (beds == 2)
    {
        for (int i = 0; i < o.furnitureCount; i++)
        {
            Furniture& f = o.furniture[i];
            int narrow = f.width < f.height ? f.width : f.height;

            if (f.symbol == 'B' && narrow > 1)
            {
                cout << "\nInvalid: two beds are only allowed if both are"
                     << " single beds (1 cell wide).\n";
                return false;
            }
        }
    }

    if (countOf(o, 'T') > 1)
    {
        cout << "\nInvalid: a " << room.typeName()
             << " can have only one table.\n";
        return false;
    }

    if (countOf(o, 'V') > 1 || countOf(o, 'K') > 1 ||
        countOf(o, 'W') > 2 || countOf(o, 'N') > 2 || countOf(o, 'O') > 3)
    {
        cout << "\nInvalid: too many of one item for a " << room.typeName()
             << " (max: 1 TV, 1 desk, 2 wardrobes, 2 nightstands, 3 sofas).\n";
        return false;
    }

    return true;
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

    cout << "\nRoom type:\n"
         << "1. Bedroom\n2. Drawing room\n3. Study\n4. Dining room\n"
         << "5. Restaurant\n6. Library\n7. Other\n"
         << "Choose (1-7): ";
    int choice;
    cin >> choice;
    room.type = (choice >= 1 && choice <= 6) ? choice : 0;

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
        else if (strcmp(f.name, "sofa") == 0)
            f.symbol = 'O';
        else if (strcmp(f.name, "tv") == 0)
            f.symbol = 'V';
        else if (strcmp(f.name, "desk") == 0)
            f.symbol = 'K';
        else if (strcmp(f.name, "nightstand") == 0)
            f.symbol = 'N';
        else
            f.symbol = 'F';

        bool sized = false;

        if (f.symbol == 'B')
        {
            cout << "Bed type:\n"
                 << "1. Single (1 x 2)\n"
                 << "2. Double (2 x 2)\n"
                 << "3. Queen  (2 x 3)\n"
                 << "4. King   (3 x 3)\n"
                 << "5. Custom size\n"
                 << "Choose (1-5): ";

            int bedType;
            cin >> bedType;

            if (bedType == 1) { f.width = 1; f.height = 2; sized = true; }
            else if (bedType == 2) { f.width = 2; f.height = 2; sized = true; }
            else if (bedType == 3) { f.width = 2; f.height = 3; sized = true; }
            else if (bedType == 4) { f.width = 3; f.height = 3; sized = true; }
        }

        if (!sized)
        {
            cout << "Enter width: ";
            cin >> f.width;

            cout << "Enter height: ";
            cin >> f.height;
        }

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

    // Every room type needs its main piece of furniture
    char need = 0;
    const char* needName = "";
    if (room.type == 1) { need = 'B'; needName = "bed"; }
    if (room.type == 2) { need = 'O'; needName = "sofa"; }
    if (room.type == 3) { need = 'K'; needName = "desk"; }
    if (room.type == 4) { need = 'T'; needName = "table"; }
    if (room.type == 5) { need = 'T'; needName = "table"; }
    if (room.type == 6) { need = 'S'; needName = "bookshelf"; }

    if (need != 0)
    {
        bool found = false;
        for (int i = 0; i < optimizer.furnitureCount; i++)
            if (optimizer.furniture[i].symbol == need) found = true;

        if (!found)
        {
            cout << "\nInvalid: a " << room.typeName()
                 << " needs a " << needName << ".\n";
            return 0;
        }
    }

    if (!checkFurnitureRules(room, optimizer))
        return 0;

    optimizer.optimize();

    cout << "\n---- OPTIMO COMPLETE ----\n";

    return 0;
}