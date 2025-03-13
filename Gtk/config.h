/*
 * config.h - Storing to and retrieving from the configuration file
 * Copyright (c) 2005 Torbjörn Andersson <d91tan@Update.UU.SE>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/*
 * Define this if GtkLevel9 should try to make the file name of the data file
 * absolute before starting the game. Since this filename is stored in saved
 * games, this is useful if you specify the filename from the command-line,
 * rather than through a file selector.
 *
 * However, the implementation assumes that "." and ".." means the same thing
 * in all file systems, and I simply do not know if this is really the case.
 */

#define MAKE_FILENAMES_ABSOLUTE

/*
 * Note that this is the number of lines in the text buffer, not the text view.
 * Since this distinction is probably not obvious to the average user, I have
 * not made it configurable. Instead, let's just make it fairly large.
 */
#define MAX_SCROLLBACK 500

/*
 * This is stupidly small, but on the off-chance that some user wants to be
 * able to play on a 320x240 screen...
 */

#define MIN_WINDOW_WIDTH 300
#define MIN_WINDOW_HEIGHT 200

/*
 * It's probably more efficient to run a couple of instruction every time the
 * application is idle. This defines how many.
 */

#define MAX_INSTRUCTIONS 20

typedef struct
{
    gint window_width;
    gint window_height;
    gint window_split;

    gchar *text_font;
    gint text_margin;
    gdouble text_leading;
    gchar *text_fg;
    gchar *text_bg;
    gchar *graphics_bg;

    gint image_filter;     /* Should really be GdkInterpType, not gint */
    gint graphics_position;
    gboolean fit_to_window;
    gdouble image_scale;
    gdouble red_gamma;
    gdouble green_gamma;
    gdouble blue_gamma;
    gboolean animate_images;
    gint animation_speed;
} Configuration;

extern Configuration Config;

void read_config_file (void);
void write_config_file (void);
void do_config (void);

#endif
