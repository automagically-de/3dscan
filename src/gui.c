#include <gtk/gtk.h>

#include "gui.h"
#include "region.h"

struct _GuiData {
	Config *config;
	Model *model;
	GtkWindow *window;
	GtkWidget *image;
	GtkWidget *l_angle;
	GtkWidget **angle_pbars;
	RegionType region_selector;
};

static gboolean gui_image_btn_press_cb(GtkWidget *widget, GdkEventButton *eb,
	gpointer user_data);
static gboolean gui_image_btn_release_cb(GtkWidget *widget, GdkEventButton *eb,
	gpointer user_data);
static gboolean gui_region_toggled_cb(GtkToggleToolButton *toggle_tool_button,
	gpointer user_data);

static gboolean gui_btn_new_cb(GtkToolButton *toolbutton, gpointer user_data);
static gboolean gui_btn_save_cb(GtkToolButton *toolbutton, gpointer user_data);

static void gui_create_toolbar(GuiData *gui, GtkBox *box);
static void gui_create_angle_view(GuiData *gui, GtkBox *box);

GuiData *gui_init(Config *config, Model *model)
{
	GuiData *data;
	GtkWidget *vbox, *hbox;

	data = g_new0(GuiData, 1);
	data->config = config;
	data->model = model;
	data->region_selector = REGION_OBJECT;

	data->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(data->window), vbox);

	gui_create_toolbar(data, GTK_BOX(vbox));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	data->image = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(hbox), data->image, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(data->image), "button-press-event",
		G_CALLBACK(gui_image_btn_press_cb), data);
	g_signal_connect(G_OBJECT(data->image), "button-release-event",
		G_CALLBACK(gui_image_btn_release_cb), data);
	gtk_widget_add_events(data->image,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gui_create_angle_view(data, GTK_BOX(vbox));

	return data;
}

/*****************************************************************************/

static void gui_create_toolbar(GuiData *gui, GtkBox *box)
{
	GtkWidget *tbar;
	GtkToolItem *titem;
	GSList *gselect = NULL;

	tbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(tbar), GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_tooltips(GTK_TOOLBAR(tbar), TRUE);
	gtk_box_pack_start(box, tbar, FALSE, TRUE, 0);

	titem = gtk_tool_button_new_from_stock("gtk-new");
	gtk_toolbar_insert(GTK_TOOLBAR(tbar), titem, -1);
	g_signal_connect(G_OBJECT(titem), "clicked",
		G_CALLBACK(gui_btn_new_cb), gui);

	titem = gtk_tool_button_new_from_stock("gtk-save");
	gtk_toolbar_insert(GTK_TOOLBAR(tbar), titem, -1);
	g_signal_connect(G_OBJECT(titem), "clicked",
		G_CALLBACK(gui_btn_save_cb), gui);

	titem = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(tbar), titem, -1);

	titem = gtk_radio_tool_button_new_from_stock(gselect, "gtk-dialog-info");
	gtk_tool_item_set_tooltip_text(titem, "select object region");
	gselect = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(titem));
	gtk_toolbar_insert(GTK_TOOLBAR(tbar), titem, -1);
	g_object_set_data(G_OBJECT(titem), "region-id",
		GINT_TO_POINTER(REGION_OBJECT));
	g_signal_connect(G_OBJECT(titem), "toggled",
		G_CALLBACK(gui_region_toggled_cb), gui);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(titem), TRUE);

	titem = gtk_radio_tool_button_new_from_stock(gselect, "gtk-justify-fill");
	gtk_tool_item_set_tooltip_text(titem, "select graycode region");
	gselect = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(titem));
	gtk_toolbar_insert(GTK_TOOLBAR(tbar), titem, -1);
	g_object_set_data(G_OBJECT(titem), "region-id",
		GINT_TO_POINTER(REGION_GRAYCODE));
	g_signal_connect(G_OBJECT(titem), "toggled",
		G_CALLBACK(gui_region_toggled_cb), gui);
}

static void gui_create_angle_view(GuiData *gui, GtkBox *box)
{
	GtkWidget *abox, *label;
	gchar *s;
	gint32 i;
	guint32 n, h, w;

	abox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(box, abox, FALSE, FALSE, 0);
	n = (1 << config_get_int(gui->config, "base", "n_bits", 0));
	h = config_get_int(gui->config, "gui", "angle_bar_height", 32);
	w = config_get_int(gui->config, "gui", "angle_bar_width", 8);
	gui->angle_pbars = g_new0(GtkWidget *, n);
	for(i = 0; i < n; i ++) {
		if((i % (n / 4)) == 0) {
			s = g_strdup_printf(" %.2f°: ", (gdouble)i * 360 / n);
			label = gtk_label_new(s);
			g_free(s);
			gtk_box_pack_start(GTK_BOX(abox), label, FALSE, TRUE, 0);
		}
		gui->angle_pbars[i] = gtk_progress_bar_new();
		gtk_progress_bar_set_orientation(
			GTK_PROGRESS_BAR(gui->angle_pbars[i]),
			GTK_PROGRESS_BOTTOM_TO_TOP);
		gtk_widget_set_size_request(gui->angle_pbars[i], w, h);
		gtk_box_pack_start(GTK_BOX(abox), gui->angle_pbars[i],
			FALSE, TRUE, 0);
	}
	gui->l_angle = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(abox), gui->l_angle, FALSE, FALSE, 5);
}

