#ifndef SCORING_CPP
#define SCORING_CPP

#include "grid.h"
#include <iostream>
using namespace std;

// How many of the best layouts are remembered
const int TOP_LAYOUTS = 3;

// Safety limit so large rooms / many items do not search forever
const long SEARCH_LIMIT = 2000000;

// One complete arrangement of all furniture, with its score
struct Layout
{
    char cells[MAX_SIZE][MAX_SIZE];
    int score;
};

class LayoutScorer
{
public:
    Room* room;
    Furniture items[MAX_FURNITURE];
    int count;
    int px[MAX_FURNITURE];
    int py[MAX_FURNITURE];

    // Priority queue (min-heap kept in a plain array).
    // heap[0] is always the WORST of the kept layouts, so it is the one
    // thrown away when a better layout arrives.
    Layout heap[TOP_LAYOUTS + 1];
    int heapSize;

    Layout ranked[TOP_LAYOUTS];     // best first, filled by findBest()
    int rankedCount;

    long steps;
    long complete;
    bool stopped;
    bool pruned;        // true when only wall / neighbour spots are tried

    LayoutScorer(Room* r)
    {
        room = r;
        count = 0;
        steps = 0;
        complete = 0;
        stopped = false;
        pruned = false;
        heapSize = 0;
        rankedCount = 0;
    }

    // ---------- priority queue (min-heap on score) ----------

    void swapLayouts(int a, int b)
    {
        Layout tmp = heap[a];
        heap[a] = heap[b];
        heap[b] = tmp;
    }

    void siftUp(int i)
    {
        while (i > 0)
        {
            int parent = (i - 1) / 2;

            if (heap[parent].score <= heap[i].score)
                break;

            swapLayouts(parent, i);
            i = parent;
        }
    }

    void siftDown(int i)
    {
        while (true)
        {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;

            if (left < heapSize && heap[left].score < heap[smallest].score)
                smallest = left;

            if (right < heapSize && heap[right].score < heap[smallest].score)
                smallest = right;

            if (smallest == i)
                break;

            swapLayouts(i, smallest);
            i = smallest;
        }
    }

    // Keep only the TOP_LAYOUTS best layouts
    void pushLayout(const Layout& l)
    {
        if (heapSize < TOP_LAYOUTS)
        {
            heap[heapSize] = l;
            siftUp(heapSize);
            heapSize++;
        }
        else if (l.score > heap[0].score)
        {
            heap[0] = l;            // replace the worst
            siftDown(0);
        }
    }

    // Remove and return the worst layout
    Layout popWorst()
    {
        Layout worst = heap[0];
        heapSize--;
        heap[0] = heap[heapSize];
        siftDown(0);
        return worst;
    }

    // Pruned mode: a spot is only worth trying if the item touches a wall
    // or touches furniture that is already placed.
    bool worthTrying(int idx, int x, int y)
    {
        Furniture& f = items[idx];

        if (x == 0 || y == 0 ||
            x + f.width == room->width ||
            y + f.height == room->height)
            return true;

        for (int j = 0; j < idx; j++)
        {
            bool yOverlap = (y < py[j] + items[j].height) && (py[j] < y + f.height);
            bool xOverlap = (x < px[j] + items[j].width) && (px[j] < x + f.width);

            if ((x + f.width == px[j] || px[j] + items[j].width == x) && yOverlap)
                return true;

            if ((y + f.height == py[j] || py[j] + items[j].height == y) && xOverlap)
                return true;
        }

        return false;
    }

    // ---------- scoring rules ----------

    // Number of window cells covered by this item (item touches that wall
    // at the window's position)
    int windowCells(Furniture& f, int x, int y)
    {
        int n = 0;

        if (y == 0)
            for (int k = x; k < x + f.width; k++)
                if (room->topWall[k] == 'W') n++;

        if (y + f.height == room->height)
            for (int k = x; k < x + f.width; k++)
                if (room->bottomWall[k] == 'W') n++;

        if (x == 0)
            for (int k = y; k < y + f.height; k++)
                if (room->leftWall[k] == 'W') n++;

        if (x + f.width == room->width)
            for (int k = y; k < y + f.height; k++)
                if (room->rightWall[k] == 'W') n++;

        return n;
    }

