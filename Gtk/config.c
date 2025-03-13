/*
 * config.c - Storing to and retrieving from the configuration file
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

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "level9.h"

#include "config.h"
#include "gui.h"
#include "text.h"
#include "graphics.h"

#define CONFIG_VERSION_MAJOR  1
#define CONFIG_VERSION_MINOR  0

/* ------------------------------------------------------------------------- *
 * The configuration data structure. There is no guarantee that all of these *
 * settings will be up-to-date at all times. In particular, I haven't found  *
 * any way of detecting when the window partition changes.                   *
 * ------------------------------------------------------------------------- */

Configuration Config =
{
    2 * MIN_WINDOW_WIDTH,   /* window width                         */
    2 * MIN_WINDOW_HEIGHT,  /* window height                        */
    150,                    /* partition                            */
    NULL,                   /* text font                            */
    20,                     /* text margin                          */
    1.0,                    /* text leading                         */
    NULL,                   /* text foreground colour               */
    NULL,                   /* text background colour               */
    NULL,                   /* graphics background colour           */
    GDK_INTERP_BILINEAR,    /* interpolation mode for image scaling */
    0,                      /* graphics position                    */
    TRUE,                   /* fit to window                        */
    1.0,                    /* image scaling                        */
    1.0,                    /* red gamma                            */
    1.0,                    /* green gamma                          */
    1.0,                    /* blue gamma                           */
    TRUE,                   /* animate images                       */
    10                      /* animation speed                      */
};

/* ------------------------------------------------------------------------- *
 * Utility functions                                                         *
 * ------------------------------------------------------------------------- */

static gchar *get_config_filename ()
{
    gchar *new_path = g_build_filename (
	g_get_user_config_dir (), "gtklevel9", NULL);
    if (!g_file_test (new_path, G_FILE_TEST_EXISTS)) {
	gchar *old_path = g_build_filename (
	    g_get_home_dir (), ".gtklevel9", NULL);
	if (g_file_test (old_path, G_FILE_TEST_EXISTS)) {
	    g_mkdir_with_parents (g_get_user_config_dir (), 0755);
	    g_rename (old_path, new_path);
	}
	g_free (old_path);
    }
    return new_path;
}

/* ------------------------------------------------------------------------- *
 * Configuration file writer.                                                *
 * ------------------------------------------------------------------------- */

void write_config_file ()
{
    gchar *filename;
    GIOChannel *file;
    GError *error = NULL;

    /*
     * HACK: As mentioned before, I don't know how to detect when the window
     * partition changes, so this is the only opportunity to make sure the
     * setting is up to date.
     */
    Config.window_split = gtk_paned_get_position (GTK_PANED (Gui.partition));

    filename = get_config_filename ();
    file = g_io_channel_new_file (filename, "w", &error);
    g_free (filename);

    if (file)
    {
	gchar *buf;
	gsize bytes_written;

	buf = g_strdup_printf (
	    "<?xml version=\"1.0\"?>\n\n"
	    "<configuration version=\"%d.%d\">\n"
	    "  <layout>\n"
	    "    <main_window>\n"
	    "      <width>%d</width>\n"
	    "      <height>%d</height>\n"
	    "      <split>%d</split>\n"
	    "    </main_window>\n"
	    "  </layout>\n\n"
	    
	    "  <text>\n"
	    "    <font>%s</font>\n"
	    "    <margin>%d</margin>\n"
	    "    <leading>%.2f</leading>\n"
	    "    <foreground>%s</foreground>\n"
	    "    <background>%s</background>\n"
	    "  </text>\n\n"
	    
	    "  <graphics>\n"
	    "    <graphics_position>%d</graphics_position>\n"
	    "    <fit_to_window>%s</fit_to_window>\n"
	    "    <scale>%.2f</scale>\n"
	    "    <filter>%d</filter>\n"
	    "    <gamma>\n"
	    "      <red>%.2f</red>\n"
	    "      <green>%.2f</green>\n"
	    "      <blue>%.2f</blue>\n"
	    "    </gamma>\n"
	    "    <background>%s</background>\n"
	    "    <animate>%s</animate>\n"
	    "    <speed>%d</speed>\n"
	    "  </graphics>\n"
	    "</configuration>\n",
	    CONFIG_VERSION_MAJOR,
	    CONFIG_VERSION_MINOR,
	    Config.window_width,
	    Config.window_height,
	    Config.window_split,
	    Config.text_font ? Config.text_font : "",
	    Config.text_margin,
	    Config.text_leading,
	    Config.text_fg ? Config.text_fg : "",
	    Config.text_bg ? Config.text_bg : "",
	    Config.graphics_position,
	    Config.fit_to_window ? "TRUE" : "FALSE",
	    Config.image_scale,
	    Config.image_filter,
	    Config.red_gamma,
	    Config.green_gamma,
	    Config.blue_gamma,
	    Config.graphics_bg ? Config.graphics_bg : "",
	    Config.animate_images ? "TRUE" : "FALSE",
	    Config.animation_speed);

	g_io_channel_write_chars (file, buf, -1, &bytes_written, &error);
	g_free (buf);
	g_io_channel_unref (file);
    }
}

