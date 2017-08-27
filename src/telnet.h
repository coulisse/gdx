#include <termios.h>
#include <fcntl.h>

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

int parse(const char *str) {
  
   const char s[2] = " ";
   char *token;
   int idx_word = 0;

   if (memcmp(str,"DX de",4) == 0) {
     message(str,0);
    /* get the first token */
    token = strtok(str, s);
   /* walk through other tokens */
     while( token != NULL ) {
   //     printf( " %s\n", token );
        switch(idx_word) {
           case 0:
            break;
           case 1:
            break;
           case 2:
            //DE
            break;
           case 3:
            //FREQ:
            perror(token);
            break;
      //     case 4:
            //DX
//THE LAST IS THE UTC
        }
        token = strtok(NULL, s);
        idx_word++;
     } 
   }
   return 0;
}

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

  unsigned char buf[BUFLEN +1];
  char cluster_str[255];
  int ind_cluster_str=0;
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
          message(LBL_ERR_SELECT,3);
          //return 1;
          return;
      }
      else if (nready == 0) {
          if (thread_glb.connection_status_request == login) {    //LOGIN --- we send the callsign at the first request
            message(LBL_LOGIN,-1);
	    pthread_mutex_lock (&mutexLock);
            thread_glb.connection_status_request = nothing;
	    pthread_mutex_unlock (&mutexLock);
            if (send(*sk, concat(thread_glb.entry_callsign,crlf), strlen(thread_glb.entry_callsign)+2, 0) < 0) {
             //return 1;
             return;
	    }
          } else if (thread_glb.connection_status_request == disconnect) {
	     pthread_mutex_lock (&mutexLock);
             thread_glb.connection_status_request = nothing;
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
                 if (parse(cluster_str)>0) {
                   message(LBL_ERR_PARSE,0);
                   return;
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
}

void* telnet(void* unused) {

    int sock;
    struct sockaddr_in server;

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        message(LBL_ERR_SOCKET,3);
        //return 1;
        gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);
        return NULL;
    }

    server.sin_addr.s_addr = inet_addr(thread_glb.entry_server);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(thread_glb.entry_port));

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
        message(LBL_ERR_CONN,3);
        //return 1;
        gtk_button_set_label(widgets->g_btn_connect,LBL_CONNECT);
        return NULL;
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
    //return 0;
    //return NULL;
    pthread_exit((void*) 0);
}

