#ifndef SCORING_CPP
#define SCORING_CPP

#include "grid.h"
#include <iostream>
using namespace std;

// How many of the best layouts are remembered
const int TOP_LAYOUTS = 3;

// How many complete layouts are tried at most. Small rooms are searched
// completely; in big rooms the positions are sampled across the whole room.
const long LAYOUT_BUDGET = 200000;

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
    bool sampled;       // true when some positions were skipped (big rooms)

    LayoutScorer(Room* r)
    {
        room = r;
        count = 0;
        steps = 0;
        complete = 0;
        stopped = false;
        sampled = false;
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

        // Tables may stand in the middle of the room (public rooms: every 2nd cell)
        if (room->type == 4 && f.symbol == 'T')
            return true;

        if (room->isPublic() && f.symbol == 'T')
            return x % 2 == 0 && y % 2 == 0;

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

    // True if two placed items share an edge (no walking space between them)
    bool touching(int i, int j)
    {
        int xi = px[i], yi = py[i], wi = items[i].width, hi = items[i].height;
        int xj = px[j], yj = py[j], wj = items[j].width, hj = items[j].height;

        bool yOverlap = (yi < yj + hj) && (yj < yi + hi);
        bool xOverlap = (xi < xj + wj) && (xj < xi + wi);

        if ((xi + wi == xj || xj + wj == xi) && yOverlap)
            return true;

        if ((yi + hi == yj || yj + hj == yi) && xOverlap)
            return true;

        return false;
    }

    // ---------- helpers for the room-type rules ----------

    bool isTall(char s)
    {
        return s == 'W' || s == 'S' || s == 'V';   // wardrobe, bookshelf, TV
    }

    bool touchesWall(int i)
    {
        return px[i] == 0 || py[i] == 0 ||
               px[i] + items[i].width == room->width ||
               py[i] + items[i].height == room->height;
    }

    // Empty cells between two items (0 = touching), along the larger axis
    int gapBetween(int i, int j)
    {
        int dx = 0, dy = 0;

        if (px[j] - (px[i] + items[i].width) > dx) dx = px[j] - (px[i] + items[i].width);
        if (px[i] - (px[j] + items[j].width) > dx) dx = px[i] - (px[j] + items[j].width);
        if (py[j] - (py[i] + items[i].height) > dy) dy = py[j] - (py[i] + items[i].height);
        if (py[i] - (py[j] + items[j].height) > dy) dy = py[i] - (py[j] + items[j].height);

        return dx > dy ? dx : dy;
    }

    // True if the two items face each other along a straight line
    bool aligned(int i, int j)
    {
        bool xOver = (px[i] < px[j] + items[j].width) && (px[j] < px[i] + items[i].width);
        bool yOver = (py[i] < py[j] + items[j].height) && (py[j] < py[i] + items[i].height);
        return xOver || yOver;
    }

    // Free cells on the best side of an item (its "front")
    int freeInFront(int i)
    {
        int x = px[i], y = py[i];
        int w = items[i].width, h = items[i].height;
        int best = 0;

        int n = 0;
        if (y > 0)
            for (int k = x; k < x + w; k++) if (room->space[y - 1][k] == '.') n++;
        if (n > best) best = n;

        n = 0;
        if (y + h < room->height)
            for (int k = x; k < x + w; k++) if (room->space[y + h][k] == '.') n++;
        if (n > best) best = n;

        n = 0;
        if (x > 0)
            for (int k = y; k < y + h; k++) if (room->space[k][x - 1] == '.') n++;
        if (n > best) best = n;

        n = 0;
        if (x + w < room->width)
            for (int k = y; k < y + h; k++) if (room->space[k][x + w] == '.') n++;
        if (n > best) best = n;

        return best;
    }

    // Pairs that are allowed (and expected) to touch
    bool mayTouch(char a, char b)
    {
        if ((a == 'C' && b == 'T') || (a == 'T' && b == 'C')) return true;
        if ((a == 'C' && b == 'K') || (a == 'K' && b == 'C')) return true;
        if ((a == 'N' && b == 'B') || (a == 'B' && b == 'N')) return true;
        return false;
    }

    // True if item i touches any item with symbol s
    bool touchesSymbol(int i, char s)
    {
        for (int j = 0; j < count; j++)
            if (j != i && items[j].symbol == s && touching(i, j))
                return true;
        return false;
    }

    // Smallest gap from item i to any item with symbol s (-1 if none)
    int nearestGap(int i, char s)
    {
        int best = -1;
        for (int j = 0; j < count; j++)
        {
            if (j == i || items[j].symbol != s) continue;
            int g = gapBetween(i, j);
            if (best == -1 || g < best) best = g;
        }
        return best;
    }

    int firstOf(char s)
    {
        for (int i = 0; i < count; i++)
            if (items[i].symbol == s) return i;
        return -1;
    }

    // ---------- presentation helpers ----------

    int wallsTouched(int i)
    {
        int n = 0;
        if (px[i] == 0) n++;
        if (py[i] == 0) n++;
        if (px[i] + items[i].width == room->width) n++;
        if (py[i] + items[i].height == room->height) n++;
        return n;
    }

    // Bed in the middle (give or take one cell) of the wall it touches
    bool bedCentred(int i)
    {
        int x2 = 2 * px[i] + items[i].width - room->width;
        int y2 = 2 * py[i] + items[i].height - room->height;

        if (x2 < 0) x2 = -x2;
        if (y2 < 0) y2 = -y2;

        if ((py[i] == 0 || py[i] + items[i].height == room->height) && x2 <= 1)
            return true;
        if ((px[i] == 0 || px[i] + items[i].width == room->width) && y2 <= 1)
            return true;
        return false;
    }

    // Items that belong together and may stand close
    bool relatedItems(char a, char b)
    {
        if (mayTouch(a, b)) return true;
        if ((a == 'O' && (b == 'T' || b == 'V')) ||
            (b == 'O' && (a == 'T' || a == 'V'))) return true;
        return false;
    }

    // How far the middle of all furniture is from the middle of the room
    int spreadPenalty()
    {
        long sumA = 0, sx = 0, sy = 0;

        for (int i = 0; i < count; i++)
        {
            long a = items[i].width * items[i].height;
            sumA += a;
            sx += a * (2 * px[i] + items[i].width);
            sy += a * (2 * py[i] + items[i].height);
        }

        int dx = (int)(sx / sumA) - room->width;
        int dy = (int)(sy / sumA) - room->height;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        return (dx + dy) / 2;
    }

    // ---------- room-type rules ----------

    // Bedroom: bed headboard on a wall, bedside table next to the bed
    int bedroomScore()
    {
        int sc = 0;

        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (s == 'B' && !touchesWall(i))
                sc -= 10;

            if (s == 'N' && firstOf('B') != -1 && !touchesSymbol(i, 'B'))
                sc -= 6;

            // A chair belongs at the table
            if (s == 'C' && firstOf('T') != -1 && !touchesSymbol(i, 'T'))
                sc -= 5;
        }
        return sc;
    }

    // Drawing room: sofa faces the TV at a comfortable distance,
    // sofa against a wall, coffee table one cell in front of the sofa
    int drawingRoomScore()
    {
        int sc = 0;
        int tv = firstOf('V');

        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (s == 'O')
            {
                if (!touchesWall(i))
                    sc -= 4;

                if (tv != -1)
                {
                    int g = gapBetween(i, tv);

                    if (!aligned(i, tv)) sc -= 10;
                    if (g < 2) sc -= 8;
                    else if (g > 5) sc -= 2 * (g - 5);
                }
            }

            if (s == 'T')
            {
                int g = nearestGap(i, 'O');
                if (g != -1)
                    sc -= 4 * (g > 1 ? g - 1 : 1 - g);

                // Coffee table between the sofa and the TV
                int so = firstOf('O');
                if (so != -1 && tv != -1)
                {
                    int x0 = px[so] < px[tv] ? px[so] : px[tv];
                    int y0 = py[so] < py[tv] ? py[so] : py[tv];
                    int x1 = px[so] + items[so].width > px[tv] + items[tv].width
                                 ? px[so] + items[so].width : px[tv] + items[tv].width;
                    int y1 = py[so] + items[so].height > py[tv] + items[tv].height
                                 ? py[so] + items[so].height : py[tv] + items[tv].height;
                    int cx = 2 * px[i] + items[i].width;
                    int cy = 2 * py[i] + items[i].height;

                    if (cx >= 2 * x0 && cx <= 2 * x1 && cy >= 2 * y0 && cy <= 2 * y1)
                        sc += 6;
                    else
                        sc -= 4;
                }
            }
        }
        return sc;
    }

    // Study: desk on a wall, ideally by a window; chair tucked at the desk
    int studyScore()
    {
        int sc = 0;

        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (s == 'K')
            {
                if (!touchesWall(i))
                    sc -= 8;

                sc += 3 * windowCells(items[i], px[i], py[i]);
            }

            if (s == 'C' && firstOf('K') != -1 && !touchesSymbol(i, 'K'))
                sc -= 8;

            // Chair sits on the long side of the desk, not at its end
            if (s == 'C')
            {
                for (int d = 0; d < count; d++)
                {
                    if (items[d].symbol != 'K' || !touching(i, d))
                        continue;

                    bool atSide = (px[i] + items[i].width == px[d] ||
                                   px[d] + items[d].width == px[i]);
                    bool wide = items[d].width >= items[d].height;

                    if ((wide && atSide) || (!wide && !atSide))
                        sc -= 6;
                }
            }
        }
        return sc;
    }

    // Dining room: chairs around the table, table not pushed to a wall
    int diningScore()
    {
        int sc = 0;

        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (s == 'T' && touchesWall(i))
                sc -= 3;

            if (s == 'C' && firstOf('T') != -1 && !touchesSymbol(i, 'T'))
                sc -= 6;
        }
        return sc;
    }

    // Restaurant / library: tables spread out, each table has chairs,
    // tables by a window are nicer
    int publicScore()
    {
        int sc = 0;

        for (int i = 0; i < count; i++)
        {
            if (items[i].symbol != 'T')
                continue;

            if (!touchesSymbol(i, 'C'))
                sc -= 4;

            sc += 2 * windowCells(items[i], px[i], py[i]);

            for (int j = i + 1; j < count; j++)
            {
                if (items[j].symbol != 'T')
                    continue;

                int g = gapBetween(i, j);
                if (g < 5)
                    sc -= 2 * (5 - g);
            }
        }
        return sc;
    }

    // Score of the arrangement currently placed in the room. Higher is better.
    // Rules for every room:
    //   1. Wardrobe / bookshelf / TV should not block a window
    //   2. Empty floor should stay connected to the door
    //   3. Tall items (wardrobe, bookshelf, TV) stand against a wall
    //   4. Wardrobe / bookshelf keep their front free (doors / access)
    // plus the rules of the chosen room type.
    // (Walking space between furniture and in front of doors is a hard
    //  rule handled in search(), not a score.)
    int scoreCurrent()
    {
        int score = 0;

        for (int i = 0; i < count; i++)
        {
            char s = items[i].symbol;

            if (isTall(s))
            {
                score -= 8 * windowCells(items[i], px[i], py[i]);

                if (!touchesWall(i))
                    score -= 6;
            }

            if (s == 'W' || s == 'S')
            {
                int need = items[i].width > items[i].height
                               ? items[i].width : items[i].height;
                int fr = freeInFront(i);

                if (fr < need)
                    score -= 3 * (need - fr);
            }
        }

        score -= 4 * cutOffFreeCells();

        // Presentation rules (make the room look like a real room)
        for (int i = 0; i < count; i++)
        {
            // Wardrobe in a corner
            if (items[i].symbol == 'W' && wallsTouched(i) >= 2)
                score += 4;

            // Bed centred on the wall it stands against
            if (items[i].symbol == 'B' && bedCentred(i))
                score += 3;

            // Unrelated furniture should not be crowded side by side
            for (int j = i + 1; j < count; j++)
            {
                char a = items[i].symbol, b = items[j].symbol;

                if (a == 'C' && b == 'C')
                    continue;

                if (!relatedItems(a, b) && gapBetween(i, j) < 2)
                    score -= 3;
            }
        }

        // Furniture spread over the room, not bunched in one part
        score -= spreadPenalty();

        if (room->type == 1) score += bedroomScore();
        if (room->type == 2) score += drawingRoomScore();
        if (room->type == 3) score += studyScore();
        if (room->type == 4) score += diningScore();
        if (room->isPublic()) score += publicScore();

        return score;
    }

    // ---------- search ----------

    bool sameItem(Furniture& a, Furniture& b)
    {
        return strcmp(a.name, b.name) == 0 &&
               a.width == b.width &&
               a.height == b.height;
    }

    // Hard spacing rules between the item idx and the items before it
    bool spacingOk(int idx)
    {
        char si = items[idx].symbol;

        // A chair belongs to a table / desk (study, dining, restaurant, library)
        if (si == 'C' && room->type >= 3 && (firstOf('T') != -1 || firstOf('K') != -1))
        {
            bool atTable = false;
            for (int j = 0; j < idx; j++)
                if ((items[j].symbol == 'T' || items[j].symbol == 'K') && touching(idx, j))
                    atTable = true;
            if (!atTable)
                return false;
        }

        for (int j = 0; j < idx; j++)
        {
            char sj = items[j].symbol;

            // No touching, except chair-table, chair-desk, nightstand-bed
            if (touching(idx, j) && !mayTouch(si, sj))
                return false;

            // Tables in a restaurant / library: room for chairs and people
            if (room->isPublic() && si == 'T' && sj == 'T' &&
                gapBetween(idx, j) < MIN_TABLE_GAP)
                return false;

            // Library aisles between shelves and tables
            if (room->type == 6 &&
                ((si == 'S' && (sj == 'S' || sj == 'T')) ||
                 (si == 'T' && sj == 'S')) &&
                gapBetween(idx, j) < LIBRARY_AISLE)
                return false;
        }
        return true;
    }

    // Every chair at a table needs the cell behind it free, so a person
    // can pull the chair back to sit down and stand up
    bool pullOutOk(int upTo)
    {
        for (int c = 0; c <= upTo; c++)
        {
            if (items[c].symbol != 'C')
                continue;

            int xc = px[c], yc = py[c];
            int wc = items[c].width, hc = items[c].height;

            for (int t = 0; t <= upTo; t++)
            {
                if (items[t].symbol != 'T' && items[t].symbol != 'K')
                    continue;

                int xt = px[t], yt = py[t];
                int wt = items[t].width, ht = items[t].height;

                bool yOver = (yc < yt + ht) && (yt < yc + hc);
                bool xOver = (xc < xt + wt) && (xt < xc + wc);

                if (xc + wc == xt && yOver)
                    if (!behindFree(xc - 1, yc, 1, hc)) return false;

                if (xt + wt == xc && yOver)
                    if (!behindFree(xc + wc, yc, 1, hc)) return false;

                if (yc + hc == yt && xOver)
                    if (!behindFree(xc, yc - 1, wc, 1)) return false;

                if (yt + ht == yc && xOver)
                    if (!behindFree(xc, yc + hc, wc, 1)) return false;
            }
        }
        return true;
    }

    bool behindFree(int x, int y, int w, int h)
    {
        if (x < 0 || y < 0 || x + w > room->width || y + h > room->height)
            return false;

        for (int i = y; i < y + h; i++)
            for (int j = x; j < x + w; j++)
                if (room->space[i][j] != '.')
                    return false;

        return true;
    }

    // Budgeted search. At each item every valid spot is tried if the budget
    // allows; otherwise spots spread over the whole room (always including
    // corners) are tried, so the result is not stuck in one corner.
    void search(int idx, int startPos, long budget)
    {
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

        // Collect the valid spots
        int cand[MAX_SIZE * MAX_SIZE];
        int n = 0;
        bool filter = (W * H > 36);

        for (int pos = startPos; pos < W * H; pos++)
        {
            int y = pos / W;
            int x = pos % W;

            if (!room->canPlace(cur, x, y))
                continue;

            px[idx] = x;
            py[idx] = y;

            if (!spacingOk(idx))
                continue;

            // Big rooms: walls, next to furniture, or an every-2nd-cell grid
            if (filter && !worthTrying(idx, x, y) && !(x % 2 == 0 && y % 2 == 0))
                continue;

            cand[n++] = pos;
        }

        if (n == 0)
            return;

        // Choose which spots to try
        int pick[MAX_SIZE * MAX_SIZE];
        int k = 0;

        if (n <= budget)
        {
            for (int t = 0; t < n; t++)
                pick[k++] = cand[t];
        }
        else
        {
            sampled = true;

            int rest[MAX_SIZE * MAX_SIZE];
            int m = 0;
            bool isPick[MAX_SIZE * MAX_SIZE];

            for (int t = 0; t < n; t++)
                isPick[t] = false;

            // Corner spots first (at most half the budget)
            long cornerLimit = budget / 2;
            long corners = 0;

            for (int t = 0; t < n; t++)
            {
                int y = cand[t] / W;
                int x = cand[t] % W;
                bool hx = (x == 0 || x + cur.width == W);
                bool hy = (y == 0 || y + cur.height == H);

                if (hx && hy && corners < cornerLimit)
                {
                    isPick[t] = true;
                    corners++;
                }
            }

            for (int t = 0; t < n; t++)
                if (!isPick[t])
                    rest[m++] = t;

            long left = budget - corners;

            for (long t = 0; t < left && m > 0; t++)
                isPick[rest[(t * m + m / 2) / left]] = true;

            for (int t = 0; t < n; t++)
                if (isPick[t])
                    pick[k++] = cand[t];
        }

        long childBudget = budget / k;
        if (childBudget < 1)
            childBudget = 1;

        for (int t = 0; t < k; t++)
        {
            int pos = pick[t];
            int y = pos / W;
            int x = pos % W;

            room->placeFurniture(cur, x, y);
            px[idx] = x;
            py[idx] = y;

            // Chairs must have room to be pulled out
            if (!pullOutOk(idx))
            {
                room->removeFurniture(cur, x, y);
                continue;
            }

            // Identical items: do not try the same spots in a different order
            int next = 0;
            if (idx + 1 < count && sameItem(items[idx + 1], cur))
                next = pos + 1;

            search(idx + 1, next, childBudget);

            room->removeFurniture(cur, x, y);
        }
    }

    // Tries layouts, keeps the best few in the priority queue.
    // Returns false if no layout fits.
    bool findBest(Furniture list[], int n)
    {
        count = n;
        for (int i = 0; i < n; i++)
            items[i] = list[i];

        // Big items first (chairs last), equal items next to each other
        for (int i = 1; i < count; i++)
        {
            Furniture key = items[i];
            int ka = key.width * key.height;
            int j = i - 1;

            while (j >= 0 && (items[j].width * items[j].height < ka ||
                   (items[j].width * items[j].height == ka &&
                    items[j].symbol > key.symbol)))
            {
                items[j + 1] = items[j];
                j--;
            }
            items[j + 1] = key;
        }

        search(0, 0, LAYOUT_BUDGET);

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

        cout << "\nRoom type: " << room->typeName() << endl;
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
        if (sampled)
            cout << " (large room: positions spread over the whole room were tried)";
        cout << endl;
    }
};

#endif