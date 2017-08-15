#include <gtk/gtk.h>

typedef struct {
  GtkEntry       *g_entry_server;
  GtkEntry       *g_entry_port;
  GtkEntry       *g_entry_callsign;
} app_widgets;

int main(int argc, char *argv[])
{
    GtkBuilder      *builder;
    GtkWidget       *window;
    app_widgets     *widgets = g_slice_new(app_widgets);

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    GError *err = NULL;

    if(0 == gtk_builder_add_from_file (builder, "glade/dxcluster.glade", &err)) {
    	fprintf(stderr, "Error loading dxcluster.glade.Error: %s\n", err->message);
    }

    window = GTK_WIDGET(gtk_builder_get_object(builder, "mainWindow"));
    if (NULL == window) {
    	fprintf(stderr, "Unable to file object with id \"mainWindow\" \n");
    }
    gtk_builder_connect_signals(builder, widgets);

    //connect all widgets
    widgets->g_entry_server  = GTK_ENTRY(gtk_builder_get_object(builder, "entryServer"));
    widgets->g_entry_port  = GTK_ENTRY(gtk_builder_get_object(builder, "entryPort"));
    widgets->g_entry_callsign  = GTK_ENTRY(gtk_builder_get_object(builder, "entryCallsign"));


    g_object_unref(builder);

    gtk_widget_show(window);
    gtk_main();

    return 0;
}

// called when window is closed
void on_window_main_destroy() {
    gtk_main_quit();
}

// called when connect button is clicked
void on_btn_connect_clicked(GtkButton *button, app_widgets *app_wdgts) {

    const gchar *entry_server;
    entry_server =   gtk_entry_get_text (app_wdgts->g_entry_server);
    const gchar *entry_port;
    entry_port =   gtk_entry_get_text (app_wdgts->g_entry_port);
    const gchar *entry_callsign;
    entry_callsign =   gtk_entry_get_text (app_wdgts->g_entry_callsign);

    fprintf(stderr, entry_server);
    fprintf(stderr, entry_port);
    fprintf(stderr, entry_callsign);

}
