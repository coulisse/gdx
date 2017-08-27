/*
TODO: resolve domain
TODO: error managing
TODO: trim input fields
TODO: dialog between terminal and gtk
TODO: enum connection request
TODO: split telnet and gui modules
TODO: communication queues?!

*/
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>

#include "utility.h"
#include "labels.h"

#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe
#define CMD 0xff
#define CMD_ECHO 1
#define CMD_WINDOW_SIZE 31
#define BUFLEN 20
#define BYE "bye"

static char crlf[] = { '\r','\n' };

typedef struct {
  char *entry_server;
  char *entry_port;
  char *entry_callsign;
  short connection_status_request;
} glb;

glb app_glb;


typedef struct {
  GtkEntry        *g_entry_server;
  GtkEntry        *g_entry_port;
  GtkEntry        *g_entry_callsign;
  GtkButton       *g_btn_connect;
  GtkLabel        *g_lbl_connection_info;
} app_widgets;


//--------------------------- TELNET -------
void negotiate(int sock, unsigned char *buf, int len) {
    int i;

    if (buf[1] == DO && buf[2] == CMD_WINDOW_SIZE) {
        unsigned char tmp1[10] = {255, 251, 31};
        if (send(sock, tmp1, 3 , 0) < 0)
            exit(1);

        unsigned char tmp2[10] = {255, 250, 31, 0, 80, 0, 24, 255, 240};
        if (send(sock, tmp2, 9, 0) < 0)
            exit(1);
        return;
    }

    for (i = 0; i < len; i++) {
        if (buf[i] == DO)
            buf[i] = WONT;
        else if (buf[i] == WILL)
            buf[i] = DO;
    }

    if (send(sock, buf, len , 0) < 0)
        exit(1);
}

static struct termios tin;

static void terminal_set(void) {
    // save terminal configuration
    tcgetattr(STDIN_FILENO, &tin);

    static struct termios tlocal;
    memcpy(&tlocal, &tin, sizeof(tin));
    cfmakeraw(&tlocal);
    tcsetattr(STDIN_FILENO,TCSANOW,&tlocal);
}

static void terminal_reset(void) {
    // restore terminal upon exit
    tcsetattr(STDIN_FILENO,TCSANOW,&tin);
}

void connection_loop(int *sk, struct timeval ts){

  app_glb.connection_status_request = 1;

  unsigned char buf[BUFLEN +1];
  int len;

  while (1) {
      // select setup
      fd_set fds;
      FD_ZERO(&fds);
      if (*sk != 0)
          FD_SET(*sk, &fds);

      FD_SET(0, &fds);

      // wait for data
      int nready = select(*sk + 1, &fds, (fd_set *) 0, (fd_set *) 0, &ts);

      if (nready < 0) {
          perror("select. Error");
          //return 1;
          return;
      }
      else if (nready == 0) {
          if (app_glb.connection_status_request == 1) {    //LOGIN --- we send the callsign at the first request
            app_glb.connection_status_request = 0;
            if (send(*sk, concat(app_glb.entry_callsign,crlf), strlen(app_glb.entry_callsign)+2, 0) < 0)
             //return 1;
             return;
          } else if (app_glb.connection_status_request == -1) {
            app_glb.connection_status_request = 0;
            if (send(*sk, concat(BYE,crlf), strlen(BYE)+2, 0) < 0)
             //return 1;
             return;
          }

          ts.tv_sec = 1; // 1 second
          ts.tv_usec = 0;
      }
      else if (*sk != 0 && FD_ISSET(*sk, &fds)) {

          // start by reading a single byte
          int rv;
          if ((rv = recv(*sk , buf , 1 , 0)) < 0)
              //return 1;
              return;
          else if (rv == 0) {
              printf("Connection closed by the remote end\n\r");
              //return 0;
              return;
          }

          if (buf[0] == CMD) {
              // read 2 more bytes
              len = recv(*sk , buf + 1 , 2 , 0);
              if (len  < 0)
                  //return 1;
                  return;
              else if (len == 0) {
                  printf("Connection closed by the remote end\n\r");
                  //return 0;
                  return;
              }
              negotiate(*sk, buf, 3);
          }
          else {
              len = 1;
              buf[len] = '\0';
              printf("%s", buf);
              fflush(0);
          }
      }

      else if (FD_ISSET(0, &fds)) {
        buf[0] = getc(stdin); //fgets(buf, 1, stdin);
        if (send(*sk, buf, 1, 0) < 0)
        //return 1;
        return;
        if (buf[0] == '\n') // with the terminal in raw mode we need to force a LF
           putchar('\r');
        }
      }
}

void* telnet(void* unused) {

    int sock;
    struct sockaddr_in server;

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        perror("Could not create socket. Error");
        //return 1;
        return NULL;
        perror("select. Error");
        return NULL;
    }

    server.sin_addr.s_addr = inet_addr(app_glb.entry_server);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(app_glb.entry_port));

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
        perror("connect failed. Error");
        //return 1;
        return NULL;
    }
    puts("Connected...\n");

    // set terminal
    terminal_set();
    atexit(terminal_reset);

    struct timeval ts;
    ts.tv_sec = 1; // 1 second
    ts.tv_usec = 0;

    //LOOP
    connection_loop(&sock, ts);

    close(sock);
    //return 0;
    return NULL;
}

//GUI
int main(int argc, char *argv[]) {
    GtkBuilder      *builder;
    GtkWidget       *window;
    app_widgets     *widgets = g_slice_new(app_widgets);

    app_glb.connection_status_request=0;

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

// called when window is closed
void on_window_main_destroy() {
    app_glb.connection_status_request=-1;
    sleep(3);
    gtk_main_quit();
}

// called when connect button is clicked
void on_swtch_connect_on(GtkButton *button, app_widgets *app_wdgts) {

  if (strcmp(gtk_button_get_label(button),LBL_CONNECT)==0) {
    //connect (create thread telnet for connection)
    app_glb.entry_server =  (char *) gtk_entry_get_text (app_wdgts->g_entry_server);
    app_glb.entry_port =   (char *) gtk_entry_get_text (app_wdgts->g_entry_port);
    app_glb.entry_callsign = (char *) gtk_entry_get_text (app_wdgts->g_entry_callsign);

    if(strlen(app_glb.entry_server) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_SRV_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_server);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(app_glb.entry_port) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_PRT_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_port);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(app_glb.entry_callsign) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CLS_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_callsign);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    };

    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CONNECTING);

    pthread_t thread_id;
    pthread_create (&thread_id, NULL, &telnet, NULL);

    gtk_button_set_label(button,LBL_DISCONNECT);

  } else {
    //disconnect
    app_glb.connection_status_request = -1;
    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_DISCONNECTING);
    gtk_button_set_label(button,LBL_CONNECT);
  }
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