/*****************************************************************************/

void gui_set_scan_progress(GuiData *data, guint32 angle, guint32 n_scans)
{
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->angle_pbars[angle]),
		(gfloat)n_scans / 10.0);
}

void gui_set_quit_handler(GuiData *data, GCallback quit, gpointer user_data)
{
	g_signal_connect(G_OBJECT(data->window), "delete-event", quit, user_data);
}

void gui_set_image(GuiData *data, GdkPixbuf *pixbuf)
{
	guint32 w, h;
	gint32 i, nb;
	gfloat sh;
	Region *region;

	w = gdk_pixbuf_get_width(pixbuf);
	h = gdk_pixbuf_get_height(pixbuf);
	nb = config_get_int(data->config, "base", "n_bits", 1);

	gtk_widget_set_size_request(data->image, w, h);
	gdk_draw_pixbuf(data->image->window,
		data->image->style->fg_gc[GTK_WIDGET_STATE(data->image)],
		pixbuf, 0, 0, 0, 0, w, h, GDK_RGB_DITHER_NONE, 0, 0);

	region = g_slist_nth_data(data->model->regions, REGION_GRAYCODE);
	if((region != NULL) &&
		(region->rect.width > 0) &&
		(region->rect.height > nb * 3)) {
		sh = (gfloat)region->rect.height / nb;
		for(i = 0; i < nb; i ++) {
			gdk_draw_rectangle(data->image->window,
				data->image->style->fg_gc[GTK_WIDGET_STATE(data->image)],
				FALSE,
				region->rect.x, region->rect.y + i * sh,
				region->rect.width, sh);
		}
	}

	region = g_slist_nth_data(data->model->regions, REGION_OBJECT);
	if((region != NULL) &&
		(region->rect.width > 0) && (region->rect.height > 0)) {
		gdk_draw_rectangle(data->image->window,
			data->image->style->fg_gc[GTK_WIDGET_STATE(data->image)],
			FALSE,
			region->rect.x, region->rect.y,
			region->rect.width, region->rect.height);
	}
}

void gui_set_angle(GuiData *data, const gchar *s)
{
	gtk_label_set_text(GTK_LABEL(data->l_angle), s);
}

void gui_show(GuiData *data)
{
	gtk_widget_show_all(GTK_WIDGET(data->window));
}

void gui_update(GuiData *data)
{
	while(gtk_events_pending())
		gtk_main_iteration();
}

void gui_cleanup(GuiData *data)
{
	g_free(data);
}

/*****************************************************************************/

static gboolean gui_image_btn_press_cb(GtkWidget *widget, GdkEventButton *eb,
	gpointer user_data)
{
	GuiData *data = user_data;
	Region *region;

	g_return_val_if_fail(data != NULL, FALSE);

	region = g_slist_nth_data(data->model->regions, data->region_selector);
	if(region == NULL)
		return FALSE;

	region->rect.x = eb->x;
	region->rect.y = eb->y;
	region->rect.width = 0;
	region->rect.height = 0;

	g_debug("button pressed @ %.0f, %.0f", eb->x, eb->y);

	return TRUE;
}

static gboolean gui_image_btn_release_cb(GtkWidget *widget, GdkEventButton *eb,
	gpointer user_data)
{
	GuiData *data = user_data;
	Region *region;
	guint32 x, y;

	g_return_val_if_fail(data != NULL, FALSE);

	region = g_slist_nth_data(data->model->regions, data->region_selector);
	if(region == NULL)
		return FALSE;

	x = region->rect.x;
	y = region->rect.y;

	if(x > eb->x) {
		region->rect.x = eb->x;
		region->rect.width = x - eb->x;
	} else {
		region->rect.width = eb->x - x;
	}
	if(y > eb->y) {
		region->rect.y = eb->y;
		region->rect.height = y - eb->y;
	} else {
		region->rect.height = eb->y - y;
	}

	model_rebuild_storage(data->model);

	return TRUE;
}

static gboolean gui_region_toggled_cb(GtkToggleToolButton *toggle_tool_button,
	gpointer user_data)
{
	GuiData *data = user_data;
	RegionType type;

	g_return_val_if_fail(data != NULL, FALSE);
	if(gtk_toggle_tool_button_get_active(
		GTK_TOGGLE_TOOL_BUTTON(toggle_tool_button))) {
		type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(toggle_tool_button),
			"region-id"));
		data->region_selector = type;
	}

	return TRUE;
}

static gboolean gui_btn_new_cb(GtkToolButton *toolbutton, gpointer user_data)
{
	GuiData *gui = user_data;

	g_return_val_if_fail(gui != NULL, FALSE);
	model_rebuild_storage(gui->model);
	return TRUE;
}

static gboolean gui_btn_save_cb(GtkToolButton *toolbutton, gpointer user_data)
{
	GuiData *gui = user_data;

	g_return_val_if_fail(gui != NULL, FALSE);
	model_save(gui->model, "test.ac");
	return TRUE;
}