/* ------------------------------------------------------------------------- *
 * Configuration file reader/parser. The parser_state variable decides which *
 * tags are currently expected, and one of the parser* variables is set to   *
 * point at the location where the parsed value should be stored. Unknown    *
 * tags are silently ignored.                                                *
 *                                                                           *
 * This makes it very simple to add new tags or even to change the expected  *
 * layout of the configuration file. I'm quite pleased with the way this     *
 * code turned out.                                                          *
 * ------------------------------------------------------------------------- */

static enum
{
    CONFIG_PARSE_TOPLEVEL,
    CONFIG_PARSE_CONFIGURATION,
    CONFIG_PARSE_LAYOUT,
    CONFIG_PARSE_LAYOUT_MAIN_WINDOW,
    CONFIG_PARSE_TEXT,
    CONFIG_PARSE_GRAPHICS,
    CONFIG_PARSE_GRAPHICS_GAMMA
} parser_state = CONFIG_PARSE_TOPLEVEL;

static gboolean *parserBool = NULL;
static gint *parserInt = NULL;
static gdouble *parserFloat = NULL;
static gchar **parserChar = NULL;

static void config_parse_reset ()
{
    parserBool = NULL;
    parserInt = NULL;
    parserFloat = NULL;
    parserChar = NULL;
}

/* Parse opening tag and attributes */

static void config_parse_start_element (GMarkupParseContext *context,
					const gchar *element_name,
					const gchar **attribute_names,
					const gchar **attribute_values,
					gpointer user_data,
					GError **error)
{
    config_parse_reset ();
    
    switch (parser_state)
    {
	case CONFIG_PARSE_TOPLEVEL:
	    if (strcmp (element_name, "configuration") == 0)
	    {
		gint version_major = 0;
		gint version_minor = 0;
		gint i;
		
		parser_state = CONFIG_PARSE_CONFIGURATION;

		for (i = 0; attribute_names[i]; i++)
		{
		    if (strcmp (attribute_names[i], "version") == 0)
			sscanf (attribute_values[i], "%d.%d", &version_major,
				&version_minor);
		}

		if (version_major != CONFIG_VERSION_MAJOR ||
		    version_minor != CONFIG_VERSION_MINOR)
		{
		    g_message (
			"Config version mismatch. Expected %d.%d, found %d.%d",
			CONFIG_VERSION_MAJOR, CONFIG_VERSION_MINOR,
			version_major, version_minor);
		    if (version_major == CONFIG_VERSION_MAJOR &&
			version_minor <= CONFIG_VERSION_MINOR)
			g_message ("This should be quite harmless.");
		    else
			g_message (
			    "Some of your settings have probably been lost.");
		}
	    }
	    break;

	case CONFIG_PARSE_CONFIGURATION:
	    if (strcmp (element_name, "layout") == 0)
		parser_state = CONFIG_PARSE_LAYOUT;
	    else if (strcmp (element_name, "text") == 0)
		parser_state = CONFIG_PARSE_TEXT;
	    else if (strcmp (element_name, "graphics") == 0)
		parser_state = CONFIG_PARSE_GRAPHICS;
	    break;

	case CONFIG_PARSE_LAYOUT:
	    if (strcmp (element_name, "main_window") == 0)
		parser_state = CONFIG_PARSE_LAYOUT_MAIN_WINDOW;
	    break;

	case CONFIG_PARSE_LAYOUT_MAIN_WINDOW:
	    if (strcmp (element_name, "width") == 0)
		parserInt = &(Config.window_width);
	    else if (strcmp (element_name, "height") == 0)
		parserInt = &(Config.window_height);
	    else if (strcmp (element_name, "split") == 0)
		parserInt = &(Config.window_split);
	    break;

	case CONFIG_PARSE_TEXT:
	    if (strcmp (element_name, "font") == 0)
		parserChar = &(Config.text_font);
	    else if (strcmp (element_name, "background") == 0)
		parserChar = &(Config.text_bg);
	    else if (strcmp (element_name, "foreground") == 0)
		parserChar = &(Config.text_fg);
	    else if (strcmp (element_name, "margin") == 0)
		parserInt = &(Config.text_margin);
	    else if (strcmp (element_name, "leading") == 0)
		parserFloat = &(Config.text_leading);
	    break;

	case CONFIG_PARSE_GRAPHICS:
	    if (strcmp (element_name, "graphics_position") == 0)
		parserInt = &(Config.graphics_position);
	    else if (strcmp (element_name, "fit_to_window") == 0)
		parserBool = &(Config.fit_to_window);
	    else if (strcmp (element_name, "scale") == 0)
		parserFloat = &(Config.image_scale);
	    else if (strcmp (element_name, "filter") == 0)
		parserInt = &(Config.image_filter);
	    else if (strcmp (element_name, "gamma") == 0)
		parser_state = CONFIG_PARSE_GRAPHICS_GAMMA;
	    else if (strcmp (element_name, "background") == 0)
		parserChar = &(Config.graphics_bg);
	    else if (strcmp (element_name, "animate") == 0)
		parserBool = &(Config.animate_images);
	    else if (strcmp (element_name, "speed") == 0)
		parserInt = &(Config.animation_speed);
	    break;

	case CONFIG_PARSE_GRAPHICS_GAMMA:
	    if (strcmp (element_name, "red") == 0)
		parserFloat = &(Config.red_gamma);
	    else if (strcmp (element_name, "green") == 0)
		parserFloat = &(Config.green_gamma);
	    else if (strcmp (element_name, "blue") == 0)
		parserFloat = &(Config.blue_gamma);
	    break;

	default:
	    break;
    }
}

