/*
 * graphics.c - Pictures and animations
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#include "level9.h"

#include "main.h"
#include "config.h"
#include "gui.h"
#include "graphics.h"

#define ANIMATION_INTERVAL 20

extern L9BYTE *startdata;
extern L9UINT32 FileSize;
extern void show_picture (int pic);

static int graphicsMode = 0;
static gchar *graphicsDir = NULL;
static BitmapType bitmapType = NO_BITMAPS;

static int currentBitmap = -1;

static gboolean interactedWithBitmap = FALSE;

static GtkCssProvider *graphics_bg_provider = NULL;

static void display_picture (void);
static guchar apply_gamma (guchar colour, double gamma);

/* ------------------------------------------------------------------------- *
 * Utility functions.                                                        *
 * ------------------------------------------------------------------------- */

typedef struct
{
    guchar red;
    guchar green;
    guchar blue;
} palEntry;

static const palEntry basePalette[] =
{
    { 0x00, 0x00, 0x00 }, /* Black  */
    { 0xFF, 0x00, 0x00 }, /* Red    */
    { 0x30, 0xE8, 0x30 }, /* Green  */
    { 0xFF, 0xFF, 0x00 }, /* Yellow */
    { 0x00, 0x00, 0xFF }, /* Blue   */
    { 0xA0, 0x68, 0x00 }, /* Brown  */
    { 0x00, 0xFF, 0xFF }, /* Cyan   */
    { 0xFF, 0xFF, 0xFF }  /* White  */
};

static palEntry imagePalette[32];

static guchar *frameBuffer = NULL;
static guchar *rgbBuffer = NULL;
static int frameWidth = 0;
static int frameHeight = 0;
static int bitmapWidth = 0;
static int bitmapHeight = 0;

static GdkPixbuf *pixbuf = NULL;

static guint animationTimer = 0;
static guint animationCost = 0;

static GdkPixbuf *erik_panel = NULL;
static gboolean erik_capturing = FALSE;

/*
 * Apply gamma-correction to a colour component. There are apparently several
 * ways of doing this, but the output from ImageMagick's algorithm looks much
 * better to me than the output from Windows Magnetic's algorithm. At least
 * when compensating for the rather dark pictures in Corruption.
 */

static guchar apply_gamma (guchar colour, double gamma)
{
#if 1
    /* Gamma correction, the way ImageMagick does it. */
    return (guchar)
	CLAMP (pow ((double) colour / 255.0, 1.0 / gamma) * 255.0, 0.0, 255.0);
#else
    /* Gamma correction, the way Windows Magnetic does it. */
    return (guchar)
	CLAMP (sqrt ((double) colour * (double) colour * gamma), 0.0, 255.0);
#endif
}

static void on_picture_area_size_allocate (
    GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
    if (Config.fit_to_window)
	display_picture ();
}

void graphics_init ()
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (imagePalette); i++)
    {
	imagePalette[i].red = 0;
	imagePalette[i].green = 0;
	imagePalette[i].blue = 0;
    }

    if (animationTimer)
    {
	g_source_remove (animationTimer);
	animationTimer = 0;
    }

    if (pixbuf)
    {
	g_object_unref (pixbuf);
	pixbuf = NULL;
    }

    g_free (frameBuffer);
    g_free (rgbBuffer);

    frameBuffer = NULL;
    rgbBuffer = NULL;

    frameWidth = 0;
    frameHeight = 0;
    bitmapWidth = 0;
    bitmapHeight = 0;

    bitmapType = NO_BITMAPS;

    currentBitmap = -1;
    interactedWithBitmap = FALSE;

    if (erik_panel)
    {
	g_object_unref (erik_panel);
	erik_panel = NULL;
    }
    erik_capturing = FALSE;

    g_signal_connect (Gui.picture_area, "size-allocate",
	G_CALLBACK (on_picture_area_size_allocate), NULL);
}

