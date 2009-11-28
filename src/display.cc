/* display-common.cc
 * 
 * Copyright 2009 Martin Read
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define DISP_CURSES_COMMON_CC
#include "dunbash.hh"
#include "monsters.hh"
#include "objects.hh"
#include "player.hh"
#include "display-common.hh"
#include <curses.h>
#include <stdio.h>
#include <panel.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

FILE *msglog_fp;

// Message channel suppression
bool suppressions[] =
{
    false, false, false, false,
    false, false, false, false,
    false, true
};

bool fruit_salad_inventory;

libmrl::Coord last_projectile_pos = libmrl::NOWHERE;
libmrl::Coord curr_projectile_pos = libmrl::NOWHERE;
Dbash_colour projectile_colour = DBCLR_L_GREY;
int projectile_delay = 40;

int you_colour;
int status_updated;
int map_updated;
int show_terrain;
int hard_redraw;

#define DISP_HEIGHT 21
#define DISP_WIDTH 21

void newsym(libmrl::Coord c)
{
    chtype ch;

    newsym_hook(c);
}

void display_init(void)
{
    int i, j;
#ifdef LOG_MESSAGES
#ifdef MULTIUSER
    user_permissions();
#endif
    msglog_fp = fopen("msglog.txt", "a");
    fprintf(msglog_fp, "-----------\nTimestamp %#Lx\n", uint64_t(time(0)));
#ifdef MULTIUSER
    game_permissions();
#endif
#endif

    if (preferred_display == std::string(""))
    {
        fprintf(stderr, "failure: no display preference specified.\n");
    }
#ifdef INCLUDE_DISP_SDL
    else if (preferred_display == std::string("sdl"))
    {
        fprintf(stderr, "failure: SDL display not implemented yet.\n");
    }
#endif
#ifdef INCLUDE_DISP_CURSES
    else if ((preferred_display == std::string("tty-classic")) ||
             (preferred_display == std::string("tty")))
    {
        // select classic ncurses display
        tty_classic_display_init();
    }
    else if (preferred_display == std::string("tty-new"))
    {
        // select new ncurses display
        tty_new_display_init();
    }
#endif
}

int read_input(char *buffer, int length)
{
    return read_input_hook(buffer, length);
}

void print_msg(int channel, const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    /* For now, assume (1) that the player will never be so inundated
     * with messages that it's dangerous to let them just fly past (2)
     * that messages will be of sane length and nicely formatted. THIS
     * IS VERY BAD CODING PRACTICE! */
    /* Note that every message forces a call to display_update().
     * Events that cause changes to the map or the player should flag
     * the change before calling printmsg. */
    // Even suppressed messages get logged.
    if (msglog_fp)
    {
        va_start(ap2, fmt);
        vfprintf(msglog_fp, fmt, ap2);
        va_end(ap2);
    }
    if (!suppressions[channel])
    {
        va_start(ap, fmt);
        print_msg_hook(channel, fmt, ap);
        va_end(ap);
    }
    display_update();
}

void show_discoveries(void)
{
    int i, j;
    print_msg(0, "You recognise the following items:\n");
    for (i = 0, j = 1; i < PO_COUNT; i++)
    {
        if (permobjs[i].known)
        {
            print_msg(0, "%s\n", permobjs[i].name);
            j++;
        }
        if (j == 19)
        {
            press_enter();
            j = 0;
        }
    }
}

void print_inv(Poclass_num filter)
{
    int i;
    std::string namestr;
    Obj const *optr;
    for (i = 0; i < 19; i++)
    {
        wattrset(message_window, 0);
        optr = u.inventory[i].snapc();
        if (optr && ((filter == POCLASS_NONE) || (permobjs[optr->obj_id].poclass == filter)))
        {
            print_msg(0, "%c) ", 'a' + i);
            if (fruit_salad_inventory)
            {
                switch (optr->quality())
                {
                case Itemqual_bad:
                    wattrset(message_window, colour_attrs[DBCLR_RED]);
                    break;
                case Itemqual_normal:
                    wattrset(message_window, 0);
                    break;
                case Itemqual_good:
                    wattrset(message_window, colour_attrs[DBCLR_GREEN]);
                    break;
                case Itemqual_great:
                    wattrset(message_window, colour_attrs[DBCLR_L_BLUE]);
                    break;
                case Itemqual_excellent:
                    wattrset(message_window, colour_attrs[DBCLR_PURPLE]);
                    break;
                }
            }
            u.inventory[i].snapc()->get_name(&namestr);
            if (u.ring == u.inventory[i])
            {
                namestr += " (on finger)";
            }
            else if (u.weapon == u.inventory[i])
            {
                namestr += " (in hand)";
            }
            else if (u.armour == u.inventory[i])
            {
                namestr += " (being worn)";
            }
            print_msg(0, "%s\n", namestr.c_str());
        }
        wattrset(message_window, 0);
    }
}

