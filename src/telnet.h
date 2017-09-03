/******************************************************************************
TELNET module
  this module contains the mains function for connect to the telnet cluster
  remote server, get data and pass them to the GUI
******************************************************************************/

#include <termios.h>
#include <fcntl.h>
#include <netdb.h>

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
static struct termios tin;

/*-----------------------------------------------------------------------------
generic funtion for display message to the main windows and also to the
terminal
-----------------------------------------------------------------------------*/
void message(char *msg, short output_type) {
    gtk_label_set_text (widgets->g_lbl_connection_info, msg);

    if (output_type == 1) {
        printf("%s\n",msg);
    } else if (output_type == 2) {
        puts (msg);
    } else if (output_type == 3) {
        perror (msg);
    };
};

/*-----------------------------------------------------------------------------
search the band through the bands global array
-----------------------------------------------------------------------------*/
char *search_band_by_freq(char *freq) {
  int i=0;
  int found=0;
  double freq_num=atof(freq);

  if (freq_num > 0) {
    freq_num=freq_num/1000;
    while ((i<glb_ind_band) && (!found)) {
      if (freq_num>=glb_band_array[i].freq_min &&
          freq_num<=glb_band_array[i].freq_max) {
            found=1;
      } else {
            i++;
      }
    }
  }

  if (found) {
    return glb_band_array[i].band;
  } else {
    return "";
  }
}

/*-----------------------------------------------------------------------------
search the call sign in the dxcc prefix
-----------------------------------------------------------------------------*/
dxcc search_dxcc_by_prefix(char *prefix) {
  int i;
  int found=0;
  int len=glb_max_prefix_len;
  int compare_min,compare_max;
  dxcc ret;

  strcpy(ret.entity,"-");
  strcpy(ret.continent,"-");

  while(len>0 && !found) {

    i=0;
    found=0;

    while ((i<glb_ind_dxcc) && (!found)) {
      if (len==strlen(glb_dxcc_array[i].prefix_min)) {
        compare_min=strncmp(prefix,glb_dxcc_array[i].prefix_min,len);
        compare_max=strncmp(prefix,glb_dxcc_array[i].prefix_max,len);
      } else {
        compare_min = -1;
        compare_max = 1;
      }

      if (compare_min >=0 && compare_max <=0) {
        found=1;
      } else {
        i++;
      }
    }

    if (found) {
      strcpy(ret.entity,glb_dxcc_array[i].entity);
      strcpy(ret.continent,glb_dxcc_array[i].continent);
      break;
    }
    len--;
  }

  return ret;
}