static gboolean is_erik (void)
{
    int i;
    if (!startdata || FileSize < 30)
	return FALSE;
    int is_version2 = (startdata[4] == 0x20 && startdata[5] == 0x00)
	&& (startdata[10] == 0x00 && startdata[11] == 0x80)
	&& startdata[20] == startdata[22] && startdata[21] == startdata[23];
    L9UINT16 game_length = is_version2
	? (startdata[28] | (startdata[29] << 8))
	: (startdata[0] | (startdata[1] << 8));
    if (game_length != 0x34b3 || game_length >= FileSize)
	return FALSE;
    L9BYTE checksum = 0;
    for (i = 0; i < game_length + 1; i++)
	checksum += startdata[i];
    return checksum == 0x53;
}

static void erik_capture (void)
{
    if (erik_panel || erik_capturing || !is_erik ())
	return;
    erik_capturing = TRUE;
    guchar *old_frame = frameBuffer, *old_rgb = rgbBuffer;
    int old_frame_width = frameWidth, old_frame_height = frameHeight;
    int old_bitmap_width = bitmapWidth, old_bitmap_height = bitmapHeight;
    BitmapType old_bitmap_type = bitmapType;
    palEntry saved_palette[32];
    memcpy (saved_palette, imagePalette, sizeof (imagePalette));
    guchar *saved_frame = NULL;
    int saved_size = 0;
    if (old_frame && old_frame_width > 0 && old_frame_height > 0)
    {
	saved_size = old_frame_width * old_frame_height;
	saved_frame = (guchar *) g_malloc (saved_size);
	memcpy (saved_frame, old_frame, saved_size);
    }
    guchar *temp_frame = (guchar *) g_malloc0 (20480), *temp_rgb = (guchar *) g_malloc (61440);
    frameBuffer = temp_frame;
    rgbBuffer = temp_rgb;
    frameWidth = bitmapWidth = 160;
    frameHeight = bitmapHeight = 128;
    show_picture (500);
    while (RunGraphics ());
    {
	int i, j;
	guchar *ptr;
	for (i = 0; i < frameHeight; i++)
	{
	    ptr = rgbBuffer + i * frameWidth * 3;
	    for (j = 0; j < frameWidth; j++)
	    {
		guchar c = frameBuffer[i * frameWidth + j];
		guchar r = imagePalette[c].red;
		guchar g = imagePalette[c].green;
		guchar b = imagePalette[c].blue;
		if (b > 200 && g > 200 && r < 50)
		{
		    r = basePalette[3].red;
		    g = basePalette[3].green;
		    b = basePalette[3].blue;
		}
		*ptr++ = r;
		*ptr++ = g;
		*ptr++ = b;
	    }
	}
    }
    erik_panel = gdk_pixbuf_new_from_data (
	rgbBuffer, GDK_COLORSPACE_RGB, FALSE, 8, 160, 128, 3 * 160, NULL, NULL);
    if (erik_panel)
	erik_panel = gdk_pixbuf_copy (erik_panel);
    erik_capturing = FALSE;
    frameBuffer = old_frame;
    rgbBuffer = old_rgb;
    frameWidth = old_frame_width;
    frameHeight = old_frame_height;
    bitmapWidth = old_bitmap_width;
    bitmapHeight = old_bitmap_height;
    bitmapType = old_bitmap_type;
    g_free (temp_frame);
    g_free (temp_rgb);
    g_clear_object (&pixbuf);
    if (old_frame)
    {
    show_picture (635);
	if (saved_frame && frameWidth == old_frame_width && frameHeight == old_frame_height)
	    memcpy (frameBuffer, saved_frame, saved_size);
	memcpy (imagePalette, saved_palette, sizeof (imagePalette));
	g_free (saved_frame);
    }
}