/* Parse closing tag */

static void config_parse_end_element (GMarkupParseContext *context,
				      const gchar *element_name,
				      gpointer user_data,
				      GError **error)
{
    config_parse_reset ();

    switch (parser_state)
    {
	case CONFIG_PARSE_CONFIGURATION:
	    if (strcmp (element_name, "configuration") == 0)
		parser_state = CONFIG_PARSE_TOPLEVEL;
	    break;

	case CONFIG_PARSE_LAYOUT:
	    if (strcmp (element_name, "layout") == 0)
		parser_state = CONFIG_PARSE_CONFIGURATION;
	    break;

	case CONFIG_PARSE_LAYOUT_MAIN_WINDOW:
	    if (strcmp (element_name, "main_window") == 0)
		parser_state = CONFIG_PARSE_LAYOUT;
	    break;

	case CONFIG_PARSE_TEXT:
	    if (strcmp (element_name, "text") == 0)
		parser_state = CONFIG_PARSE_CONFIGURATION;
	    break;
	    
	case CONFIG_PARSE_GRAPHICS:
	    if (strcmp (element_name, "graphics") == 0)
		parser_state = CONFIG_PARSE_CONFIGURATION;
	    break;

	case CONFIG_PARSE_GRAPHICS_GAMMA:
	    if (strcmp (element_name, "gamma") == 0)
		parser_state = CONFIG_PARSE_GRAPHICS;
	    break;

	default:
	    break;
    }
}

/* Parse text between tags */

static void config_parse_text (GMarkupParseContext *context,
			       const gchar *text,
			       gsize text_len,
			       gpointer user_data,
			       GError **error)
{
    if (parserBool)
	*parserBool = (strcmp (text, "TRUE") == 0) ? TRUE : FALSE;
    else if (parserInt)
	*parserInt = (gint) g_ascii_strtod (text, NULL);
    else if (parserFloat) {
	gchar *text_copy = g_strdup (text);
	gchar *p;
	for (p = text_copy; *p; p++) {
	    if (*p == ',') *p = '.';
	}
	*parserFloat = g_ascii_strtod (text_copy, NULL);
	g_free (text_copy);
    }
    else if (parserChar)
    {
	if (*parserChar)
	    g_free (*parserChar);
	if (strlen (text) > 0)
	    *parserChar = g_strdup (text);
    }
}

/*
 * Error handling. We really ought to do something more useful here, but it
 * shouldn't ever happen...
 */

static void config_parse_error (GMarkupParseContext *context,
				GError *error,
				gpointer user_data)
{
    g_warning ("Config file parser error: %s", error->message);
}

