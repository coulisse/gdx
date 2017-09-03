/******************************************************************************

                             ===  gdx  ===

   gdx - Ham radio cluster browser
   Copyright (C) 2017  Corrado Gerbaldo - IU1BOW


   gdx is a GNU/Linux application written in C and use GTK

   mail: corrado.gerbaldo@gmail.com

   license:              GNU GENERAL PUBLIC LICENSE
                          Version 3, 29 June 2007
                            see file "LICENSE"

  Changelog:
  ..............................................................................
  version: alpha
  date...: 02/09/2017
  comment: alpha release
  ..............................................................................

******************************************************************************/

#define PATH_GLADE "../glade/dxcluster.glade"
#define PATH_BAND  "../cfg/band.cfg"
#define PATH_DXCC  "../cfg/country.cfg"
#define PATH_CONTINENTS "../cfg/continents.cfg"

#define START_DXCC  "Prefix"
#define END_DXCC    "NOTES:"
#define UNDER_DXCC  "__________________"

// structure for the main info got from cluster
typedef struct {
  short rc;
  char dx[16];
  char freq[16];
  char de[16];
  char utc[6];
  char locator[6];
  char comments[255];
  char band[5];
  char dx_entity[35];
  char dx_continent[5];
  char de_entity[35];
  char de_continent[5];
} dxcluster;

//struct used for global variables
typedef struct {
  char *entry_server;
  char *entry_port;
  char *entry_callsign;
  enum status_request {disconnect=-1, nothing, login} connection_status_request;
} glb;

//structure of the band file
typedef struct {
  double freq_min;
  double freq_max;
  char band[5];
  int filtered;
} band;

//structure of the dxcc country file
typedef struct {
  char prefix_min[19];
  char prefix_max[19];
  char entity[35];
  char continent[6];
  char ITU[6];
  char CQ[6];
  char entity_code[4];
} dxcc;

//structure of the continents file
typedef struct {
  char de_dx[3];
  char continent[2];
  int filtered;
} continent;

#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <locale.h>

//global variable
glb program_glb;

//global variables for thread
pthread_t thread_id;
pthread_mutex_t mutexLock;

//global array for the bands
band glb_band_array[50]={};
int glb_ind_band=0;

//global array for the dxcc prefixs
dxcc glb_dxcc_array[1500]={};
int glb_ind_dxcc=0;
int glb_max_prefix_len=0;

//global array for filter continents
continent glb_continent_array[14];
int glb_ind_continent=0;

#include "utility.h"
#include "labels.h"

//struct that contains all GTK widgets for the GUI
typedef struct {
  GtkEntry            *g_entry_server;
  GtkEntry            *g_entry_port;
  GtkEntry            *g_entry_callsign;
  GtkButton           *g_btn_connect;
  GtkLabel            *g_lbl_connection_info;
  GtkListStore        *g_list_store_cluster;
  GtkTreeModelFilter  *g_filter_cluster;
  GtkTreeView         *g_tree_view_cluster;
} app_widgets;

//global widgets
app_widgets     *widgets;

//global builder
GtkBuilder      *builder;

#include "telnet.h"

/*-----------------------------------------------------------------------------
load bands file into a global array
-----------------------------------------------------------------------------*/
int load_bands(){

  int ret;

  FILE * fps;
  fps = fopen (PATH_BAND, "r");
  if( fps==NULL ) {
    printf("Error loading band file");
    return 1;
  }
  ret = fscanf(fps,"%lf %lf %s %d",
        &glb_band_array[glb_ind_band].freq_min,
        &glb_band_array[glb_ind_band].freq_max,
        glb_band_array[glb_ind_band].band,
        &glb_band_array[glb_ind_band].filtered);
  while(ret!=EOF) {

      glb_ind_band++;
      ret = fscanf(fps,"%lf %lf %s %d",
            &glb_band_array[glb_ind_band].freq_min,
            &glb_band_array[glb_ind_band].freq_max,
            glb_band_array[glb_ind_band].band,
            &glb_band_array[glb_ind_band].filtered);
  }
  fclose(fps);
  return 0;
}

/*-----------------------------------------------------------------------------
writes bands to file reading from global array
-----------------------------------------------------------------------------*/
int write_bands(){
  int i;

  setlocale(LC_ALL,"C");  //workaround in order to read/write band with same format
  FILE * fps;
  fps = fopen (PATH_BAND, "w");
  if( fps==NULL ) {
    printf("Error opening band file for output");
    return 1;
  }

  for (i=0;i<glb_ind_band;i++) {
    fprintf(fps,"%f %f %s %d\n",
          glb_band_array[i].freq_min,
          glb_band_array[i].freq_max,
          glb_band_array[i].band,
          glb_band_array[i].filtered);

  }
  fclose(fps);
  return 0;

}