    // Free cells that cannot be reached from the door (or, with no door,
    // that are cut off from the biggest open area).
    int cutOffFreeCells()
    {
        int W = room->width;
        int H = room->height;

        int label[MAX_SIZE][MAX_SIZE];
        int compSize[MAX_SIZE * MAX_SIZE];
        bool compHasDoor[MAX_SIZE * MAX_SIZE];
        int stack[MAX_SIZE * MAX_SIZE];
        int comps = 0;
        int totalFree = 0;

        for (int i = 0; i < H; i++)
            for (int j = 0; j < W; j++)
                label[i][j] = -1;

        for (int i = 0; i < H; i++)
        {
            for (int j = 0; j < W; j++)
            {
                if (room->space[i][j] != '.' || label[i][j] != -1)
                    continue;

                int top = 0;
                stack[top++] = i * W + j;
                label[i][j] = comps;
                compSize[comps] = 0;
                compHasDoor[comps] = false;

                while (top > 0)
                {
                    int cur = stack[--top];
                    int r = cur / W;
                    int c = cur % W;

                    compSize[comps]++;
                    totalFree++;

                    if (room->blocked[r][c])
                        compHasDoor[comps] = true;

                    int dr[4] = {-1, 1, 0, 0};
                    int dc[4] = {0, 0, -1, 1};

                    for (int d = 0; d < 4; d++)
                    {
                        int nr = r + dr[d];
                        int nc = c + dc[d];

                        if (nr < 0 || nc < 0 || nr >= H || nc >= W)
                            continue;

                        if (room->space[nr][nc] != '.' || label[nr][nc] != -1)
                            continue;

                        label[nr][nc] = comps;
                        stack[top++] = nr * W + nc;
                    }
                }

                comps++;
            }
        }

        bool anyDoor = false;
        for (int k = 0; k < comps; k++)
            if (compHasDoor[k]) anyDoor = true;

        int reachable = 0;

        if (anyDoor)
        {
            for (int k = 0; k < comps; k++)
                if (compHasDoor[k]) reachable += compSize[k];
        }
        else
        {
            for (int k = 0; k < comps; k++)
                if (compSize[k] > reachable) reachable = compSize[k];
        }

        return totalFree - reachable;
    }

    // Score of the arrangement currently placed in the room.
    // Higher is better. Only two rules are used:
    //   1. Wardrobe / bookshelf should not block a window
    //   2. Empty floor should stay connected to the door
    int scoreCurrent()
    {
        int score = 0;

        // Rule 1: -8 for every window cell covered by a wardrobe or bookshelf
        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (s == 'W' || s == 'S')
                score -= 8 * windowCells(items[i], px[i], py[i]);
        }

        // Rule 2: -4 for every empty cell cut off from the door
        score -= 4 * cutOffFreeCells();

        return score;
    }

    // ---------- search ----------

    bool sameItem(Furniture& a, Furniture& b)
    {
        return strcmp(a.name, b.name) == 0 &&
               a.width == b.width &&
               a.height == b.height;
    }

    void search(int idx, int startPos)
    {
        if (stopped)
            return;

        if (idx == count)
        {
            complete++;

            Layout l;
            for (int i = 0; i < MAX_SIZE; i++)
                for (int j = 0; j < MAX_SIZE; j++)
                    l.cells[i][j] = room->space[i][j];

            l.score = scoreCurrent();

            pushLayout(l);

            return;
        }

        Furniture cur = items[idx];
        int W = room->width;
        int H = room->height;

        for (int pos = startPos; pos < W * H; pos++)
        {
            if (++steps > SEARCH_LIMIT)
            {
                stopped = true;
                return;
            }

            int y = pos / W;
            int x = pos % W;

            if (!room->canPlace(cur, x, y))
                continue;

            if (pruned && !worthTrying(idx, x, y))
                continue;

            room->placeFurniture(cur, x, y);
            px[idx] = x;
            py[idx] = y;

            // Identical items: do not try the same spots in a different order
            int next = 0;
            if (idx + 1 < count && sameItem(items[idx + 1], cur))
                next = pos + 1;

            search(idx + 1, next);

            room->removeFurniture(cur, x, y);

            if (stopped)
                return;
        }
    }

    // Tries layouts, keeps the best few in the priority queue.
    // Returns false if no layout fits.
    bool findBest(Furniture list[], int n)
    {
        count = n;
        for (int i = 0; i < n; i++)
            items[i] = list[i];

        // Pass 1: try every possible layout
        search(0, 0);

        // Pass 2: too many layouts - retry, trying only sensible spots
        if (stopped)
        {
            heapSize = 0;

            steps = 0;
            complete = 0;
            stopped = false;
            pruned = true;

            search(0, 0);
        }

        // Empty the queue from worst to best, filling the list from the back
        // so that ranked[0] is the best layout
        rankedCount = heapSize;
        for (int k = rankedCount - 1; k >= 0; k--)
            ranked[k] = popWorst();

        return rankedCount > 0;
    }

    void showBest()
    {
        Layout& top = ranked[0];

        for (int i = 0; i < MAX_SIZE; i++)
            for (int j = 0; j < MAX_SIZE; j++)
                room->space[i][j] = top.cells[i][j];

        cout << "\n----- OPTIMIZED LAYOUT (BEST SCORE) -----\n";
        room->display();

        cout << "\nBest score: " << top.score << endl;

        if (rankedCount > 1)
        {
            cout << "Other good layouts: ";
            for (int i = 1; i < rankedCount; i++)
            {
                cout << ranked[i].score;
                if (i + 1 < rankedCount) cout << ", ";
            }
            cout << endl;
        }

        cout << "Layouts checked: " << complete;
        if (stopped)
            cout << " (search stopped at limit, best of those checked)";
        else if (pruned)
            cout << " (large room: only wall / next-to-furniture spots tried)";
        cout << endl;
    }
};

#endif