/*-----------------------------------------------------------------------------
parse the string retruned  by the cluster

this function return a structure
- first of check if it is an interesting lint (DX de..)
- then split it by spaces
- the starting from the left search the UTC and then get the locator
- search the de continent and the dx continent
- search the band with the frequence
-----------------------------------------------------------------------------*/
dxcluster parse(char *str) {

    dxcluster dxc={0,"","","","","","","","","","",""};

    const char s[2] = " ";
    char *token;
    int idx_word = 0;
    int ln_token;
    int len_str_org;
    int len_utc;
    int utc_strt_pos;
    int utc_end_pos;
    int dx_end_pos;
    char *str_org;
    char str_comments[255];
    dxcc search_dxcc;

    str=clrStr(str);
    str_org=malloc(strlen(str));
    strcpy(str_org,str);

   if (memcmp(str,"DX de",4) != 0) {
    message(LBL_CONNECTED,0);
    return dxc;
  }

  /* get the first token */
  //message(str,0);
  token = strtok(str, s);
 /* walk through other tokens */
   while( token != NULL ) {
       switch(idx_word) {

        case 2:
          //DE - remove the last ":"
          ln_token = strlen(token)-1;
          if (ln_token>0) {
            if (token[ln_token]==':') {
              token[ln_token]='\0';
            }//     printf( " %s\n", token );
          }
          strcat(dxc.de,token);
          break;

        //freq
        case 3:
          strcat(dxc.freq,token);
          break;

        //DX//     printf( " %s\n", token );
        case 4:
          strcat(dxc.dx,token);
          break;

        //UTC
        case 5 ... 999:
          if (strlen(token)==5) {
             //TODO: && (isnumber(subString(token,4)))
            if (token[4]=='Z') {
              memcpy(dxc.utc,token,5);
            }
          }
      }
      token = strtok(NULL, s);
      idx_word++;
   }

   //get locator and comments
   len_str_org=strlen(str_org);
   len_utc=strlen(dxc.utc);

   if (len_utc>0) {
     //get the locator after the utc
     utc_strt_pos=strstr(str_org,dxc.utc)-str_org;
     utc_end_pos=utc_strt_pos+len_utc;
     mid(str_org,utc_end_pos+1,len_str_org+1-utc_end_pos,dxc.locator,5);
     //get comments between DX and utc
     if (strlen(dxc.dx)>0) {
       dx_end_pos=strstr(str_org,dxc.dx)-str_org+strlen(dxc.dx);
       mid(str_org,dx_end_pos+1,utc_strt_pos-dx_end_pos-1,str_comments,255);
       strcpy(dxc.comments,zstring_trim(str_comments));

       search_dxcc = search_dxcc_by_prefix(dxc.dx);
       strcpy(dxc.dx_entity,search_dxcc.entity);
       strcpy(dxc.dx_continent,search_dxcc.continent);
       dxc.dx_continent[2]='\0';
       //printf("search_dxcc (dx) <%s>\n", search_dxcc.continent);

       search_dxcc = search_dxcc_by_prefix(dxc.de);
       strcpy(dxc.de_entity,search_dxcc.entity);
       strcpy(dxc.de_continent,search_dxcc.continent);
       dxc.de_continent[2]='\0';
       //printf("search_dxcc (de) <%s>\n", search_dxcc.continent);

     }
   }

   //search and add band
   strcpy(dxc.band,search_band_by_freq(dxc.freq));

   free(str);
   free(str_org);
   dxc.rc=1;
   return dxc;
}

/*..............................................................................
handsaking with socket/server
..............................................................................*/
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

/*..............................................................................
setting terminal
..............................................................................*/
static void terminal_set(void) {
    // save terminal configuration
    tcgetattr(STDIN_FILENO, &tin);
    static struct termios tlocal;
    memcpy(&tlocal, &tin, sizeof(tin));
    cfmakeraw(&tlocal);
    tcsetattr(STDIN_FILENO,TCSANOW,&tlocal);
}

/*..............................................................................
reset terminal
..............................................................................*/
static void terminal_reset(void) {
    // restore terminal upon exit
    tcsetattr(STDIN_FILENO,TCSANOW,&tin);
}