void read_config_file ()
{
    GMarkupParseContext *context;
    GMarkupParser parser =
	{
	    config_parse_start_element,
	    config_parse_end_element,
	    config_parse_text,
	    NULL,
	    config_parse_error
	};
    gchar *filename;
    gchar *text;
    gsize length;
    GError *error = NULL;

    filename = get_config_filename ();
    g_file_get_contents (filename, &text, &length, &error);
    g_free (filename);

    if (text)
    {
	context = g_markup_parse_context_new (&parser, 0, NULL, NULL);
	g_markup_parse_context_parse (context, text, -1, &error);
	g_markup_parse_context_end_parse (context, &error);
	g_markup_parse_context_free (context);

	if (parser_state != CONFIG_PARSE_TOPLEVEL)
	    g_warning ("Unexpected parser state at end of config file");
    }

    /* Apply the settings from the configuration file */

    text_refresh ();
    graphics_refresh ();
    gui_refresh ();
}

/* ------------------------------------------------------------------------- *
 * Configuration window.                                                     *
 * ------------------------------------------------------------------------- */

static void toggle_sensitivity (GtkToggleButton *toggle_button,
    gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET (user_data);
    gtk_widget_set_sensitive (
	widget, gtk_toggle_button_get_active (toggle_button));
}

static void reset_font (GtkWidget *button, GtkWidget *font_button)
{
    GtkSettings *settings = gtk_settings_get_default ();
    gchar *font_name;
    g_object_get (settings, "gtk-font-name", &font_name, NULL);
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_button), font_name);
    g_free (font_name);
    Config.text_font = NULL;
    g_signal_emit_by_name (font_button, "font-set");
    text_refresh ();
}

static GtkWidget *add_font_button (GtkWidget *tab, gchar *text, gchar *title)
{
    GtkWidget *label;
    GtkWidget *font_button;
    GtkWidget *box;
    GtkWidget *reset;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX (tab), box, TRUE, TRUE, 0);

    label = gtk_label_new_with_mnemonic ("_Text font:");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (label), 0.5);
    gtk_widget_set_size_request (label, 120, -1);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    font_button = gtk_font_button_new ();
    gtk_font_button_set_use_font (GTK_FONT_BUTTON (font_button), TRUE);
    gtk_font_button_set_use_size (GTK_FONT_BUTTON (font_button), TRUE);
    gtk_font_button_set_title (GTK_FONT_BUTTON (font_button), title);
    gtk_box_pack_start (GTK_BOX (box), font_button, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), font_button);

    reset = gtk_button_new_from_icon_name ("view-refresh", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class (gtk_widget_get_style_context (reset), "flat");
    g_signal_connect (reset, "clicked", G_CALLBACK (reset_font), font_button);
    gtk_box_pack_start (GTK_BOX (box), reset, FALSE, FALSE, 0);

    return font_button;
}

static void reset_value (GtkWidget *button, GtkWidget *widget)
{
    gdouble *default_value = g_object_get_data (
	G_OBJECT (button), "default-value");
    if (GTK_IS_SPIN_BUTTON (widget))
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), *default_value);
    else
	gtk_range_set_value (GTK_RANGE (widget), *default_value);
}

static GtkWidget *add_reset_button (GtkWidget *widget, gdouble default_value)
{
    GtkWidget *button = gtk_button_new_from_icon_name (
	"view-refresh", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class (
	gtk_widget_get_style_context (button), "flat");
    gdouble *value = g_new (gdouble, 1);
    *value = default_value;
    g_object_set_data_full (G_OBJECT (button), "default-value", value, g_free);
    g_signal_connect (button, "clicked", G_CALLBACK (reset_value), widget);
    return button;
}

static GtkWidget *add_scale (GtkWidget *tab, gchar *text, gdouble min,
			   gdouble max, gdouble step, gdouble value,
			   gint digits, gdouble reset_value)
{
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *scale;
    GtkWidget *reset;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX (tab), box, TRUE, TRUE, 0);

    label = gtk_label_new_with_mnemonic (text);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (label), 0.5);
    gtk_widget_set_size_request (label, 120, -1);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
	min, max, step);
    gtk_scale_set_digits (GTK_SCALE (scale), digits);
    gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_RIGHT);
    gtk_range_set_value (GTK_RANGE (scale), value);
    gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), scale);

    reset = add_reset_button (scale, reset_value);
    gtk_box_pack_start (GTK_BOX (box), reset, FALSE, FALSE, 0);

    return scale;
}

typedef struct
{
    GtkWidget *checkbox;
    GtkWidget *button;
} ColourSetting;

ColourSetting *text_fg;
ColourSetting *text_bg;
ColourSetting *graphics_bg;

