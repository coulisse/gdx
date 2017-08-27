/*
TODO: resolve domain
TODO: error managing
TODO: trim input fields
TODO: dialog between terminal and gtk
TODO: split telnet and gui modules
TODO: communication queues?!
TODO: get connection error (and reset the connection button)
TODO: start thread at the init

*/

typedef struct {
  char dx[16];
  int freq;
  char de[16];
  char tags[10];
  char comments[255];
  int utc;
  int date;
} dxc;

typedef struct {
  char *entry_server;
  char *entry_port;
  char *entry_callsign;
 // short connection_status_request;
  enum status_request {disconnect=-1, nothing, login} connection_status_request;
  char msg[255];
  dxc dxcluster_record;
} glb;


#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

glb thread_glb;
pthread_t thread_id;
pthread_mutex_t mutexConn;

#include "utility.h"
#include "labels.h"
#include "telnet.h"


typedef struct {
  GtkEntry        *g_entry_server;
  GtkEntry        *g_entry_port;
  GtkEntry        *g_entry_callsign;
  GtkButton       *g_btn_connect;
  GtkLabel        *g_lbl_connection_info;
} app_widgets;


//GUI
int main(int argc, char *argv[]) {
    GtkBuilder      *builder;
    GtkWidget       *window;
    app_widgets     *widgets = g_slice_new(app_widgets);

    thread_glb.connection_status_request=nothing;

    gtk_init(&argc, &argv);
    builder = gtk_builder_new();
    GError *err = NULL;

    if(0 == gtk_builder_add_from_file (builder, "../glade/dxcluster.glade", &err)) {
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
    widgets->g_btn_connect  = GTK_BUTTON(gtk_builder_get_object(builder, "btnConnect"));
    widgets->g_lbl_connection_info  = GTK_LABEL(gtk_builder_get_object(builder, "lblConnectionInfo"));

    g_object_unref(builder);
    gtk_widget_show(window);
    gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);

    gtk_main();

    return 0;
}


// called when connect button is clicked
void on_swtch_connect_on(GtkButton *button, app_widgets *app_wdgts) {

  if (strcmp(gtk_button_get_label(button),LBL_CONNECT)==0) {
    //connect (create thread telnet for connection)
    thread_glb.entry_server =  (char *) gtk_entry_get_text (app_wdgts->g_entry_server);
    thread_glb.entry_port =   (char *) gtk_entry_get_text (app_wdgts->g_entry_port);
    thread_glb.entry_callsign = (char *) gtk_entry_get_text (app_wdgts->g_entry_callsign);

    if(strlen(thread_glb.entry_server) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_SRV_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_server);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(thread_glb.entry_port) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_PRT_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_port);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(thread_glb.entry_callsign) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CLS_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_callsign);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    };

    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CONNECTING);
    thread_glb.connection_status_request = login;

    //creating thread
    pthread_mutex_init(&mutexConn, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create (&thread_id,  &attr, &telnet, NULL);

    gtk_button_set_label(button,LBL_DISCONNECT);

  } else {

    //disconnect
    thread_glb.connection_status_request = disconnect;
    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_DISCONNECTING);
    gtk_button_set_label(button,LBL_CONNECT);
  }
}

// called when window is closed
void on_window_main_destroy() {
    void *status;
    int rc;

    thread_glb.connection_status_request=disconnect;
    if (thread_id>0) { 
        rc = pthread_join(thread_id, &status);
	if (rc) {
	   printf("ERROR; return code from pthread_join() is %d\n", rc);
	}
     };
    pthread_mutex_destroy(&mutexConn);
    gtk_main_quit();
}

/*
gui: accept only numeric keys
*/
void key_event_numeric_check(GtkWidget *widget, GdkEventKey *event) {
  if (
      ((event->keyval >= '0')&&(event->keyval <= '9')) ||
      (event->keyval == GDK_KEY_BackSpace) ||
      (event->keyval == GDK_KEY_Clear) ||
      (event->keyval == GDK_KEY_Delete) ||
      (event->keyval == GDK_KEY_Tab) ||
      (event->keyval == GDK_KEY_ISO_Enter) ||
      (event->keyval == GDK_KEY_End) ||
      (event->keyval == GDK_KEY_Begin) ||
      (event->keyval == GDK_KEY_Home) ||
      (event->keyval == GDK_KEY_Insert) ||
      (event->keyval == GDK_KEY_Left) ||
      (event->keyval == GDK_KEY_Up) ||
      (event->keyval == GDK_KEY_Right) ||
      (event->keyval == GDK_KEY_KP_0) ||
      (event->keyval == GDK_KEY_KP_1) ||
      (event->keyval == GDK_KEY_KP_2) ||
      (event->keyval == GDK_KEY_KP_3) ||
      (event->keyval == GDK_KEY_KP_4) ||
      (event->keyval == GDK_KEY_KP_5) ||
      (event->keyval == GDK_KEY_KP_6) ||
      (event->keyval == GDK_KEY_KP_7) ||
      (event->keyval == GDK_KEY_KP_8) ||
      (event->keyval == GDK_KEY_KP_9) ||
      (event->keyval == GDK_KEY_KP_Tab) ||
      (event->keyval == GDK_KEY_KP_Enter) ||
      (event->keyval == GDK_KEY_KP_Home) ||
      (event->keyval == GDK_KEY_KP_Left) ||
      (event->keyval == GDK_KEY_KP_Up) ||
      (event->keyval == GDK_KEY_KP_Right) ||
      (event->keyval == GDK_KEY_KP_Down) ||
      (event->keyval == GDK_KEY_KP_End) ||
      (event->keyval == GDK_KEY_KP_Begin) ||
      (event->keyval == GDK_KEY_KP_Insert) ||
      (event->keyval == GDK_KEY_KP_Delete) ||
      (event->keyval == GDK_KEY_Control_L) ||
      (event->keyval == GDK_KEY_Control_R) ||
      (event->keyval == GDK_KEY_Down)) {
      #pragma GCC diagnostic ignored "-Wformat-zero-length"
      g_printerr("");
      #pragma GCC diagnostic warning "-Wformat-zero-length"
  }
  return;
}
