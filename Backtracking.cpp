#include "grid.h"
#include "scoring.cpp"
#include <iostream>
using namespace std;

class Optimizer
{
public:
    Room* room;
    Furniture furniture[MAX_FURNITURE];
    int furnitureCount;

    Optimizer(Room* r)
    {
        room = r;
        furnitureCount = 0;
    }

    void addFurniture(Furniture f)
    {
        if (furnitureCount < MAX_FURNITURE)
        {
            furniture[furnitureCount] = f;
            furnitureCount++;
        }
    }

    // Old first-fit search (kept for reference, no longer used by optimize)
    bool generateLayout(int index)
    {
        if (index == furnitureCount)
            return true;

        Furniture current = furniture[index];

        for (int y = 0; y < room->height; y++)
        {
            for (int x = 0; x < room->width; x++)
            {
                if (room->canPlace(current, x, y))
                {
                    room->placeFurniture(current, x, y);

                    if (generateLayout(index + 1))
                        return true;

                    room->removeFurniture(current, x, y);
                }
            }
        }

        return false;
    }

    // Tries many layouts, scores each one, and shows the highest scoring
    void optimize()
    {
        LayoutScorer scorer(room);

        if (scorer.findBest(furniture, furnitureCount))
        {
            scorer.showBest();
        }
        else
        {
            cout << "\nNo valid layout found.\n";
        }
    }
};