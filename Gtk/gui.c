/*
 * gui.c - User interface
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

#include <gtk/gtk.h>

#include "main.h"
#include "config.h"
#include "gui.h"
#include "text.h"
#include "graphics.h"

GuiWidgets Gui;

/*
 * This should be automagically called every time the user changes the size
 * of the game window.
 */

static gboolean configure_window (GtkWidget *widget, GdkEventConfigure *event,
				  gpointer user_data)
{
    static int last_width = 0;
    static int last_height = 0;

    if (last_width != event->width || last_height != event->height) {
	Config.window_width = event->width;
	Config.window_height = event->height;
	last_width = event->width;
	last_height = event->height;

	if (Config.fit_to_window)
	    g_timeout_add (2, (GSourceFunc) graphics_refresh, NULL);
    }

    return FALSE;
}

void gui_refresh ()
{
    gtk_window_set_default_size (
	GTK_WINDOW (Gui.main_window), Config.window_width,
	Config.window_height);

    GtkWidget *graphics_widget = Gui.picture_area;
    GtkWidget *text_widget = gtk_widget_get_parent(Gui.text_view);

    if (graphics_widget) {
	GtkWidget *parent = gtk_widget_get_parent (graphics_widget);
	if (parent) {
	    g_object_ref (graphics_widget);
	    gtk_container_remove (GTK_CONTAINER (parent), graphics_widget);
	}
    }
    if (text_widget) {
	GtkWidget *parent = gtk_widget_get_parent (text_widget);
	if (parent) {
	    g_object_ref (text_widget);
	    gtk_container_remove (GTK_CONTAINER (parent), text_widget);
	}
    }

    gboolean is_horizontal = (
	Config.graphics_position == 2 || Config.graphics_position == 3);
    GtkWidget *new_partition = gtk_paned_new (
	is_horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    if (is_horizontal) {
	gtk_widget_set_size_request (new_partition, MIN_WINDOW_WIDTH, -1);
	if (graphics_widget) gtk_widget_set_size_request (graphics_widget, 50, -1);
	if (text_widget) gtk_widget_set_size_request (text_widget, 50, -1);
    } else {
	gtk_widget_set_size_request (new_partition, -1, MIN_WINDOW_HEIGHT);
	if (graphics_widget) gtk_widget_set_size_request (graphics_widget, -1, 50);
	if (text_widget) gtk_widget_set_size_request (text_widget, -1, 50);
    }

    gtk_container_remove (GTK_CONTAINER (Gui.main_box), Gui.partition);
    Gui.partition = new_partition;
    gtk_box_pack_start (GTK_BOX (Gui.main_box), Gui.partition, TRUE, TRUE, 0);

    if (graphics_widget && text_widget) {
	switch (Config.graphics_position) {
	    case 0:
		gtk_paned_pack1 (
		    GTK_PANED (Gui.partition), graphics_widget, TRUE, FALSE);
		gtk_paned_pack2 (
		    GTK_PANED (Gui.partition), text_widget, TRUE, FALSE);
		break;
	    case 1:
		gtk_paned_pack1 (
		    GTK_PANED (Gui.partition), text_widget, TRUE, FALSE);
		gtk_paned_pack2 (
		    GTK_PANED (Gui.partition), graphics_widget, TRUE, FALSE);
		break;
	    case 2:
		gtk_paned_pack1 (
		    GTK_PANED (Gui.partition), graphics_widget, TRUE, FALSE);
		gtk_paned_pack2 (
		    GTK_PANED (Gui.partition), text_widget, TRUE, FALSE);
		break;
	    case 3:
		gtk_paned_pack1 (
		    GTK_PANED (Gui.partition), text_widget, TRUE, FALSE);
		gtk_paned_pack2 (
		    GTK_PANED (Gui.partition), graphics_widget, TRUE, FALSE);
		break;
	    case 4:
		gtk_paned_pack1 (
		    GTK_PANED (Gui.partition), graphics_widget, TRUE, FALSE);
		gtk_paned_pack2 (
		    GTK_PANED (Gui.partition), text_widget, TRUE, FALSE);
		break;
	}
    }

    if (graphics_widget) g_object_unref (graphics_widget);
    if (text_widget) g_object_unref (text_widget);

    gtk_paned_set_position (GTK_PANED (Gui.partition), 
	Config.graphics_position == 4 ? 0 : Config.window_split);
    gtk_widget_show_all (Gui.partition);

    if (Config.graphics_position == 4 && graphics_widget) {
	gtk_widget_hide (graphics_widget);
	gtk_widget_set_size_request (graphics_widget, 1, 1);
	gtk_paned_set_position (GTK_PANED (Gui.partition), 0);
	GtkStyleContext *context = gtk_widget_get_style_context (Gui.partition);
	gtk_style_context_add_class (context, "hidden-handle");
	gtk_widget_set_can_focus (Gui.partition, FALSE);
    } else if (Config.graphics_position != 4 && graphics_widget) {
	if (gtk_orientable_get_orientation (
	    GTK_ORIENTABLE (Gui.partition)) == GTK_ORIENTATION_HORIZONTAL) {
	    gtk_widget_set_size_request (graphics_widget, 50, -1);
	} else {
	    gtk_widget_set_size_request (graphics_widget, -1, 50);
	}

	gtk_paned_set_position (
	    GTK_PANED (Gui.partition), Config.window_split);
	GtkStyleContext *context =
	    gtk_widget_get_style_context (Gui.partition);
	gtk_style_context_remove_class (context, "hidden-handle");
	gtk_widget_set_can_focus (Gui.partition, TRUE);
	gtk_widget_show (graphics_widget);
    }

    GtkWidget *scrolled_window = gtk_widget_get_parent (Gui.text_view);
    gtk_scrolled_window_set_policy (
	GTK_SCROLLED_WINDOW (scrolled_window),
	GTK_POLICY_AUTOMATIC,
	GTK_POLICY_AUTOMATIC
    );

    gtk_widget_grab_focus (Gui.text_view);
}

static void do_open ()
{
    start_new_game (NULL, NULL);
}

static gboolean confirm_quit (void)
{
    GtkWidget *dialog = gtk_message_dialog_new (
	GTK_WINDOW (Gui.main_window),
	GTK_DIALOG_MODAL,
	GTK_MESSAGE_QUESTION,
	GTK_BUTTONS_YES_NO,
	NULL
    );

    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
	"\nDo you really want to stop?");

    gboolean quit = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES);
    gtk_widget_destroy (dialog);

    return quit;
}

