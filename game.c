#include "common.h"

int count_neighbours(int x, int y, uint8_t *board, int width, int height)
{
    int count = 0;

    if ((x-1 >= 0) && (y-1 >= 0) && (board[(y-1)*width+(x-1)] == 'X'))
        ++count;

    if ((y-1 >= 0) && (board[(y-1)*width+x] == 'X'))
        ++count;

    if ((x+1 < width) && (y-1 >= 0) && (board[(y-1)*width+(x+1)] == 'X'))
        ++count;

    if ((x-1 >= 0) && (board[y*width+(x-1)] == 'X'))
        ++count;

    if ((x+1 < width) && (board[y*width+(x+1)] == 'X'))
        ++count;

    if ((x-1 >= 0) && (y+1 < height) && (board[(y+1)*width+(x-1)] == 'X'))
        ++count;

    if ((y+1 < height) && (board[(y+1)*width] == 'X'))
        ++count;

    if ((x+1 < width) && (y+1 < height) && (board[(y+1)*width+(x+1)] == 'X'))
        ++count;
    
    return count;
}

GAME_UPDATE(game_update)
{
    /* Input handling */
    switch (g->input)
    {
    case 'g': // open/close the git menu
        g->flags = g->flags & GIT_MENU ? NORMAL : GIT_MENU;
        // in any case, clean input fields
        g->inputn = 0;
        for (size_t i = 0; i < sizeof(g->inputfield); ++i)
            g->inputfield[i] = 0;
        return;
        break;
    case 'n': // advance the simulation
        break;

    case 'd': // toggle debug
        g->flags = g->flags & DEBUG_NEIGHBOURS ? g->flags & ~(DEBUG_NEIGHBOURS) : g->flags | DEBUG_NEIGHBOURS;
        return;
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        // If we are on the menu, record numerical input
        if (g->flags & GIT_MENU && g->inputn < sizeof(g->inputfield))
            g->inputfield[g->inputn++] = g->input;
        break;

    case KEY_ENTER:
    case KEY_IL:
    case '\n':
    case '\r':
        // If we are on the menu, check if the input field points to the ID of a correct commit
        if (g->flags & GIT_MENU)
        {
            int n = strtol(g->inputfield, NULL, 10);
            commit_node_t *commit = g->commit_list;
            int i;
            for (i = 1; i < n; ++i)
            {
                if (commit && commit->next)
                {
                    commit = commit->next;
                }
                else
                {
                    break;
                }
            }

            if (i == n) // we found the commit we wanted
            {
                // Mark the commit for the platform layer to take care of it
                g->selected_oid = commit->oid;
            }
        }
        break;

    case KEY_BACKSPACE:
    case KEY_DC:
    case KEY_DL:
        // If we are on the menu, erase 1 character from input
        if (g->flags & GIT_MENU && g->inputn)
        {
            g->inputfield[--g->inputn] = 0;
        }
        
    default: // ignore non-expected output
        return;
        break;
    }
    
    
    if (g->flags & NORMAL)
    {
        for (int i = 0; i < g->height; ++i)
            for (int j = 0; j < g->width; ++j)
            {
                int neighbours = count_neighbours(j, i, g->board, g->width, g->height);
                if (g->board[i*g->width+j] == 'X') // currently alive
                {
                    if (neighbours == 2 || neighbours == 3)
                    {
                        // Live
                        g->aux_board[i*g->width+j] = 'X';
                    }
                    else
                    {
                        // Die
                        g->aux_board[i*g->width+j] = ' ';
                    }
                }
                else
                {
                    // Currently dead                    
                    if (neighbours == 3)
                    {
                        // Live
                        g->aux_board[i*g->width+j] = 'X';
                    }
                    else
                    {
                        // Die
                        g->aux_board[i*g->width+j] = ' ';
                    }
                }
            }
        memcpy(g->board, g->aux_board, g->width*g->height);
    }
    else if (g->flags & GIT_MENU)
    {
        return;
    }
}