int inv_select(Poclass_num filter, const char *action, int accept_blank)
{
    int selection;
    int ch;
    int i;
    int items = 0;
    for (i = 0; i < 19; i++)
    {
        if ((u.inventory[i].valid()) && ((filter == POCLASS_NONE) || (permobjs[u.inventory[i].otyp()].poclass == filter)))
        {
            items++;
        }
    }
    if (items == 0)
    {
        print_msg(MSGCHAN_PROMPT, "You have nothing to %s.\n", action);
        return -1;
    }
    print_msg(MSGCHAN_PROMPT, "Items available to %s\n", action);
    print_inv(filter);
    if (accept_blank)
    {
        print_msg(MSGCHAN_PROMPT, "-) no item\n");
    }
    print_msg(MSGCHAN_PROMPT, "[ESC or SPACE to cancel]\n");
tryagain:
    print_msg(MSGCHAN_PROMPT, "What do you want to %s? ", action);
    ch = wgetch(message_window);
    switch (ch)
    {
    case '-':
        if (accept_blank)
        {
            print_msg(MSGCHAN_PROMPT, "\n");
            return -2;
        }
    case 'x':
    case '\x1b':
    case ' ':
        print_msg(MSGCHAN_PROMPT, "\nNever mind.\n");
        return -1;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
        /* I am assuming that we're in a place where the character
         * set is a strict superset of ASCII. If we're not, the
         * following code may break. */
        print_msg(MSGCHAN_PROMPT, "\n");
        selection = ch - 'a';
        if ((u.inventory[selection].valid()) && ((filter == POCLASS_NONE) || (permobjs[u.inventory[selection].otyp()].poclass == filter)))
        {
            return selection;
        }
        /* Fall through */
    default:
        print_msg(MSGCHAN_PROMPT, "\nBad selection\n");
        goto tryagain;
    }
}

int select_dir(libmrl::Coord *psign, bool silent)
{
    int ch;
    int done = 0;
    if (!silent)
    {
        print_msg(MSGCHAN_PROMPT, "Select a direction with movement keys.\n[ESC or space to cancel].\n");
    }
    while (!done)
    {
        ch = wgetch(message_window);
        switch (ch)
        {
        case 'h':
            *psign = libmrl::WEST;
            done = 1;
            break;
        case 'j':
            *psign = libmrl::SOUTH;
            done = 1;
            break;
        case 'k':
            *psign = libmrl::NORTH;
            done = 1;
            break;
        case 'l':
            *psign = libmrl::EAST;
            done = 1;
            break;
        case 'y':
            *psign = libmrl::NORTHWEST;
            done = 1;
            break;
        case 'u':
            *psign = libmrl::NORTHEAST;
            done = 1;
            break;
        case 'b':
            *psign = libmrl::SOUTHWEST;
            done = 1;
            break;
        case 'n':
            *psign = libmrl::SOUTHEAST;
            done = 1;
            break;
        case '\n':
        case '.':
            psign->y = 0;
            psign->x = 0;
            done = 1;
            break;
        case '\x1b':
        case ' ':
            return -1;	/* cancelled. */
        default:
            if (!silent)
            {
                print_msg(MSGCHAN_PROMPT, "Bad direction (use movement keys).\n");
                print_msg(MSGCHAN_PROMPT, "[Press ESC or space to cancel.]\n");
            }
            break;
        }
    }
    return 0;
}

bool get_interrupt(void)
{
    bool ret = false;
    nodelay(message_window, TRUE);
    if (wgetch(message_window) >= 0)
    {
        ret = true;
    }
    nodelay(message_window, FALSE);
    return ret;
}