static void do_quit ()
{
    if (confirm_quit ())
	close_application (NULL, NULL);
}

static gboolean on_delete_event (
    GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return !confirm_quit ();
}

static void create_menus (GtkWidget *main_box)
{
    GtkWidget *menubar;
    GtkWidget *menu;
    GtkWidget *menuitem;
    GtkWidget *submenu;

    menubar = gtk_menu_bar_new ();
    gtk_box_pack_start (GTK_BOX (main_box), menubar, FALSE, FALSE, 0);

    menu = gtk_menu_new ();
    menuitem = gtk_menu_item_new_with_mnemonic ("_File");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);

    submenu = gtk_menu_item_new_with_mnemonic ("_Open");
    g_signal_connect (
	G_OBJECT (submenu), "activate", G_CALLBACK (do_open), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), submenu);

    submenu = gtk_menu_item_new_with_mnemonic ("_Preferences...");
    g_signal_connect (
	G_OBJECT (submenu), "activate", G_CALLBACK (do_config), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), submenu);

    submenu = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), submenu);

    submenu = gtk_menu_item_new_with_mnemonic ("_Quit");
    g_signal_connect (
	G_OBJECT (submenu), "activate", G_CALLBACK (do_quit), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), submenu);

    menu = gtk_menu_new ();
    menuitem = gtk_menu_item_new_with_mnemonic ("_Help");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);

    submenu = gtk_menu_item_new_with_mnemonic ("_About");
    g_signal_connect (
	G_OBJECT (submenu), "activate", G_CALLBACK (do_about), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), submenu);
}

void gui_init ()
{
    GtkWidget *text_scroll;
    GdkPixbuf *icon;

    Gui.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (Gui.main_window), "GtkLevel9");

    GtkIconTheme *theme = gtk_icon_theme_get_default ();
    icon = gtk_icon_theme_load_icon (theme, "gtklevel9", 32, 0, NULL);

    if (!icon) {
	icon = gdk_pixbuf_new_from_file ("gtklevel9.png", NULL);
    }

    if (icon) {
	gtk_window_set_icon (GTK_WINDOW (Gui.main_window), icon);
	gtk_window_set_default_icon (icon);
	g_object_unref (icon);
    }

    gtk_widget_set_size_request (Gui.main_window, MIN_WINDOW_WIDTH,
				 MIN_WINDOW_HEIGHT);

    g_signal_connect (G_OBJECT (Gui.main_window), "destroy",
		      G_CALLBACK (close_application), NULL);
    g_signal_connect (G_OBJECT (Gui.main_window), "configure-event",
		      G_CALLBACK (configure_window), NULL);
    g_signal_connect (G_OBJECT (Gui.main_window), "delete-event",
		      G_CALLBACK (on_delete_event), NULL);

    /* The main "box" */

    Gui.main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (Gui.main_window), Gui.main_box);

    /* Menus */
    create_menus (Gui.main_box);

    /* The game area; picture and text */
    
    Gui.partition = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (Gui.main_box), Gui.partition, TRUE, TRUE, 0);

    Gui.statusbar = gtk_statusbar_new ();

    gtk_box_pack_end (GTK_BOX (Gui.main_box), Gui.statusbar, FALSE, FALSE, 0);

    Gui.picture_area = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (
	GTK_SCROLLED_WINDOW (Gui.picture_area), GTK_POLICY_EXTERNAL,
	GTK_POLICY_EXTERNAL);
    gtk_paned_add1 (GTK_PANED (Gui.partition), Gui.picture_area);

    text_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (
	GTK_SCROLLED_WINDOW (text_scroll), GTK_POLICY_NEVER,
	GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (
	GTK_SCROLLED_WINDOW (text_scroll), GTK_SHADOW_IN);
    gtk_paned_add2 (GTK_PANED (Gui.partition), text_scroll);

    Gui.picture = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (Gui.picture_area), Gui.picture);

    Gui.text_buffer = gtk_text_buffer_new (NULL);

    gtk_text_buffer_create_tag (
	Gui.text_buffer, "level9-input", "weight", PANGO_WEIGHT_BOLD,
	"editable", TRUE, NULL);
    gtk_text_buffer_create_tag (
	Gui.text_buffer, "level9-old-input", "weight", PANGO_WEIGHT_BOLD,
	"editable", FALSE, NULL);
    gtk_text_buffer_create_tag (
	Gui.text_buffer, "level9-input-padding",
	"weight", PANGO_WEIGHT_BOLD, "editable", TRUE, NULL);

    Gui.text_view = gtk_text_view_new_with_buffer (Gui.text_buffer);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (Gui.text_view), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (Gui.text_view), 3);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (Gui.text_view), 3);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (Gui.text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (Gui.text_view), TRUE);
    gtk_container_add (GTK_CONTAINER (text_scroll), Gui.text_view);
}