GAME_RENDER(game_render)
{
    wclear(g->window);
    if (g->flags & NORMAL)
    {
        wmove(g->window, 0, 0);
        //wprintw(g->window, "%s", g->board);
        wattrset(g->window, A_BOLD);
        for (int i = 0; i < g->height; ++i)
        {
            for (int j = 0; j < g->width; ++j)
            {
                if (g->flags & DEBUG_NEIGHBOURS)
                {
                    int neighbours = count_neighbours(j, i, g->board, g->width, g->height);
                    wprintw(g->window, "%d", neighbours);
                }
                else
                {
                    wprintw(g->window, "%c", g->board[i*g->width+j]);
                }
            }
        }
        wattroff(g->window, A_BOLD);
    }
    else
    {
        // print info about commits
        commit_node_t *commit = g->commit_list;

        int maxy;
        int maxx;
        getmaxyx(g->window, maxy, maxx);

        int wiggle_room = maxx - (32*4) + 9;

        if (wiggle_room < 0)
            wiggle_room = 0;

        int id_xoffset = wiggle_room/2;
        int p_xoffset = id_xoffset + 3;
        int g_xoffset = p_xoffset + 3;
        int oid_xoffset = g_xoffset + 3;
        int author_xoffset = oid_xoffset + 32;
        int date_xoffset = author_xoffset + 32;
        int summary_xoffset = date_xoffset + 32;

        char legend[] = "Write the commit # to load a different version on runtime:";
        int legend_xoffset = (maxx - sizeof(legend)) / 2;
        mvwprintw(g->window, 5, legend_xoffset, legend);
        mvwprintw(g->window, 5, legend_xoffset + sizeof(legend) + 1, g->inputfield);

        // Save cursor position of input field

        int inputcursory;
        int inputcursorx;        
        getyx(g->window, inputcursory, inputcursorx);

        // Print debug info
        mvwprintw(g->window, 7, legend_xoffset, g->debuginfo);
        
        wattrset(g->window, A_BOLD);
        mvwprintw(g->window, 10, id_xoffset, "ID");
        mvwprintw(g->window, 10, p_xoffset, "P");
        mvwprintw(g->window, 10, g_xoffset, "G");
        mvwprintw(g->window, 10, oid_xoffset, "OID");
        mvwprintw(g->window, 10, author_xoffset, "AUTHOR");
        mvwprintw(g->window, 10, date_xoffset, "DATE");
        mvwprintw(g->window, 10, summary_xoffset, "SUMMARY");
        wattroff(g->window, A_BOLD);
        int i = 1;
        
        while (commit)
        {
            mvwprintw(g->window, 10+i, id_xoffset, "%d", i);
            
            if (!memcmp(commit->oid.id, g->platform_oid.id, 20))
            {
                mvwprintw(g->window, 10+i, p_xoffset, "X");
            }

            if (!memcmp(commit->oid.id, g->game_oid.id, 20))
            {
                mvwprintw(g->window, 10+i, g_xoffset, "X");
            }
            
            mvwprintw(g->window, 10+i, oid_xoffset, "%.31s", commit->oid_as_string);
            mvwprintw(g->window, 10+i, author_xoffset, commit->author);
            wprintw(g->window, " <%s>", commit->email);
            mvwprintw(g->window, 10+i, date_xoffset, commit->date_as_string);
            mvwprintw(g->window, 10+i, summary_xoffset, commit->summary);


            // Move the cursor back to the input field
            wmove(g->window, inputcursory, inputcursorx);
            
            commit = commit->next;
            ++i;
        }
        wborder(g->window, 0, 0, 0, 0, 0, 0, 0, 0);
    }    
}



GAME_RESET(game_reset)
{
    int i;
    for (i = 0; i < (g->height*g->width); ++i)
        g->board[i] = ((double)rand() / RAND_MAX) > 0.8 ? 'X' : ' ';

    for (i = 0; i < (g->height*g->width); ++i)
        g->aux_board[i] = ' ';

    g->board[i] = '\0';
    g->aux_board[i] = '\0';
}