Game_cmd get_command(void)
{
    int ch;
    int done = 0;
    while (!done)
    {
        ch = wgetch(message_window);
        switch (ch)
        {
        case 'a':
            return ATTACK;
        case 'z':
            return ZAP_WAND;
        case 'A':
            return ACTIVATE_MISC;
        case 'v':
            return VOCALIZE_WORD;
        case '0':
        case ',':
        case 'g':
            return GET_ITEM;
        case 'd':
            return DROP_ITEM;
        case 'D':
            return DUMP_CHARA;
        case 'S':
            return SAVE_GAME;
        case 'X':
            return QUIT;
        case 'i':
            return SHOW_INVENTORY;
        case 'I':
            return INSPECT_ITEM;
        case ';':
            return FARLOOK;
        case ':':
            return FLOORLOOK;
        case '#':
            return SHOW_TERRAIN;
        case '\\':
            return SHOW_DISCOVERIES;
        case '\x12':
            return RNG_TEST;
        case 'h':
            return MOVE_WEST;
        case 'j':
            return MOVE_SOUTH;
        case 'k':
            return MOVE_NORTH;
        case 'l':
            return MOVE_EAST;
        case 'y':
            return MOVE_NW;
        case 'u':
            return MOVE_NE;
        case 'b':
            return MOVE_SW;
        case 'n':
            return MOVE_SE;
        case 'H':
            return FARMOVE_WEST;
        case 'J':
            return FARMOVE_SOUTH;
        case 'K':
            return FARMOVE_NORTH;
        case 'L':
            return FARMOVE_EAST;
        case 'Y':
            return FARMOVE_NW;
        case 'U':
            return FARMOVE_NE;
        case 'B':
            return FARMOVE_SW;
        case 'N':
            return FARMOVE_SE;
        case 'q':
            return QUAFF_POTION;
        case 'r':
            return READ_SCROLL;
        case 'w':
            return WIELD_WEAPON;
        case 'W':
            return WEAR_ARMOUR;
        case 'T':
            return TAKE_OFF_ARMOUR;
        case 'P':
            return PUT_ON_RING;
        case 'R':
            return REMOVE_RING;
        case '?':
            return GIVE_HELP;
        case 'V':
            return PRINT_VERSION;
        case '<':
            return GO_UP_STAIRS;
        case '>':
            return GO_DOWN_STAIRS;
        case 'e':
            return EAT_FOOD;
        case '.':
            return STAND_STILL;
        case '\x04':
            return WIZARD_DESCEND;
        case '\x05':
            return WIZARD_LEVELUP;
        case '\x10':
            return WIZARD_DUMP_PERSEFFS;
        case '\x0f':
            return WIZARD_CURSE_ME;
        case '\x14':
            return WIZARD_TELEPORT;
        }
    }
    return NO_CMD;
}

int display_shutdown(void)
{
    display_update();
    clear();
    refresh();
    endwin();
    return 0;
}

void pressanykey(void)
{
    print_msg(MSGCHAN_PROMPT, "Press any key to continue.\n");
    wgetch(message_window);
}

int getYN(const char *msg)
{
    int ch;
    print_msg(MSGCHAN_PROMPT, "%s", msg);
    print_msg(MSGCHAN_PROMPT, "Press capital Y to confirm, any other key to cancel\n");
    ch = wgetch(message_window);
    if (ch == 'Y')
    {
        return 1;
    }
    return 0;
}

int getyn(const char *msg)
{
    int ch;
    print_msg(MSGCHAN_PROMPT, "%s", msg);
    while (1)
    {
        ch = wgetch(message_window);
        switch (ch)
        {
        case 'y':
        case 'Y':
            return 1;
        case 'n':
        case 'N':
            return 0;
        case '\x1b':
        case ' ':
            return -1;
        default:
            print_msg(MSGCHAN_PROMPT, "Invalid response. Press y or n (ESC or space to cancel)\n");
        }
    }
}

void print_help(void)
{
    print_help_en_GB();
}