/*-----------------------------------------------------------------------------
Loads continent configuration file
-----------------------------------------------------------------------------*/
int load_continents(){

  int ret;

  FILE * fps;
  fps = fopen (PATH_CONTINENTS, "r");
  if( fps==NULL ) {
    printf("Error loading continents file");
    return 1;
  }
  ret = fscanf(fps,"%s %s %d",
        glb_continent_array[glb_ind_continent].de_dx,
        glb_continent_array[glb_ind_continent].continent,
        &glb_continent_array[glb_ind_continent].filtered);

  while(ret!=EOF) {
    //  printf("DE-DX <%s> continent <%s> filtered <%d>\n",glb_continent[glb_ind_continent].de_dx,  glb_continent[glb_ind_continent].continent, glb_continent[glb_ind_continent].filtered);
      glb_ind_continent++;
      ret = fscanf(fps,"%s %s %d",
            glb_continent_array[glb_ind_continent].de_dx,
            glb_continent_array[glb_ind_continent].continent,
            &glb_continent_array[glb_ind_continent].filtered);
  }
  fclose(fps);

  return 0;
}

/*-----------------------------------------------------------------------------
writes continents configuration file
-----------------------------------------------------------------------------*/
int write_continents(){
  int i;

  FILE * fps;
  fps = fopen (PATH_CONTINENTS, "w");
  if( fps==NULL ) {
    printf("Error opening continents file for output");
    return 1;
  }

  for (i=0;i<glb_ind_continent;i++) {
    fprintf(fps,"%s %s %d\n",
          glb_continent_array[i].de_dx,
          glb_continent_array[i].continent,
          glb_continent_array[i].filtered);
  }
  fclose(fps);
  return 0;

}

/*-----------------------------------------------------------------------------
loads dxcc file into a global array
-----------------------------------------------------------------------------*/
int load_dxcc(){

  const int max_len=256;
  char str[max_len];
  char subbuff[6];
  int start=FALSE;
  int end=FALSE;
  char *s_repl;
  char *token;
  const char s_comma[2] = ",";
  const char s_minus[2] = "-";
  char *minus_pos=NULL;
  char *blank_pos=NULL;

  FILE * fp;
  fp = fopen (PATH_DXCC, "r");
  if( fp==NULL ) {
    printf("Error loading dxcc file");
    return 1;
  }

  while(fgets(str, max_len, fp)!=NULL) {
    memcpy(subbuff, &str[4], 6 );
    subbuff[6] = '\0';
    if (strcmp(subbuff,START_DXCC)==0) {
      if (!end) {
          start=TRUE;
      }
    }

    if (strcmp(subbuff,END_DXCC)==0) {
      end=TRUE;
    }

    if (start && !end) {
      memcpy(glb_dxcc_array[glb_ind_dxcc].prefix_min,&str[4], 18);
      glb_dxcc_array[glb_ind_dxcc].prefix_min[19]='\0';
      //printf("%s --> ",glb_dxcc_array[glb_ind_dxcc].prefix );
      s_repl = replace_char(glb_dxcc_array[glb_ind_dxcc].prefix_min, '*', ' ');
      s_repl = replace_char(s_repl, '^', ' ');
      s_repl = replace_char(s_repl, '#', ' ');
      //s_repl = replace_char(s_repl, '/', ',');
      s_repl = replace_char(s_repl, '(', ',');
      s_repl = replace_char(s_repl, ')', ' ');

      //Split by ","
      token = strtok(s_repl, s_comma);
     /* walk through other tokens */
       while( token != NULL ) {
         minus_pos=strstr(token,s_minus);
         strcpy(glb_dxcc_array[glb_ind_dxcc].prefix_min,token);
         if (minus_pos==NULL){
           strcpy(glb_dxcc_array[glb_ind_dxcc].prefix_max,token);
         } else {
          strcpy(glb_dxcc_array[glb_ind_dxcc].prefix_max,minus_pos+1*sizeof(char));
          glb_dxcc_array[glb_ind_dxcc].prefix_min[minus_pos-token]='\0';
         }

         blank_pos=strstr(glb_dxcc_array[glb_ind_dxcc].prefix_min," ");
         if (blank_pos>0){
           glb_dxcc_array[glb_ind_dxcc].prefix_min[blank_pos-glb_dxcc_array[glb_ind_dxcc].prefix_min]='\0';
         }

         blank_pos=strstr(glb_dxcc_array[glb_ind_dxcc].prefix_max," ");

         if (blank_pos>0){
           glb_dxcc_array[glb_ind_dxcc].prefix_max[blank_pos-glb_dxcc_array[glb_ind_dxcc].prefix_max]='\0';
         }

         if ((strcmp(glb_dxcc_array[glb_ind_dxcc].prefix_min,START_DXCC)!=0) &&
            (strcmp(glb_dxcc_array[glb_ind_dxcc].prefix_min,UNDER_DXCC)!=0) &&
            (strlen(glb_dxcc_array[glb_ind_dxcc].prefix_min) > glb_max_prefix_len)) {
              glb_max_prefix_len=strlen(glb_dxcc_array[glb_ind_dxcc].prefix_min);
         }

         if ((strcmp(glb_dxcc_array[glb_ind_dxcc].prefix_max,START_DXCC)!=0) &&
            (strcmp(glb_dxcc_array[glb_ind_dxcc].prefix_min,UNDER_DXCC)!=0) &&
            (strlen(glb_dxcc_array[glb_ind_dxcc].prefix_max) > glb_max_prefix_len)) {
              glb_max_prefix_len=strlen(glb_dxcc_array[glb_ind_dxcc].prefix_max);
         }

         memcpy(glb_dxcc_array[glb_ind_dxcc].entity, &str[24], 33);
         glb_dxcc_array[glb_ind_dxcc].entity[34]='\0';
         memcpy(glb_dxcc_array[glb_ind_dxcc].continent, &str[59], 5);
         glb_dxcc_array[glb_ind_dxcc].continent[5]='\0';
         memcpy(glb_dxcc_array[glb_ind_dxcc].ITU, &str[65], 5);
         glb_dxcc_array[glb_ind_dxcc].ITU[5]='\0';
         memcpy(glb_dxcc_array[glb_ind_dxcc].CQ, &str[71], 5);
         glb_dxcc_array[glb_ind_dxcc].CQ[5]='\0';
         memcpy(glb_dxcc_array[glb_ind_dxcc].entity_code, &str[77], 3);
         glb_dxcc_array[glb_ind_dxcc].entity_code[3]='\0';
         glb_ind_dxcc++;
         token = strtok(NULL, s_comma);
      }
    }
  }

  fclose(fp);
  return 0;
}