static void color_checkbox_changed (
    GtkToggleButton *toggle_button, gpointer user_data);

static ColourSetting *add_colour_setting (GtkWidget *tab, gchar *text,
					  gchar *title, gchar *colour_name)
{
    ColourSetting *s;
    GdkRGBA rgba;

    s = g_new (ColourSetting, 1);

    s->checkbox = gtk_check_button_new_with_mnemonic (text);
    gtk_box_pack_start (GTK_BOX (tab), s->checkbox, TRUE, TRUE, 0);

    s->button = gtk_color_button_new ();
    gtk_box_pack_start (GTK_BOX (tab), s->button, TRUE, TRUE, 0);
    gtk_color_button_set_title (GTK_COLOR_BUTTON (s->button), title);

    if (g_strcmp0 (text, "Override text _foreground colour") == 0) {
	gdk_rgba_parse (&rgba, "#000000");
    } else if (g_strcmp0 (text, "Override text _background colour") == 0) {
	gdk_rgba_parse (&rgba, "#FFFFFF");
    } else {
	gdk_rgba_parse (&rgba, "#F6F5F4");
    }
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER(s->button), &rgba);

    if (colour_name && gdk_rgba_parse (&rgba, colour_name))
    {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s->checkbox), TRUE);
	gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (s->button), &rgba);
    } else
	gtk_widget_set_sensitive (GTK_WIDGET (s->button), FALSE);

    g_signal_connect (G_OBJECT (s->checkbox), "toggled",
	G_CALLBACK (toggle_sensitivity), s->button);
    g_signal_connect (G_OBJECT (s->checkbox), "toggled",
	G_CALLBACK (color_checkbox_changed), s);

    return s;
}

static void update_colour_setting (gchar **colour, ColourSetting *s)
{
    if (*colour) {
	g_free (*colour);
	*colour = NULL;
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->checkbox)))
    {
	GdkRGBA rgba;
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (s->button), &rgba);
	*colour = g_strdup_printf ("#%02X%02X%02X",
			       (int) (rgba.red * 255),
			       (int) (rgba.green * 255),
			       (int) (rgba.blue * 255));
    }
}

static GtkWidget *image_scale;

static void on_scale_combo_changed (GtkComboBox *combo, gpointer user_data)
{
    GtkWidget *scale_box = GTK_WIDGET (user_data);
    int active = gtk_combo_box_get_active (combo);
    gtk_widget_set_sensitive (scale_box, active == 1);
    Config.fit_to_window = (active == 0);
    graphics_refresh ();
}

typedef struct
{
    const gchar *description;
    GdkInterpType interp_type;
} ComboBoxItem;

static const ComboBoxItem imageFilters[] = {
    /*
     * In reality, the value and index into this array are probably the same.
     * But I don't want to make that assumption.
     */
    { "Nearest neighbour", GDK_INTERP_NEAREST  },
    { "Tiles",             GDK_INTERP_TILES    },
    { "Bilinear",          GDK_INTERP_BILINEAR },
    { "Hyperbolic",        GDK_INTERP_HYPER    }
};

static int get_interp_type_index (GdkInterpType interp_type)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (imageFilters); i++)
	if (imageFilters[i].interp_type == interp_type)
	    return i;

    return -1;
}

static GtkWidget *add_combo_box (GtkWidget *tab, gchar *text,
				const gchar **items, gint n_items,
				gint active)
{
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *combo;
    gint i;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX (tab), box, TRUE, TRUE, 0);

    label = gtk_label_new_with_mnemonic (text);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (label), 0.5);
    gtk_widget_set_size_request (label, 120, -1);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    combo = gtk_combo_box_text_new ();
    for (i = 0; i < n_items; i++)
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), items[i]);
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);
    gtk_box_pack_start (GTK_BOX (box), combo, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

    return combo;
}

static gchar *format_animation_speed (
    GtkScale *scale, gdouble value, gpointer user_data)
{
    return g_strdup_printf ("%.0f   ", value);
}

static void scale_changed (GtkRange *range, gpointer user_data)
{
    if (user_data == &Config.animation_speed) {
	Config.animation_speed = (gint) gtk_range_get_value(range);
    } else {
	gdouble *target = (gdouble *)user_data;
	*target = gtk_range_get_value(range);
    }
    graphics_refresh ();
}

static void spin_changed_int (GtkSpinButton *spin, gpointer user_data)
{
    gint *target = (gint *) user_data;
    if (GTK_IS_SPIN_BUTTON (spin)) {
	*target = gtk_spin_button_get_value_as_int (spin);
	text_refresh ();
    }
}