static void print_help_en_GB(void)
{
    print_msg(0, "MOVEMENT\n");
    print_msg(0, "y  k  u\n");
    print_msg(0, " \\ | /\n");
    print_msg(0, "  \\|/\n");
    print_msg(0, "h--*--l\n");
    print_msg(0, "  /|\\\n");
    print_msg(0, " / | \\\n");
    print_msg(0, "b  j  n\n");
    print_msg(0, "Attack monsters in melee by bumping into them.\n");
    print_msg(0, "Doors do not have to be opened before you go through.\n");
    print_msg(0, "Turn on NUM LOCK to use the numeric keypad for movement.\n");
    print_msg(0, "Capitals HJKLYUBN move in the corresponding direction\n"
              "until something interesting happens or is found.\n");
    pressanykey();
    print_msg(0, "\nACTIONS\n");
    print_msg(0, "a   make an attack (used to fire bows)\n");
    print_msg(0, "P   put on a ring\n");
    print_msg(0, "R   remove a ring\n");
    print_msg(0, "W   wear armour\n");
    print_msg(0, "T   take off armour\n");
    print_msg(0, "r   read a scroll\n");
    print_msg(0, "w   wield a weapon\n");
    print_msg(0, "q   quaff a potion\n");
    print_msg(0, "z   zap a wand\n");
    print_msg(0, "A   activate a miscellaneous item\n");
    print_msg(0, "g   pick up an item (also 0 or comma)\n");
    print_msg(0, "d   drop an item\n");
    print_msg(0, "e   eat something edible\n");
    print_msg(0, ">   go down stairs\n");
    print_msg(0, "5   do nothing (wait until next action)\n");
    print_msg(0, ".   do nothing (wait until next action)\n");
    pressanykey();
    print_msg(0, "\nOTHER COMMANDS\n");
    print_msg(0, "S   save and exit\n");
    print_msg(0, "X   quit without saving\n");
    print_msg(0, "i   print your inventory\n");
    print_msg(0, "I   examine an item you are carrying\n");
    print_msg(0, "#   show underlying terrain of occupied squares\n");
    print_msg(0, "\\   list all recognised items\n");
    print_msg(0, "D   dump your character's details to <name>.dump\n");
    print_msg(0, "?   print this message\n");
    print_msg(0, "Control-W    print information about this program's absence of warranty.\n");
    print_msg(0, "Control-D    print information about redistributing this program.\n");
    pressanykey();
    print_msg(0, "\nSYMBOLS - you and your surroundings\n");
    print_msg(0, "@   you\n");
    print_msg(0, ".   floor\n");
    print_msg(0, "<   stairs up\n");
    print_msg(0, ">   stairs down\n");
    print_msg(0, "\"   a pool of liquid\n");
    print_msg(0, "_   an altar\n");
    print_msg(0, "-   an anvil or other unobstructive fitting\n");
    print_msg(0, "|   a furnace or other obstructive fitting\n");
    print_msg(0, "#   wall\n");
    print_msg(0, "+   a door or tombstone\n");
    pressanykey();
    print_msg(0, "\nSYMBOLS - treasure\n");
    print_msg(0, ")   a weapon\n");
    print_msg(0, "(   a missile weapon\n");
    print_msg(0, "[   a suit of armour\n");
    print_msg(0, "=   a ring\n");
    print_msg(0, "?   a scroll\n");
    print_msg(0, "!   a potion\n");
    print_msg(0, "%%   some food\n");
    print_msg(0, "&   corpses, severed body parts, etc.\n");
    print_msg(0, "/   a magic wand\n");
    print_msg(0, "*   a miscellaneous item\n");
    pressanykey();
    print_msg(0, "\nDemons are represented as numbers.\nMost other monsters are shown as letters.\n");
    print_msg(0, "\nThis is all the help you get. Good luck!\n");
}

void animate_projectile(libmrl::Coord pos, Dbash_colour col)
{
    if (!pos_visible(pos))
    {
        return;
    }
    projectile_colour = col;
    last_projectile_pos = curr_projectile_pos;
    curr_projectile_pos = pos;
    if (last_projectile_pos != libmrl::NOWHERE)
    {
        newsym(last_projectile_pos);
    }
    newsym(curr_projectile_pos);
    display_update();
    usleep(projectile_delay * 1000);
}

void projectile_done(void)
{
    last_projectile_pos = curr_projectile_pos;
    curr_projectile_pos = libmrl::NOWHERE;
    newsym(last_projectile_pos);
    last_projectile_pos = libmrl::NOWHERE;
    display_update();
}

