/******************************************************************************
Utility module
  this module contains generic function that could be used also in other
  applications
******************************************************************************/
#include <ctype.h>
#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/*-----------------------------------------------------------------------------
return a new string with every instance of ch replaced by repl
-----------------------------------------------------------------------------*/
char* replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos){
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

/*-----------------------------------------------------------------------------
concatenate two string
-----------------------------------------------------------------------------*/
char* concat(const char *s1, const char *s2) {
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

/*-----------------------------------------------------------------------------
check if a string contains only numbers
-----------------------------------------------------------------------------*/
int isnumber(const char*s) {
   char* e = NULL;
   (void) strtol(s, &e, 0);
   return e != NULL && *e == (char)0;
}

/*-----------------------------------------------------------------------------
clear string removing fist 31 ascii characters
-----------------------------------------------------------------------------*/
char *clrStr(char *someString) {
  int ln,i,j=0;
  ln=strlen(someString);
  char *new = malloc(ln+1);
  for (i=0;i<ln;i++) {
    if (someString[i]>31) {
      new[j]=someString[i];
      j++;
    }
  }

  new[j+1]='\0';
  return new;
}

/*-----------------------------------------------------------------------------
mid function
-----------------------------------------------------------------------------*/
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen) {
   size_t len = min( dstlen - 1, length);

        strncpy(dst, src + start, len);
        // zero terminate because strncpy() didn't ?
        if(len < length)
                dst[dstlen-1] = 0;
}

/*-----------------------------------------------------------------------------
trim function
got from: https://github.com/fnoyanisi/zString
-----------------------------------------------------------------------------*/
char *zstring_trim(char *str){
    char *src=str;  /* save the original pointer */
    char *dst=str;  /* result */
    char c;
    int is_space=0;
    int in_word=0;  /* word boundary logical check */
    int index=0;    /* index of the last non-space char*/

    /* validate input */
    if (!str)
        return str;

    while ((c=*src)){
        is_space=0;

        if (c=='\t' || c=='\v' || c=='\f' || c=='\n' || c=='\r' || c==' ')
            is_space=1;

        if(is_space == 0){
         /* Found a word */
            in_word = 1;
            *dst++ = *src++;  /* make the assignment first
                               * then increment
                               */
        } else if (is_space==1 && in_word==0) {
         /* Already going through a series of white-spaces */
            in_word=0;
            ++src;
        } else if (is_space==1 && in_word==1) {
         /* End of a word, dont mind copy white spaces here */
            in_word=0;
            *dst++ = *src++;
            index = (dst-str)-1; /* location of the last char */
        }
    }

    /* terminate the string */
    *(str+index)='\0';

    return str;
}
