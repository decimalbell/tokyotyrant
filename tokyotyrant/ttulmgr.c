/*************************************************************************************************
 * The test cases of the update log API
 *                                                      Copyright (C) 2006-2008 Mikio Hirabayashi
 * This file is part of Tokyo Tyrant.
 * Tokyo Tyrant is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Tyrant is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Tyrant; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#include <tculog.h>
#include "myconf.h"

#define RECBUFSIZ      32                // buffer for records

typedef struct {                         // type of structure for read thread
  TCULRD *ulrd;
  int id;
  int rnum;
} TARGREAD;


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(const char *msg);
static int printhex(const char *ptr, int size);
static char *mygetline(FILE *ifp);
static char *hextoobj(const char *str, int *sp);
static int runexport(int argc, char **argv);
static int runimport(int argc, char **argv);
static int procexport(const char *upath, uint64_t ts, uint32_t sid);
static int procimport(const char *upath, uint64_t lim);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "export")){
    rv = runexport(argc, argv);
  } else if(!strcmp(argv[1], "import")){
    rv = runimport(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: test cases of the remote database API of Tokyo Tyrant\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s export [-ts num] [-sid num] upath\n", g_progname);
  fprintf(stderr, "  %s import upath\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print error information */
static void printerr(const char *msg){
  fprintf(stderr, "%s: error: %s\n", g_progname, msg);
}


/* print record data */
static int printhex(const char *ptr, int size){
  int len = 0;
  while(size-- > 0){
    if(len > 0) putchar(' ');
    len += printf("%02X", *(unsigned char *)ptr);
    ptr++;
  }
  return len;
}


/* read a line from a file descriptor */
static char *mygetline(FILE *ifp){
  int len = 0;
  int blen = 1024;
  char *buf = tcmalloc(blen);
  bool end = true;
  int c;
  while((c = fgetc(ifp)) != EOF){
    end = false;
    if(c == '\0') continue;
    if(blen <= len){
      blen *= 2;
      buf = tcrealloc(buf, blen + 1);
    }
    if(c == '\n' || c == '\r') c = '\0';
    buf[len++] = c;
    if(c == '\0') break;
  }
  if(end){
    tcfree(buf);
    return NULL;
  }
  buf[len] = '\0';
  return buf;
}


/* create a binary object from a hexadecimal string */
static char *hextoobj(const char *str, int *sp){
  int len = strlen(str);
  char *buf = tcmalloc(len + 1);
  int j = 0;
  for(int i = 0; i < len; i += 2){
    while(strchr(" \n\r\t\f\v", str[i])){
      i++;
    }
    char mbuf[3];
    if((mbuf[0] = str[i]) == '\0') break;
    if((mbuf[1] = str[i+1]) == '\0') break;
    mbuf[2] = '\0';
    buf[j++] = (char)strtol(mbuf, NULL, 16);
  }
  buf[j] = '\0';
  *sp = j;
  return buf;
}




/* parse arguments of export command */
static int runexport(int argc, char **argv){
  char *upath = NULL;
  uint64_t ts = 0;
  uint32_t sid = 0;
  bool ph = false;
  for(int i = 2; i < argc; i++){
    if(!upath && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-ts")){
        if(++i >= argc) usage();
        ts = strtoll(argv[i], NULL, 10);
      } else if(!strcmp(argv[i], "-sid")){
        if(++i >= argc) usage();
        sid = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-ph")){
        ph = true;
      } else {
        usage();
      }
    } else if(!upath){
      upath = argv[i];
    } else {
      usage();
    }
  }
  if(!upath) usage();
  int rv = procexport(upath, ts, sid);
  return rv;
}


/* parse arguments of import command */
static int runimport(int argc, char **argv){
  char *upath = NULL;
  uint64_t lim = 0;
  for(int i = 2; i < argc; i++){
    if(!upath && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-lim")){
        if(++i >= argc) usage();
        lim = tcatoi(argv[i]);
      } else {
        usage();
      }
    } else if(!upath){
      upath = argv[i];
    } else {
      usage();
    }
  }
  if(!upath) usage();
  int rv = procimport(upath, lim);
  return rv;
}


/* perform export command */
static int procexport(const char *upath, uint64_t ts, uint32_t sid){
  TCULOG *ulog = tculognew();
  if(!tculogopen(ulog, upath, 0)){
    printerr("tculogopen");
    return 1;
  }
  bool err = false;
  TCULRD *ulrd = tculrdnew(ulog, ts);
  if(ulrd){
    const char *rbuf;
    int rsiz;
    uint64_t rts;
    uint32_t rsid;
    while(!err && (rbuf = tculrdread(ulrd, &rsiz, &rts, &rsid)) != NULL){
      if(rsid == sid) continue;
      printf("%llu\t%u\t", (unsigned long long)rts, (unsigned int)rsid);
      printhex(rbuf, rsiz);
      putchar('\n');
    }
    tculrddel(ulrd);
  } else {
    printerr("tculrdnew");
    err = true;
  }
  if(!tculogclose(ulog)){
    printerr("tculogclose");
    err = true;
  }
  tculogdel(ulog);
  return err ? 1 : 0;
}


/* perform import command */
static int procimport(const char *upath, uint64_t lim){
  TCULOG *ulog = tculognew();
  if(!tculogopen(ulog, upath, lim)){
    printerr("tculogopen");
    return 1;
  }
  bool err = false;
  char *line;
  while(!err && (line = mygetline(stdin)) != NULL){
    uint64_t ts = strtoll(line, NULL, 10);
    char *pv = strchr(line, '\t');
    if(!pv || ts < 1){
      tcfree(line);
      continue;
    }
    pv++;
    uint32_t sid = strtoll(pv, NULL, 10);
    pv = strchr(pv, '\t');
    if(!pv || sid < 1){
      tcfree(line);
      continue;
    }
    pv++;
    int osiz;
    unsigned char *obj = (unsigned char *)hextoobj(pv, &osiz);
    if(!obj || osiz < 3 || *obj != 0xc8){
      tcfree(obj);
      tcfree(line);
      continue;
    }
    if(!tculogwrite(ulog, ts, sid, obj, osiz)){
      printerr("tculogwrite");
      err = true;
    }
    tcfree(obj);
    tcfree(line);
  }
  if(!tculogclose(ulog)){
    printerr("tculogclose");
    err = true;
  }
  tculogdel(ulog);
  return err ? 1 : 0;
}



// END OF FILE