/*-----------------------------------------------------------------------------
GUI: set the toggle buttons filter as loaded by the file band.tab
the id of the toggle buttons are the same of the band
-----------------------------------------------------------------------------*/
void set_filter_button_band(GtkBuilder *builder) {
  int i;
  GtkToggleButton *tb;
  for (i=0;i<glb_ind_band;i++) {
    tb=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, glb_band_array[i].band));
    gtk_toggle_button_set_active (tb,(gboolean) glb_band_array[i].filtered);
  }
}

/*-----------------------------------------------------------------------------
set the filter when a band toggle button is pressed. The name of the button
is the same of the filter
The refilter
-----------------------------------------------------------------------------*/
void manageBandFilters (GtkToggleButton *button, app_widgets *app_wdgts) {
  const gchar *btnId;
  int i=0;
  int found=0;

  #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
  btnId = gtk_buildable_get_name(button);
  #pragma GCC diagnostic warning "-Wincompatible-pointer-types"

  /*find and set the band in the array band comparing the toggle button ID
    with band name: it's important that the ID and the band name are the same */
  while ((i<glb_ind_band) && (!found)) {
    if (strcmp(btnId,glb_band_array[i].band)==0) {
        glb_band_array[i].filtered =gtk_toggle_button_get_active(button);
        found=1;
    } else {
        i++;
    }
  }
//refilter the entire list
  gtk_tree_model_filter_refilter (app_wdgts->g_filter_cluster);

}

/*-----------------------------------------------------------------------------
search the band through the bands global array and return the filter
-----------------------------------------------------------------------------*/
gboolean search_filter_by_band(const char *band) {
  int i=0;
  int found=0;

  while ((i<glb_ind_band) && (!found)) {
    if (strcmp(band,glb_band_array[i].band)==0) {
          found=1;
    } else {
          i++;
    }
  }

  if (found) {
    return (gboolean) glb_band_array[i].filtered;
  } else {
    return TRUE; //if the band is not founds return true in order to show it
  }
}