void farlook(void)
{
    libmrl::Coord screenpos = { 10, 10 };
    libmrl::Coord mappos = u.pos;
    libmrl::Coord step;
    std::string name;
    bool done = false;
    int i;

    print_msg(MSGCHAN_PROMPT, "Use the movement keys to move the cursor.\n");
    print_msg(MSGCHAN_PROMPT, "Press '.' to examine a square.\n");
    print_msg(MSGCHAN_PROMPT, "Press ESC or SPACE when finished.\n");
    wmove(world_window, screenpos.y, screenpos.x);
    wrefresh(world_window);
    while (!done)
    {
        i = select_dir(&step, true);
        if (i == -1)
        {
            done = true;
        }
        else if ((step.y == 0) && (step.x == 0))
        {
            if (currlev->outofbounds(mappos))
            {
                print_msg(MSGCHAN_PROMPT, "The Outer Darkness.\n");
            }
            else if (!(currlev->flags_at(mappos) & MAPFLAG_EXPLORED))
            {
                print_msg(MSGCHAN_PROMPT, "Unexplored territory\n");
            }
            else
            {
                Mon_handle mh = currlev->monster_at(mappos);
                Obj_handle oh = currlev->object_at(mappos);
                if (mappos == u.pos)
                {
                    print_msg(MSGCHAN_PROMPT, "An unfortunate adventurer\n");
                }
                if (mh.valid() && mon_visible(mh))
                {
                    //describe_monster(currlev->monster_at(mappos));
                    mh.snapc()->get_name(&name, 0);
                    print_msg(MSGCHAN_PROMPT, "%s\n", name.c_str());
                }
                if (oh.valid())
                {
                    oh.snapc()->get_name(&name);
                    print_msg(MSGCHAN_PROMPT, "%s\n", name.c_str());
                }
                print_msg(MSGCHAN_PROMPT, "%s\n", terrain_data[currlev->terrain_at(mappos)].name);
            }
            if (wizard_mode)
            {
                print_msg(0, "%d %d: flags %8.8x\n", mappos.y, mappos.x, currlev->flags_at(mappos));
            }
        }
        else
        {
            if (currlev->outofbounds(mappos + step))
            {
                continue;
            }
            libmrl::Coord tmp_spos = screenpos + step;
            if ((tmp_spos.y < 0) || (tmp_spos.x < 0) ||
                (tmp_spos.y >= DISP_HEIGHT) || (tmp_spos.x >= DISP_WIDTH))
            {
                continue;
            }
            screenpos = tmp_spos;
            mappos = mappos + step;
            wmove(world_window, screenpos.y, screenpos.x);
            wrefresh(world_window);
        }
    }
    print_msg(MSGCHAN_PROMPT, "Done.\n");
}

void get_smite_target(libmrl::Coord *ppos)
{
    libmrl::Coord screenpos = { 10, 10 };
    libmrl::Coord mappos = u.pos;
    libmrl::Coord step;
    std::string name;
    bool done = false;
    int i;

    print_msg(MSGCHAN_PROMPT, "Use the movement keys to move the cursor.\n");
    print_msg(MSGCHAN_PROMPT, "Press '.' or ENTER to select a target square.\n");
    print_msg(MSGCHAN_PROMPT, "Press ESC or SPACE to cancel.\n");
    wmove(world_window, screenpos.y, screenpos.x);
    wrefresh(world_window);
    while (!done)
    {
        i = select_dir(&step, true);
        if (i == -1)
        {
            done = true;
        }
        else if ((step.y == 0) && (step.x == 0))
        {
            if (currlev->outofbounds(mappos))
            {
                print_msg(MSGCHAN_PROMPT, "The Outer Darkness.\n");
            }
            else if (!(currlev->flags_at(mappos) & MAPFLAG_EXPLORED))
            {
                print_msg(MSGCHAN_PROMPT, "Unexplored territory\n");
            }
            else
            {
                Mon_handle mh = currlev->monster_at(mappos);
                Obj_handle oh = currlev->object_at(mappos);
                if (mappos == u.pos)
                {
                    print_msg(MSGCHAN_PROMPT, "An unfortunate adventurer\n");
                }
                else if (mh.valid() & mon_visible(mh))
                {
                    //describe_monster(currlev->monster_at(mappos));
                    mh.snapc()->get_name(&name, 0);
                    print_msg(MSGCHAN_PROMPT, "%s\n", name.c_str());
                }
                if (oh.valid())
                {
                    oh.snapc()->get_name(&name);
                    print_msg(MSGCHAN_PROMPT, "%s\n", name.c_str());
                }
                print_msg(MSGCHAN_PROMPT, "%s\n", terrain_data[currlev->terrain_at(mappos)].name);
            }
        }
        else
        {
            if (currlev->outofbounds(mappos + step))
            {
                continue;
            }
            libmrl::Coord tmp_spos = screenpos + step;
            if ((tmp_spos.y < 0) || (tmp_spos.x < 0) ||
                (tmp_spos.y >= DISP_HEIGHT) || (tmp_spos.x >= DISP_WIDTH))
            {
                continue;
            }
            screenpos = tmp_spos;
            mappos = mappos + step;
            wmove(world_window, screenpos.y, screenpos.x);
            wrefresh(world_window);
        }
    }
    print_msg(MSGCHAN_PROMPT, "Done.\n");
}

void print_version(void)
{
    print_msg(0, "You are using Martin's Dungeon Bash version %s\n", LONG_VERSION);
}

/* display.cc */