static void spin_changed_float (GtkSpinButton *spin, gpointer user_data)
{
    gdouble *target = (gdouble *) user_data;
    if (GTK_IS_SPIN_BUTTON (spin)) {
	*target = gtk_spin_button_get_value (spin);
	text_refresh ();
    }
}

static void font_changed (GtkFontButton *button, gpointer user_data)
{
    const gchar *new_font =
	gtk_font_chooser_get_font (GTK_FONT_CHOOSER (button));
    if (new_font && g_utf8_validate (new_font, -1, NULL)) {
	gchar *temp = g_strdup (new_font);
	if (Config.text_font)
	    g_free (Config.text_font);
	Config.text_font = temp;
	text_refresh ();
    }
}

static void color_changed (GtkColorButton *button, gpointer user_data)
{
    gchar **target = (gchar **) user_data;
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
    if (*target)
	g_free (*target);
    *target = g_strdup_printf ("#%02X%02X%02X",
			    (int) (rgba.red * 255),
			    (int) (rgba.green * 255),
			    (int) (rgba.blue * 255));
    if (target == &Config.graphics_bg)
	graphics_refresh ();
    else
	text_refresh ();
}

static void on_filter_changed(GtkComboBox *combo, gpointer user_data)
{
    int filter_idx = gtk_combo_box_get_active(combo);
    if (filter_idx != -1)
	Config.image_filter = imageFilters[filter_idx].interp_type;
    else
	Config.image_filter = GDK_INTERP_BILINEAR;
    graphics_refresh ();
}

static void on_position_changed (GtkComboBox *combo, gpointer user_data)
{
    Config.graphics_position = gtk_combo_box_get_active (combo);
    gui_refresh();
    graphics_refresh();
}

static void toggle_changed (GtkToggleButton *toggle_button, gpointer user_data)
{
    gboolean *target = (gboolean *) user_data;
    *target = gtk_toggle_button_get_active (toggle_button);
    graphics_run ();
}

static void color_checkbox_changed (
    GtkToggleButton *toggle_button, gpointer user_data)
{
    ColourSetting *s = (ColourSetting *) user_data;

    if (s == text_fg) {
	update_colour_setting (&Config.text_fg, text_fg);
	text_refresh ();
    } else if (s == text_bg) {
	update_colour_setting (&Config.text_bg, text_bg);
	text_refresh ();
    } else if (s == graphics_bg) {
	update_colour_setting (&Config.graphics_bg, graphics_bg);
	graphics_refresh ();
    }
}