/*-----------------------------------------------------------------------------
search the continent through the continents global array and return the filter
-----------------------------------------------------------------------------*/
gboolean search_filter_by_continent(const char *de_dx, const char *continent) {
  int i=0;
  int found=0;

  while ((i<glb_ind_continent) && (!found)) {
    if ((strcmp(de_dx,glb_continent_array[i].de_dx)==0) &&
        (strcmp(continent,glb_continent_array[i].continent)==0)) {
          found=1;
    } else {
          i++;
    }

  }

  if (found) {
    return (gboolean) glb_continent_array[i].filtered;
  } else {
    return TRUE; //if the continents is not founds return true in order to show it
  }
}

/*-----------------------------------------------------------------------------
GUI: set the toggle buttons filter as loaded by the file continents.cfg
   the id of the toggle buttons are the same of the continents
-----------------------------------------------------------------------------*/
void set_filter_button_continent(GtkBuilder *builder) {
  int i;
  GtkToggleButton *tb;
  char btnId[5];

  for (i=0;i<glb_ind_continent;i++) {
    strcpy(btnId,glb_continent_array[i].de_dx);
    strcat(btnId,"-");
    strcat(btnId,glb_continent_array[i].continent);
    tb=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, btnId));
    gtk_toggle_button_set_active (tb,(gboolean) glb_continent_array[i].filtered);
  }
}

/*-----------------------------------------------------------------------------
set the filter when a continent toggle button is pressed. The name of the button
is the same of the filter (+DE / DX); then refilter.
-----------------------------------------------------------------------------*/
void manageContinentFilters (GtkToggleButton *button, app_widgets *app_wdgts) {
  const gchar *btnId;
  char de_dx[3];
  char continent[3];
  int i=0;
  int found=0;

  #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
  btnId = gtk_buildable_get_name(button);
  #pragma GCC diagnostic warning "-Wincompatible-pointer-types"
  strncpy(de_dx,btnId,2);
  memcpy(continent,btnId+3,2);
  //printf("de_dx <%s>, continent <%s>",de_dx,continent);

  /*find and set the de-dx/continent in the array  comparing the  */
  while ((i<glb_ind_continent) && (!found)) {
    if ((strcmp(de_dx,glb_continent_array[i].de_dx)==0) &&
       (strcmp(continent,glb_continent_array[i].continent)==0)) {
        glb_continent_array[i].filtered =gtk_toggle_button_get_active(button);
        found=1;
    } else {
        i++;
    }
  }

//refilter the entire list
  gtk_tree_model_filter_refilter (app_wdgts->g_filter_cluster);
}

/*.............................................................................
GUI: set all toggle button as set/unset.
..............................................................................*/
//set all toggle button as set or unset
void setFilters (gboolean set_unset) {
  int i;
  GtkToggleButton *tb;
  char btnId[5];

  for (i=0;i<glb_ind_band;i++) {
    tb=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, glb_band_array[i].band));
    gtk_toggle_button_set_active (tb,set_unset);
  }

  for (i=0;i<glb_ind_continent;i++) {
    strcpy(btnId,glb_continent_array[i].de_dx);
    strcat(btnId,"-");
    strcat(btnId,glb_continent_array[i].continent);
    tb=GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, btnId));
    gtk_toggle_button_set_active (tb,set_unset);
  }
}

/*-----------------------------------------------------------------------------
GUI: signal on "ALL" button clicked
-----------------------------------------------------------------------------*/
void manageALLFilters (GtkButton *button, app_widgets *app_wdgts) {
  setFilters(TRUE);
}

/*-----------------------------------------------------------------------------
GUI: signal on "NONE" button clicked
-----------------------------------------------------------------------------*/
void manageNONEFilters (GtkButton *button, app_widgets *app_wdgts) {
  setFilters(FALSE);
}

/*.............................................................................
GUI: Filter function "linked" to the TreeModelFilter with
     gtk_tree_model_filter_set_visible_func in the main.
     this funtion filter by band, de continent, dx continent
..............................................................................*/
static gboolean row_visible (GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
  gchar *str_band;
  gchar *str_de_continent;
  gchar *str_dx_continent;

  gboolean visible_band = FALSE;
  gboolean visible_de_continent = FALSE;
  gboolean visible_dx_continent = FALSE;

  gboolean visible = FALSE;

  gtk_tree_model_get (model, iter,  6, &str_band, -1);
  gtk_tree_model_get (model, iter, 10, &str_de_continent, -1);
  gtk_tree_model_get (model, iter,  8, &str_dx_continent, -1);

  if (str_band && str_de_continent && str_dx_continent) {
    //visible = search_filter_by_band((char *)str_band);
    visible_band = search_filter_by_band((char *)str_band);
    visible_de_continent = search_filter_by_continent("DE",(char *)str_de_continent);
    visible_dx_continent = search_filter_by_continent("DX",(char *)str_dx_continent);
    visible = visible_band && visible_de_continent  &&  visible_dx_continent;
  }
  g_free (str_band);
  g_free (str_de_continent);
  g_free (str_dx_continent);
  return visible;
}