static void display_picture ()
{
    palEntry pal[32];
    guchar *ptr;
    int i, j;

    if (!frameBuffer)
	return;

    if (pixbuf)
	g_object_unref (pixbuf);

    if (bitmapWidth == 0 || bitmapHeight == 0)
	return;

    if (is_erik () && !erik_panel)
	erik_capture ();

    for (i = 0; i < G_N_ELEMENTS (pal); i++)
    {
	pal[i].red = apply_gamma (imagePalette[i].red, Config.red_gamma);
	pal[i].green = apply_gamma (imagePalette[i].green, Config.green_gamma);
	pal[i].blue = apply_gamma (imagePalette[i].blue, Config.blue_gamma);
    }

    for (i = 0; i < frameHeight; i++)
    {
	ptr = rgbBuffer + i * frameWidth * 3;
	for (j = 0; j < frameWidth; j++)
	{
	    guchar c = frameBuffer[i * frameWidth + j];

	    *ptr++ = pal[c].red;
	    *ptr++ = pal[c].green;
	    *ptr++ = pal[c].blue;
	}
    }

    pixbuf = gdk_pixbuf_new_from_data (
	rgbBuffer, GDK_COLORSPACE_RGB, FALSE, 8, bitmapWidth, bitmapHeight,
	3 * frameWidth, NULL, NULL);

    gdouble scale = 1.0;
    if (!erik_capturing && Config.fit_to_window)
    {
	GtkAllocation a;
	gtk_widget_get_allocation (Gui.picture_area, &a);
	gdouble total_width = (is_erik () && erik_panel)
	    ? bitmapWidth * (1.0 + 364.0 / 598.0)
	    : bitmapWidth;
	gdouble ws = (gdouble) a.width / total_width;
	gdouble hs = (gdouble) a.height / bitmapHeight;
	scale = MIN (ws, hs);
    }
    else if (!erik_capturing && Config.image_scale != 1.0)
	scale = Config.image_scale;

    if (scale != 1.0)
    {
	GdkPixbuf *s = gdk_pixbuf_scale_simple (
	    pixbuf,
	    MAX ((gint) ((gdouble) bitmapWidth * scale + 0.5), 1),
	    MAX ((gint) ((gdouble) bitmapHeight * scale + 0.5), 1),
	    Config.image_filter);
	g_object_unref (pixbuf);
	pixbuf = s;
    }

    if (is_erik () && erik_panel)
    {
	int pw = gdk_pixbuf_get_width (pixbuf);
	int ph = gdk_pixbuf_get_height (pixbuf);
	int panel_w = MAX (1, (int)(pw * 0.2977 + 0.5));
	int bar_w = MAX (1, (int)(pw * 0.0067 + 0.5));
	double scale = MAX ((double)panel_w / 160.0, (double)ph / 128.0);
	int scaled_w = MAX (1, (int)(160 * scale + 0.5));
	int scaled_h = MAX (1, (int)(128 * scale + 0.5));
	GdkPixbuf *scaled = gdk_pixbuf_scale_simple (erik_panel, scaled_w, scaled_h, Config.image_filter);
	if (!scaled)
	    return;
	GdkPixbuf *panel = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, panel_w, ph);
	if (!panel)
	{
	    g_object_unref (scaled);
	    return;
	}
	guint32 yellow = (basePalette[3].red << 24) | (basePalette[3].green << 16) | 
			 (basePalette[3].blue << 8) | 0xFF;
	gdk_pixbuf_fill (panel, yellow);
	gdk_pixbuf_copy_area (scaled,
	    (scaled_w > panel_w) ? (scaled_w - panel_w) / 2 : 0,
	    (scaled_h > ph) ? (scaled_h - ph) / 2 : 0,
	    MIN (scaled_w, panel_w), MIN (scaled_h, ph),
	    panel,
	    (panel_w > scaled_w) ? (panel_w - scaled_w) / 2 : 0,
	    (ph > scaled_h) ? (ph - scaled_h) / 2 : 0);
	g_object_unref (scaled);
	GdkPixbuf *bar = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, bar_w, ph);
	if (!bar)
	{
	    g_object_unref (panel);
	    return;
	}
	gdk_pixbuf_fill (bar, 0);
	GdkPixbuf *comp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
	    panel_w + bar_w + pw + bar_w + panel_w, ph);
	if (comp)
	{
	    gdk_pixbuf_fill (comp, 0);
	    gdk_pixbuf_copy_area (panel, 0, 0, panel_w, ph, comp, 0, 0);
	    gdk_pixbuf_copy_area (bar, 0, 0, bar_w, ph, comp, panel_w, 0);
	    gdk_pixbuf_copy_area (pixbuf, 0, 0, pw, ph, comp, panel_w + bar_w, 0);
	    gdk_pixbuf_copy_area (bar, 0, 0, bar_w, ph, comp, panel_w + bar_w + pw, 0);
	    gdk_pixbuf_copy_area (panel, 0, 0, panel_w, ph, comp, panel_w + bar_w + pw + bar_w, 0);
	    g_object_unref (pixbuf);
	    pixbuf = comp;
	}
	g_object_unref (panel);
	g_object_unref (bar);
    }

    gtk_image_set_from_pixbuf (GTK_IMAGE (Gui.picture), pixbuf);
    gtk_widget_queue_draw (Gui.picture);
}