void do_config ()
{
    GtkWidget *dialog;
    GtkWidget *dummy;
    GtkWidget *tabs;
    GtkWidget *text_font;
    GtkWidget *graphics_tab;
    GtkWidget *colour_tab;
    GtkWidget *fit_to_window;
    GtkWidget *red_gamma;
    GtkWidget *green_gamma;
    GtkWidget *blue_gamma;
    GtkWidget *animate_images;
    GtkWidget *animation_speed;
    GtkWidget *image_filter;
    GtkWidget *position_combo;
    GtkWidget *scale_combo;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *widget;
    GtkWidget *scale_box;
    gint filter_idx;
    gint i;
    gchar *saved_font = NULL;

    Configuration saved_config = Config;
    saved_font = Config.text_font ? g_strdup (Config.text_font) : NULL;
    int saved_split = gtk_paned_get_position (GTK_PANED (Gui.partition));

    gchar *saved_text_fg = Config.text_fg ? g_strdup (Config.text_fg) : NULL;
    gchar *saved_text_bg = Config.text_bg ? g_strdup (Config.text_bg) : NULL;
    gchar *saved_graphics_bg =
	Config.graphics_bg ? g_strdup (Config.graphics_bg) : NULL;

    /*
     * Some settings, such as the partition, may have changed since they were
     * last restored. Save them now so the config dialog won't accidentally
     * restore the old values.
     */
    write_config_file ();

    dialog = gtk_dialog_new_with_buttons (
	"Preferences",
	GTK_WINDOW (Gui.main_window),
	GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	"_OK",
	GTK_RESPONSE_ACCEPT,
	"_Cancel",
	GTK_RESPONSE_REJECT,
	NULL);

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 400);

    /* Top level */

    tabs = gtk_notebook_new ();
    gtk_box_pack_start (
	GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
	tabs, TRUE, TRUE, 0);

    colour_tab = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width (GTK_CONTAINER (colour_tab), 5);
    gtk_notebook_append_page (GTK_NOTEBOOK (tabs), colour_tab,
			      gtk_label_new ("Text and Colour"));

    graphics_tab = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    gtk_container_set_border_width (GTK_CONTAINER (graphics_tab), 5);
    gtk_notebook_append_page (GTK_NOTEBOOK (tabs), graphics_tab,
			      gtk_label_new ("Graphics"));

    /* Text and colour settings */

    text_font = add_font_button (
	colour_tab, "_Text font:", "Select Text Font");
    if (Config.text_font)
	gtk_font_chooser_set_font (
	    GTK_FONT_CHOOSER (text_font), Config.text_font);
    g_signal_connect (text_font, "font-set",
	G_CALLBACK (font_changed), NULL);

    dummy = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (colour_tab), dummy, FALSE, FALSE, 8);

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX (colour_tab), box, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic ("Text _margins:");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_size_request (label, 120, -1);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    widget = gtk_spin_button_new_with_range (0, 100, 1);
    gtk_widget_set_size_request (widget, MIN_WINDOW_HEIGHT, -1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), Config.text_margin);
    gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
    g_signal_connect (widget, "value-changed",
	G_CALLBACK (spin_changed_int), &Config.text_margin);
    gtk_box_pack_start (GTK_BOX(box), 
	add_reset_button (widget, 20.0), FALSE, FALSE, 0);

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX (colour_tab), box, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic ("Line _spacing:");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_size_request (label, 120, -1);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    widget = gtk_spin_button_new_with_range (0.0, 5.0, 0.5);
    gtk_widget_set_size_request (widget, MIN_WINDOW_HEIGHT, -1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), Config.text_leading);
    gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
    g_signal_connect (widget, "value-changed",
	G_CALLBACK (spin_changed_float), &Config.text_leading);
    gtk_box_pack_start (GTK_BOX(box), 
	add_reset_button (widget, 1.0), FALSE, FALSE, 0);

    dummy = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (colour_tab), dummy, FALSE, FALSE, 8);

    text_fg = add_colour_setting (
	colour_tab, "Override text _foreground colour",
	"Select text foreground colour", Config.text_fg);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (text_fg->checkbox),
	Config.text_fg != NULL);
    g_signal_connect (text_fg->button, "color-set",
	G_CALLBACK (color_changed), &Config.text_fg);
    g_signal_connect (text_fg->checkbox, "toggled",
	G_CALLBACK (color_checkbox_changed), text_fg);
    if (saved_text_fg) Config.text_fg = g_strdup (saved_text_fg);

    text_bg = add_colour_setting (
	colour_tab, "Override text _background colour",
	"Select text background colour", Config.text_bg);
    gtk_toggle_button_set_active (
	GTK_TOGGLE_BUTTON (text_bg->checkbox), Config.text_bg != NULL);
    g_signal_connect (text_bg->button, "color-set",
	G_CALLBACK (color_changed), &Config.text_bg);
    g_signal_connect (text_bg->checkbox, "toggled",
	G_CALLBACK (color_checkbox_changed), text_bg);
    if (saved_text_bg) Config.text_bg = g_strdup(saved_text_bg);

    graphics_bg = add_colour_setting (
	colour_tab, "Override picture back_ground colour",
	"Select picture background colour", Config.graphics_bg);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (graphics_bg->checkbox),
	Config.graphics_bg != NULL);
    g_signal_connect (graphics_bg->button, "color-set",
	G_CALLBACK (color_changed), &Config.graphics_bg);
    if (saved_graphics_bg) Config.graphics_bg = g_strdup(saved_graphics_bg);

    /* Picture settings */

    const gchar *items[G_N_ELEMENTS (imageFilters)];
    for (i = 0; i < G_N_ELEMENTS (imageFilters); i++) {
	items[i] = imageFilters[i].description;
    }
    widget = add_combo_box (graphics_tab, "_Interpolation:",
	items, G_N_ELEMENTS (imageFilters),
	get_interp_type_index (Config.image_filter));
    g_signal_connect (G_OBJECT (widget), "changed",
	G_CALLBACK (on_filter_changed), NULL);

    static const gchar *position_items[] = {
	"Top",
	"Bottom",
	"Left",
	"Right",
	"Hidden"
    };
    position_combo = add_combo_box (graphics_tab, "Graphics _position:",
	(const gchar **) position_items, G_N_ELEMENTS (position_items),
	Config.graphics_position);
    g_signal_connect (G_OBJECT (position_combo), "changed",
	G_CALLBACK (on_position_changed), NULL);
    gtk_combo_box_set_active (
	GTK_COMBO_BOX (position_combo), Config.graphics_position);

    static const gchar *scale_items[] = {
	"Fit to window",
	"Custom scale"
    };
    scale_combo = add_combo_box (graphics_tab, "_Scaling mode:",
	(const gchar **) scale_items, G_N_ELEMENTS (scale_items),
	Config.fit_to_window ? 0 : 1);
    gtk_combo_box_set_active (
	GTK_COMBO_BOX (scale_combo), Config.fit_to_window ? 0 : 1);

    image_scale = add_scale (graphics_tab, "Scale _factor:", 0.1, 5.0, 0.1,
	Config.image_scale, 2, 1.0);
    g_signal_connect (image_scale, "value-changed",
	G_CALLBACK (scale_changed), &Config.image_scale);
    scale_box = gtk_widget_get_parent (image_scale);
    gtk_widget_set_sensitive (scale_box, !Config.fit_to_window);
    g_signal_connect (G_OBJECT (scale_combo), "changed",
	G_CALLBACK (on_scale_combo_changed), scale_box);

    dummy = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (graphics_tab), dummy, FALSE, FALSE, 8);

    /* Gamma settings */
    red_gamma = add_scale (graphics_tab, "_Red gamma:", 0.1, 5.0, 0.1,
	Config.red_gamma, 2, 1.0);
    g_signal_connect (red_gamma, "value-changed",
	G_CALLBACK (scale_changed), &Config.red_gamma);

    green_gamma = add_scale (graphics_tab, "_Green gamma:", 0.1, 5.0, 0.1,
	Config.green_gamma, 2, 1.0);
    g_signal_connect (green_gamma, "value-changed",
	G_CALLBACK(scale_changed), &Config.green_gamma);

    blue_gamma = add_scale (graphics_tab, "_Blue gamma:", 0.1, 5.0, 0.1,
	Config.blue_gamma, 2, 1.0);
    g_signal_connect (blue_gamma, "value-changed",
	G_CALLBACK (scale_changed), &Config.blue_gamma);

    dummy = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (graphics_tab), dummy, FALSE, FALSE, 8);

    /* Animation settings */
    animate_images =
	gtk_check_button_new_with_mnemonic ("_Animate line drawing");
    gtk_toggle_button_set_active (
	GTK_TOGGLE_BUTTON (animate_images), Config.animate_images);
    gtk_box_pack_start (GTK_BOX (graphics_tab), animate_images, TRUE, TRUE, 0);
    g_signal_connect (animate_images, "toggled",
	G_CALLBACK (toggle_changed), &Config.animate_images);

    animation_speed = add_scale (
	graphics_tab, "Animation sp_eed:", 1.0, 50.0, 1.0,
	Config.animation_speed, 0, 10.0);
    gtk_range_set_value (GTK_RANGE (animation_speed), Config.animation_speed);
    g_signal_connect (animation_speed, "value-changed",
	G_CALLBACK (scale_changed), &Config.animation_speed);
    g_signal_connect (G_OBJECT (animation_speed), "format-value",
	G_CALLBACK (format_animation_speed), NULL);
    box = gtk_widget_get_parent (animation_speed);
    gtk_widget_set_sensitive (box, Config.animate_images);
    g_signal_connect (G_OBJECT (animate_images), "toggled",
	G_CALLBACK (toggle_sensitivity), box);

    /* Run the dialog */

    gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
	g_free (Config.text_font);
	Config.text_font = g_strdup(
	    gtk_font_chooser_get_font (GTK_FONT_CHOOSER(text_font)));

	update_colour_setting (&Config.text_fg, text_fg);
	update_colour_setting (&Config.text_bg, text_bg);
	update_colour_setting (&Config.graphics_bg, graphics_bg);

	write_config_file ();
    }
    else
    {
	Config = saved_config;
	if (saved_font && g_utf8_validate (saved_font, -1, NULL)) {
	    Config.text_font = g_strdup (saved_font);
	    gtk_font_chooser_set_font (
		GTK_FONT_CHOOSER (text_font), saved_font);
	}

	/* Apply settings */

	text_refresh ();
	graphics_refresh ();
	gui_refresh ();
	gtk_paned_set_position (GTK_PANED (Gui.partition), saved_split);
    }

    gtk_widget_destroy (dialog);
    g_free (text_fg);
    g_free (text_bg);
    g_free (graphics_bg);
    g_free (saved_font);
    g_free (saved_text_fg);
    g_free (saved_text_bg);
    g_free (saved_graphics_bg);
}