/*-----------------------------------------------------------------------------
GUI: signal on "CONNECT" button clicked
- if connect:
  . get and checks the server / port / callsign
  . create a separate thread that connect to the telnet server
  . set the connection request to "login"
- if disconnect:
  . set the connection request to "disconnect" (so the thread discconects)
-----------------------------------------------------------------------------*/
void on_swtch_connect_on(GtkButton *button, app_widgets *app_wdgts) {

  if (strcmp(gtk_button_get_label(button),LBL_CONNECT)==0) {
    //connect (create thread telnet for connection)
    program_glb.entry_server =   (char *) gtk_entry_get_text (app_wdgts->g_entry_server);
    program_glb.entry_port =     (char *) gtk_entry_get_text (app_wdgts->g_entry_port);
    program_glb.entry_callsign = (char *) gtk_entry_get_text (app_wdgts->g_entry_callsign);

    if(strlen(program_glb.entry_server) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_SRV_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_server);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(program_glb.entry_port) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_PRT_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_port);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    } else if(strlen(program_glb.entry_callsign) == 0) {
        gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CLS_MISS);
        gtk_widget_grab_focus((GtkWidget*)app_wdgts->g_entry_callsign);
        gtk_button_set_label(button,LBL_CONNECT);
        return;
    };

    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_CONNECTING);
    program_glb.connection_status_request = login;

    //creating thread
    pthread_mutex_init(&mutexLock, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create (&thread_id,  &attr, &telnet, NULL);

  } else {
    //disconnect
    program_glb.connection_status_request = disconnect;
    gtk_label_set_text (app_wdgts->g_lbl_connection_info,LBL_DISCONNECTING);
    gtk_button_set_label(button,LBL_CONNECT);
  }
}

/*-----------------------------------------------------------------------------
GUI: signal on main window close
- write the bands configuration file
- write the continents configuration file
- request to the child thread (telnet) to disconnect
- wait until the child thread (telnet) die (join to the main)
- end
-----------------------------------------------------------------------------*/
void on_window_main_destroy() {
  void *status;
  int rc;

  rc=write_bands();
  if (rc>0) {
    printf("LBL_ERR_BAND\n");
  }

  rc=write_continents();
  if (rc>0) {
    printf("LBL_ERR_BAND\n");
  }
  program_glb.connection_status_request=disconnect;
  if (thread_id>0) {
    rc = pthread_join(thread_id, &status);
  	if (rc) {
      printf("LBL_ERR_JOIN %d\n", rc);
  	}
  };
  pthread_mutex_destroy(&mutexLock);
  gtk_main_quit();
}

/*-----------------------------------------------------------------------------
GUI: signal on a button is pressed on port txt field
  this function checks that the key pressed is numeric
-----------------------------------------------------------------------------*/
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

/*******************************************************************************
GUI: main

- load bands config file
- load continents config file
- load dxcc country config file
- load glade file and build GUI
- connect all widgets
- link filter to the treemodel
- set button bands filters
- set button continent filters
- show the window
- call the GTK Main
*******************************************************************************/
int main(int argc, char *argv[]) {

    GtkWidget       *window;

    if (load_bands()>0) {
      exit(1);
    };

    if (load_continents()>0) {
      exit(1);
    };

    if (load_dxcc()>0) {
      exit(1);
    };

    widgets = g_slice_new(app_widgets);
    program_glb.connection_status_request=nothing;
    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    GError *err = NULL;

    if(0 == gtk_builder_add_from_file (builder, PATH_GLADE, &err)) {
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
    widgets->g_list_store_cluster  = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststoreCluster"));
    widgets->g_filter_cluster  = GTK_TREE_MODEL_FILTER(gtk_builder_get_object(builder, "treeModelFilterCluster"));
    widgets->g_tree_view_cluster  = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewCluster"));

    //assign the filter function in order to filter rows
    gtk_tree_model_filter_set_visible_func (widgets->g_filter_cluster,
                                            (GtkTreeModelFilterVisibleFunc) row_visible,
                                            widgets->g_filter_cluster, NULL);
    set_filter_button_band(builder);
    set_filter_button_continent(builder);
    gtk_widget_show(window);
    gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);

    gtk_main();

    return 0;
}