/*-----------------------------------------------------------------------------
main loop that send and receive data with remote telnet server and the socket
in this loop we send login command if require, or disconnect command; otherwise
we call the parse function.
If the parse funcion return a valid dxc structure we add it to the liststore
-----------------------------------------------------------------------------*/
void connection_loop(int *sk, struct timeval ts){
  unsigned char buf[BUFLEN +1];
  char cluster_str[255];
  int ind_cluster_str=0;
  int len;
  GtkTreeIter treeIter;
  dxcluster dxc;

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
          message(LBL_ERR_SELECT,3);
          //return 1;
          return;
      }
      else if (nready == 0) {
        if (program_glb.connection_status_request == login) {    //LOGIN --- we send the callsign at the first request
          message(LBL_LOGIN,-1);
	        pthread_mutex_lock (&mutexLock);
          program_glb.connection_status_request = nothing;
	        pthread_mutex_unlock (&mutexLock);
          if (send(*sk, concat(program_glb.entry_callsign,crlf), strlen(program_glb.entry_callsign)+2, 0) < 0) {
            //return 1;
            return;
	        }
        } else if (program_glb.connection_status_request == disconnect) {
	        pthread_mutex_lock (&mutexLock);
          program_glb.connection_status_request = nothing;
	        pthread_mutex_unlock (&mutexLock);
          if (send(*sk, concat(BYE,crlf), strlen(BYE)+2, 0) < 0) {
            //return 1;
            return;
	         }
        }
        ts.tv_sec = 1; // 1 second
        ts.tv_usec = 0;
      }
      else if (*sk != 0 && FD_ISSET(*sk, &fds)) {

          // start by reading a single byte
          int rv;
          if ((rv = recv(*sk , buf , 1 , 0)) < 0) {
              //return 1;
              return;
	  }
          else if (rv == 0) {
              message(LBL_CONN_CLOSED,1);
              //return 0;
              return;
          }

          if (buf[0] == CMD) {
              // read 2 more bytes
              len = recv(*sk , buf + 1 , 2 , 0);
              if (len  < 0) {
                  //return 1;
                  return;
              }
              else if (len == 0) {
                  message(LBL_CONN_CLOSED,1);
                  //return 0;
                  return;
              }
              negotiate(*sk, buf, 3);
          }
          else {
              len = 1;
              buf[len] = '\0';
              printf("%s", buf);
              //write the character to the string until to its end and then parse
              cluster_str[ind_cluster_str]=buf[0];
              ind_cluster_str++;
              if (buf[0]=='\n')  {
                 cluster_str[ind_cluster_str]='\0';
                 ind_cluster_str=0;
                 dxc=parse(cluster_str);
                 if (dxc.rc) {
                   //adding the structure returned by parsing to the liststore
                   gtk_list_store_prepend(widgets->g_list_store_cluster,&treeIter);
                   gtk_list_store_set (widgets->g_list_store_cluster,&treeIter,
                                                0,dxc.de,
                                                1,dxc.freq,
                                                2,dxc.dx,
                                                3,dxc.utc,
                                                4,dxc.locator,
                                                5,dxc.comments,
                                                6,dxc.band,
                                                7,dxc.dx_entity,
                                                8,dxc.dx_continent,
                                                9,dxc.de_entity,
                                               10,dxc.de_continent,
                                                -1);
                  //scroll to the first element
                  gtk_tree_view_scroll_to_cell (widgets->g_tree_view_cluster,
                                                gtk_tree_path_new_first (),
                                                NULL,
                                                FALSE,
                                                0,
                                                0);

                };
              };
            fflush(0);
          }
      }

      else if (FD_ISSET(0, &fds)) {
        buf[0] = getc(stdin); //fgets(buf, 1, stdin);
        if (send(*sk, buf, 1, 0) < 0) {
        //return 1;
           return;
	      }
        if (buf[0] == '\n') // with the terminal in raw mode we need to force a LF
           putchar('\r');
        }
      }
      free(buf);
      free(cluster_str);
}

/*******************************************************************************
entry point of the thread
- get the host / ip / port and then create a sock
- when sock/terminal are ok, call the main connection loop
- when the connection loop ends, then also the thread end.
*******************************************************************************/

void* telnet(void* unused) {

    int sock;
    struct sockaddr_in server;
    struct hostent      *returned_host;
    char               **pointer_pointer;
    char                 answer[INET_ADDRSTRLEN];

    //Create socket
  //  sock = socket(AF_INET , SOCK_STREAM , 0);

    returned_host=gethostbyname(program_glb.entry_server);
    if(returned_host==NULL)   {
      fprintf(stderr,"error %d\n",h_errno);
      return NULL;
    }

    for(pointer_pointer=returned_host->h_addr_list;
        *pointer_pointer;
        pointer_pointer++)   {
      inet_ntop(AF_INET,(void *)*pointer_pointer,answer,sizeof(answer));
      printf("IP address          : %s\n",answer);
      sock=socket(AF_INET,SOCK_STREAM,0);
      if (sock == -1) {
          message(LBL_ERR_SOCKET,3);
          //return 1;
          gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);
          return NULL;
      }

      memset(&server,0,sizeof(server));
      server.sin_family=AF_INET;
      server.sin_port=htons(atoi(program_glb.entry_port));

      memmove(&server.sin_addr,
              *pointer_pointer,
              sizeof(server.sin_addr)
             );

      if(connect(sock,(struct sockaddr*)&server,sizeof(server))<0)  {
        message(LBL_ERR_CONN,3);
        //return 1;
        gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);
        return NULL;
      }

      printf("connection established on file descriptor %d\n",sock);
    }

    message(LBL_CONNECTED,2);
    gtk_button_set_label(widgets->g_btn_connect,LBL_DISCONNECT);

    // set terminal
    terminal_set();
    atexit(terminal_reset);

    struct timeval ts;
    ts.tv_sec = 1; // 1 second
    ts.tv_usec = 0;

    //LOOP
    connection_loop(&sock, ts);

    close(sock);

    pthread_exit((void*) 0);
}