static gboolean graphics_callback (gpointer user_data)
{
    int result = TRUE;

    if (applicationExiting)
    {
	animationTimer = 0;
	return FALSE;
    }
    
    animationCost = 0;
    while (animationCost < Config.animation_speed)
    {
	result = RunGraphics ();

	if (!result)
	{
	    animationTimer = 0;
	    break;
	}
    }

    display_picture ();
    return result;
}

void graphics_run ()
{
    if (graphicsMode == 1)
    {
	if (Config.animate_images)
	{
	    /*
	     * The graphics will be redrawn several times per second. Using an
	     * expensive scaler will really, really slow things down here. To
	     * ensure tha the GUI does not bog down completely under the load,
	     * we make sure that this timer has a lower priority than anything
	     * GTK+ itself does.
	     */
	    if (!animationTimer)
		animationTimer = g_timeout_add_full (
		    G_PRIORITY_LOW, ANIMATION_INTERVAL, graphics_callback,
		    NULL, NULL);
	} else
	{
	    RunGraphics ();
	    while (RunGraphics ())
		;
	    display_picture ();
	}
    }
}

void graphics_set_directory (gchar *dir)
{
    graphicsDir = dir;
    bitmapType = DetectBitmaps (dir);
}

void graphics_refresh ()
{
    GdkRGBA colour;
    GtkWidget *viewport;
    GtkStyleContext *style_context;
    gchar *css;

    if (animationTimer)
    {
	g_source_remove (animationTimer);
	animationTimer = g_timeout_add_full (
	    G_PRIORITY_LOW, ANIMATION_INTERVAL, graphics_callback, NULL, NULL);
    }

    display_picture ();

    /*
     * The picture's parent widget is, I believe, an automagically created
     * viewport thingy, so we don't have any handle to it.
     */
    viewport = gtk_widget_get_parent (Gui.picture);
    style_context = gtk_widget_get_style_context (viewport);

    if (graphics_bg_provider) {
	gtk_style_context_remove_provider (style_context,
		GTK_STYLE_PROVIDER (graphics_bg_provider));
	g_object_unref (graphics_bg_provider);
	graphics_bg_provider = NULL;
    }

    if (Config.graphics_bg && gdk_rgba_parse (&colour, Config.graphics_bg))
    {
	graphics_bg_provider = gtk_css_provider_new ();
	css = g_strdup_printf ("viewport { background-color: %s; }", Config.graphics_bg);
	gtk_css_provider_load_from_data (graphics_bg_provider, css, -1, NULL);
	gtk_style_context_add_provider (style_context,
		GTK_STYLE_PROVIDER (graphics_bg_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free (css);
    }
}

static void cleanup_statusbar (guint context_id) {
    if (GTK_IS_WIDGET (Gui.statusbar)) {
	gtk_widget_hide (Gui.statusbar);
	if (GTK_IS_STATUSBAR (Gui.statusbar))
	    gtk_statusbar_pop (GTK_STATUSBAR (Gui.statusbar), context_id);
    }
}

/* ------------------------------------------------------------------------- *
 * Still pictures.                                                           *
 * ------------------------------------------------------------------------- */

void os_graphics (int mode)
{
    if (animationTimer)
    {
	g_source_remove (animationTimer);
	animationTimer = 0;
    }

    g_free (frameBuffer);
    g_free (rgbBuffer);
    frameBuffer = NULL;
    rgbBuffer = NULL;

    graphicsMode = mode;

    switch (graphicsMode)
    {
	case 1:
	    /* Line/Fill graphics */
	    GetPictureSize (&frameWidth, &frameHeight);
	    bitmapWidth = frameWidth;
	    bitmapHeight = frameHeight;
	    break;

	case 2:
	    /* Bitmap graphics */
	    if (bitmapType == NO_BITMAPS)
		return;

	    frameWidth = MAX_BITMAP_WIDTH;
	    frameHeight = MAX_BITMAP_HEIGHT;
	    break;

	default:
	    /* No graphics */
	    return;
    }

    if (frameWidth == 0 || frameHeight == 0)
	return;

    frameBuffer = (guchar *) g_malloc0 (frameHeight * frameWidth);
    rgbBuffer = (guchar *) g_malloc (frameHeight * frameWidth * 3);
}

void os_cleargraphics ()
{
    if (!frameBuffer)
	return;

    memset (frameBuffer, 0, frameWidth * frameHeight);
}

void os_setcolour (int colour, int index)
{
    if (colour < 0 || colour >= G_N_ELEMENTS (imagePalette))
	return;

    if (index < 0 || index >= G_N_ELEMENTS (basePalette))
	return;

    imagePalette[colour].red = basePalette[index].red;
    imagePalette[colour].green = basePalette[index].green;
    imagePalette[colour].blue = basePalette[index].blue;
}

static void plot (int x, int y, int colour1, int colour2)
{
    if (x < 0 || x >= frameWidth || y < 0 || y >= frameHeight)
	return;

    if (frameBuffer[y * frameWidth + x] == colour2)
	frameBuffer[y * frameWidth + x] = colour1;
}

void os_drawline (int x1, int y1, int x2, int y2, int colour1, int colour2)
{
    /* Bresenham's line algorithm, as described by Wikipedia */
    int delta_x, delta_y;
    int x_step, y_step;
    int err, delta_err;
    int x, y;

    animationCost++;

    gboolean steep = abs (y2 - y1) > abs (x2 - x1);

    if (steep)
    {
	int tmp;

	tmp = x1;
	x1 = y1;
	y1 = tmp;

	tmp = x2;
	x2 = y2;
	y2 = tmp;
    }

    delta_x = abs (x2 - x1);
    delta_y = abs (y2 - y1);
    err = 0;
    delta_err = delta_y;
    x = x1;
    y = y1;

    x_step = (x1 < x2) ? 1 : -1;
    y_step = (y1 < y2) ? 1 : -1;

    if (steep)
	plot (y, x, colour1, colour2);
    else
	plot (x, y, colour1, colour2);

    while (x != x2)
    {
	x += x_step;
	err += delta_err;

	if (2 * err > delta_x)
	{
	    y += y_step;
	    err -= delta_x;
	}

	if (steep)
	    plot (y, x, colour1, colour2);
	else
	    plot (x, y, colour1, colour2);
    }
}

/*
 * Filling is a lot harder to get right than it looks. I made several attempts
 * at adapting algorithms from Interactive Computer Graphics by Peter Burger
 * and Duncan Gillies, before taking the more practical route and adapting the
 * one found in Bill Kendrick's TuxPaint. Hopefully this one will work...
 */

void os_fill (int x, int y, int colour1, int colour2)
{
    int left, right;
    int i;

    animationCost++;

    if (colour1 == colour2)
	return;
    
    if (x < 0 || x >= frameWidth || y < 0 || y >= frameHeight)
	return;

    if (frameBuffer[y * frameWidth + x] != colour2)
	return;

    /* Find left side, filling along the way */

    left = x;

    while (left >= 0 && frameBuffer[y * frameWidth + left] == colour2)
    {
	frameBuffer[y * frameWidth + left] = colour1;
	left--;
    }

    left++;

    /* Find right side, filling along the way */

    right = x + 1;
    
    while (right < frameWidth && frameBuffer[y * frameWidth + right] == colour2)
    {
	frameBuffer[y * frameWidth + right] = colour1;
	right++;
    }

    right--;

    for (i = left; i <= right; i++)
    {
	if (y - 1 >= 0 && frameBuffer[(y - 1) * frameWidth + i] == colour2)
	    os_fill (i, y - 1, colour1, colour2);

	if (y + 1 < frameHeight && frameBuffer[(y + 1) * frameWidth + i] == colour2)
	    os_fill (i, y + 1, colour1, colour2);
    }
}

void graphics_interact ()
{
    if (graphicsMode == 2)
	interactedWithBitmap = TRUE;
}

void os_show_bitmap (int pic, int x, int y)
{
    Bitmap *bitmap;
    int i;

    if (applicationExiting)
	return;

    if (pic == currentBitmap)
	return;

    bitmap = DecodeBitmap (graphicsDir, bitmapType, pic, x, y);

    if (!bitmap)
	return;

    guchar *initialFrameBuffer = frameBuffer;
    int initialFrameWidth = frameWidth;
    int initialFrameHeight = frameHeight;

    if (currentBitmap != -1 && !interactedWithBitmap)
    {
	if (!GTK_IS_WIDGET (Gui.statusbar))
	return;

	guint context_id;

	context_id = gtk_statusbar_get_context_id (
	    GTK_STATUSBAR (Gui.statusbar), "os_show_bitmap");

	gtk_widget_show (Gui.statusbar);
	gtk_statusbar_push (
	    GTK_STATUSBAR (Gui.statusbar), context_id, "Press any key...");

	os_flush ();
	os_readchar (5000);

	if (frameBuffer != initialFrameBuffer || 
	    frameWidth != initialFrameWidth || 
	    frameHeight != initialFrameHeight) {
	    cleanup_statusbar (context_id);
	    return;
	}

	if (applicationExiting) {
	    cleanup_statusbar (context_id);
	    return;
	}

	cleanup_statusbar (context_id);
    }

    if (applicationExiting)
	return;

    if (frameBuffer != initialFrameBuffer || 
	frameWidth != initialFrameWidth || 
	frameHeight != initialFrameHeight)
	return;

    if (!frameBuffer || !bitmap->bitmap)
	return;

    if (frameWidth <= 0 || frameHeight <= 0 || 
	bitmap->width <= 0 || bitmap->height <= 0)
	return;

    interactedWithBitmap = FALSE;
    currentBitmap = pic;

    bitmapWidth = bitmap->width;
    bitmapHeight = bitmap->height;

    for (i = 0; i < bitmap->npalette; i++)
    {
	imagePalette[i].red = bitmap->palette[i].red;
	imagePalette[i].green = bitmap->palette[i].green;
	imagePalette[i].blue = bitmap->palette[i].blue;
    }

    for (i = 0; i < bitmap->height; i++)
    {
	if ((i + 1) * frameWidth > frameWidth * frameHeight ||
	    (i + 1) * bitmap->width > bitmap->width * bitmap->height)
	    return;
	memcpy (frameBuffer + i * frameWidth,
		bitmap->bitmap + i * bitmap->width,
		bitmap->width);
    }

    display_picture ();
}

L9BOOL os_find_file(char *NewName)
{
    char *fname, *p;
    FILE *f;

    f = fopen(NewName, "rb");
    if (f != NULL)
    {
	fclose(f);
	return TRUE;
    }

    fname = strrchr(NewName, '/');
    if (fname != NULL)
	++fname;
    else
	fname = NewName;

    for (p = fname; *p != '\0'; ++p)
	*p = toupper(*p);

    f = fopen(NewName, "rb");
    if (f != NULL)
    {
	fclose(f);
	return TRUE;
    }

    for (p = fname; *p != '\0'; ++p)
	*p = tolower(*p);

    f = fopen(NewName, "rb");
    if (f != NULL)
    {
	fclose(f);
	return TRUE;
    }

    return FALSE;
}

