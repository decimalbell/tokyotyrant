/*************************************************************************************************
 * The command line utility of the remote database API
 *                                                      Copyright (C) 2007-2008 Mikio Hirabayashi
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
#include <tcrdb.h>
#include "myconf.h"

#define DEFPORT        1978              // default port
#define REQHEADMAX     32                // maximum number of request headers of HTTP
#define MINIBNUM       31                // bucket number of map for trivial use


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(TCRDB *rdb);
static int printdata(const char *ptr, int size, bool px);
static char *hextoobj(const char *str, int *sp);
static int runinform(int argc, char **argv);
static int runput(int argc, char **argv);
static int runout(int argc, char **argv);
static int runget(int argc, char **argv);
static int runmget(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runsync(int argc, char **argv);
static int runvanish(int argc, char **argv);
static int runcopy(int argc, char **argv);
static int runrestore(int argc, char **argv);
static int runsetmst(int argc, char **argv);
static int runrepl(int argc, char **argv);
static int runhttp(int argc, char **argv);
static int runversion(int argc, char **argv);
static int procinform(const char *host, int port, bool st);
static int procput(const char *host, int port, const char *kbuf, int ksiz,
                   const char *vbuf, int vsiz, int dmode);
static int procout(const char *host, int port, const char *kbuf, int ksiz);
static int procget(const char *host, int port, const char *kbuf, int ksiz, bool px, bool pz);
static int procmget(const char *host, int port, const TCLIST *keys, bool px);
static int proclist(const char *host, int port, int max, bool pv, bool px, const char *fmstr);
static int procsync(const char *host, int port);
static int procvanish(const char *host, int port);
static int proccopy(const char *host, int port, const char *dpath);
static int procrestore(const char *host, int port, const char *upath, uint64_t ts);
static int procsetmst(const char *host, int port, const char *mhost, int mport);
static int procrepl(const char *host, int port, uint64_t ts, uint32_t sid, bool ph);
static int prochttp(const char *url, TCMAP *hmap, bool ih);
static int procversion(void);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "inform")){
    rv = runinform(argc, argv);
  } else if(!strcmp(argv[1], "put")){
    rv = runput(argc, argv);
  } else if(!strcmp(argv[1], "out")){
    rv = runout(argc, argv);
  } else if(!strcmp(argv[1], "get")){
    rv = runget(argc, argv);
  } else if(!strcmp(argv[1], "mget")){
    rv = runmget(argc, argv);
  } else if(!strcmp(argv[1], "list")){
    rv = runlist(argc, argv);
  } else if(!strcmp(argv[1], "sync")){
    rv = runsync(argc, argv);
  } else if(!strcmp(argv[1], "vanish")){
    rv = runvanish(argc, argv);
  } else if(!strcmp(argv[1], "copy")){
    rv = runcopy(argc, argv);
  } else if(!strcmp(argv[1], "restore")){
    rv = runrestore(argc, argv);
  } else if(!strcmp(argv[1], "setmst")){
    rv = runsetmst(argc, argv);
  } else if(!strcmp(argv[1], "repl")){
    rv = runrepl(argc, argv);
  } else if(!strcmp(argv[1], "http")){
    rv = runhttp(argc, argv);
  } else if(!strcmp(argv[1], "version") || !strcmp(argv[1], "--version")){
    rv = runversion(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: the command line utility of the remote database API\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s inform [-port num] [-st] host\n", g_progname);
  fprintf(stderr, "  %s put [-port num] [-sx] [-dk|-dc] host key value\n", g_progname);
  fprintf(stderr, "  %s out [-port num] [-sx] host key\n", g_progname);
  fprintf(stderr, "  %s get [-port num] [-sx] [-px] [-pz] host key\n", g_progname);
  fprintf(stderr, "  %s mget [-port num] [-sx] [-px] host [key...]\n", g_progname);
  fprintf(stderr, "  %s list [-port num] [-m num] [-pv] [-px] [-fm str] host\n", g_progname);
  fprintf(stderr, "  %s sync [-port num] host\n", g_progname);
  fprintf(stderr, "  %s vanish [-port num] host\n", g_progname);
  fprintf(stderr, "  %s copy [-port num] host dpath\n", g_progname);
  fprintf(stderr, "  %s restore [-port num] [-ts num] host upath\n", g_progname);
  fprintf(stderr, "  %s setmst [-port num] [-mport num] host [mhost]\n", g_progname);
  fprintf(stderr, "  %s repl [-port num] [-ts num] [-sid num] [-ph] host\n", g_progname);
  fprintf(stderr, "  %s http [-ah name value] [-ih] url\n", g_progname);
  fprintf(stderr, "  %s version\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print error information */
static void printerr(TCRDB *rdb){
  int ecode = tcrdbecode(rdb);
  fprintf(stderr, "%s: error: %d: %s\n", g_progname, ecode, tcrdberrmsg(ecode));
}


/* print record data */
static int printdata(const char *ptr, int size, bool px){
  int len = 0;
  while(size-- > 0){
    if(px){
      if(len > 0) putchar(' ');
      len += printf("%02X", *(unsigned char *)ptr);
    } else {
      putchar(*ptr);
      len++;
    }
    ptr++;
  }
  return len;
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


/* parse arguments of inform command */
static int runinform(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  bool st = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-st")){
        st = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = procinform(host, port, st);
  return rv;
}


/* parse arguments of put command */
static int runput(int argc, char **argv){
  char *host = NULL;
  char *key = NULL;
  char *value = NULL;
  int port = DEFPORT;
  int dmode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!key){
      key = argv[i];
    } else if(!value){
      value = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !key || !value) usage();
  int ksiz, vsiz;
  char *kbuf, *vbuf;
  if(sx){
    kbuf = hextoobj(key, &ksiz);
    vbuf = hextoobj(value, &vsiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
    vsiz = strlen(value);
    vbuf = tcmemdup(value, vsiz);
  }
  int rv = procput(host, port, kbuf, ksiz, vbuf, vsiz, dmode);
  tcfree(vbuf);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of out command */
static int runout(int argc, char **argv){
  char *host = NULL;
  char *key = NULL;
  int port = DEFPORT;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = hextoobj(key, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  int rv = procout(host, port, kbuf, ksiz);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of get command */
static int runget(int argc, char **argv){
  char *host = NULL;
  char *key = NULL;
  int port = DEFPORT;
  bool sx = false;
  bool px = false;
  bool pz = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-pz")){
        pz = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = hextoobj(key, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  int rv = procget(host, port, kbuf, ksiz, px, pz);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of mget command */
static int runmget(int argc, char **argv){
  char *host = NULL;
  TCLIST *keys = tcmpoollistnew(tcmpoolglobal());
  int port = DEFPORT;
  bool sx = false;
  bool px = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      tclistpush2(keys, argv[i]);
    }
  }
  if(!host) usage();
  if(sx){
    for(int i = 0; i < tclistnum(keys); i++){
      int ksiz;
      char *kbuf = hextoobj(tclistval2(keys, i), &ksiz);
      tclistover(keys, i, kbuf, ksiz);
      tcfree(kbuf);
    }
  }
  int rv = procmget(host, port, keys, px);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  int max = -1;
  bool pv = false;
  bool px = false;
  char *fmstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-m")){
        if(++i >= argc) usage();
        max = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-pv")){
        pv = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-fm")){
        if(++i >= argc) usage();
        fmstr = argv[i];
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = proclist(host, port, max, pv, px, fmstr);
  return rv;
}


/* parse arguments of sync command */
static int runsync(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = procsync(host, port);
  return rv;
}


/* parse arguments of vanish command */
static int runvanish(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = procvanish(host, port);
  return rv;
}


/* parse arguments of copy command */
static int runcopy(int argc, char **argv){
  char *host = NULL;
  char *dpath = NULL;
  int port = DEFPORT;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!dpath){
      dpath = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !dpath) usage();
  int rv = proccopy(host, port, dpath);
  return rv;
}


/* parse arguments of restore command */
static int runrestore(int argc, char **argv){
  char *host = NULL;
  char *upath = NULL;
  int port = DEFPORT;
  uint64_t ts = 0;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-ts")){
        if(++i >= argc) usage();
        ts = strtoll(argv[i], NULL, 10);
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!upath){
      upath = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !upath) usage();
  int rv = procrestore(host, port, upath, ts);
  return rv;
}


/* parse arguments of setmst command */
static int runsetmst(int argc, char **argv){
  char *host = NULL;
  char *mhost = NULL;
  int port = DEFPORT;
  int mport = DEFPORT;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-mport")){
        if(++i >= argc) usage();
        mport = atoi(argv[i]);
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!mhost){
      mhost = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = procsetmst(host, port, mhost, mport);
  return rv;
}


/* parse arguments of repl command */
static int runrepl(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  uint64_t ts = 0;
  uint32_t sid = 0;
  bool ph = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-ts")){
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
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host) usage();
  int rv = procrepl(host, port, ts, sid, ph);
  return rv;
}


/* parse arguments of http command */
static int runhttp(int argc, char **argv){
  char *url = NULL;
  struct {
    char *name;
    char *value;
  } heads[REQHEADMAX];
  int hnum = 0;
  bool ih = false;
  for(int i = 2; i < argc; i++){
    if(!url && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-ah")){
        if(++i >= argc) usage();
        char *name = argv[i];
        if(++i >= argc) usage();
        char *value = argv[i];
        if(hnum < REQHEADMAX){
          heads[hnum].name = name;
          heads[hnum].value = value;
          hnum++;
        }
      } else if(!strcmp(argv[i], "-ih")){
        ih = true;
      } else {
        usage();
      }
    } else if(!url){
      url = argv[i];
    } else {
      usage();
    }
  }
  if(!url) usage();
  TCMAP *hmap = tcmapnew2(hnum + 1);
  for(int i = 0; i < hnum; i++){
    tcmapput2(hmap, heads[i].name, heads[i].value);
  }
  int rv = prochttp(url, hmap, ih);
  tcmapdel(hmap);
  return rv;
}


/* parse arguments of version command */
static int runversion(int argc, char **argv){
  int rv = procversion();
  return rv;
}


/* perform inform command */
static int procinform(const char *host, int port, bool st){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(st){
    char *status = tcrdbstat(rdb);
    if(status){
      printf("%s", status);
      tcfree(status);
    } else {
      printerr(rdb);
      err = true;
    }
  } else {
    printf("record number: %llu\n", (unsigned long long)tcrdbrnum(rdb));
    printf("file size: %llu\n", (unsigned long long)tcrdbsize(rdb));
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform put command */
static int procput(const char *host, int port, const char *kbuf, int ksiz,
                   const char *vbuf, int vsiz, int dmode){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  switch(dmode){
  case -1:
    if(!tcrdbputkeep(rdb, kbuf, ksiz, vbuf, vsiz)){
      printerr(rdb);
      err = true;
    }
    break;
  case 1:
    if(!tcrdbputcat(rdb, kbuf, ksiz, vbuf, vsiz)){
      printerr(rdb);
      err = true;
    }
    break;
  default:
    if(!tcrdbput(rdb, kbuf, ksiz, vbuf, vsiz)){
      printerr(rdb);
      err = true;
    }
    break;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform out command */
static int procout(const char *host, int port, const char *kbuf, int ksiz){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbout(rdb, kbuf, ksiz)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform get command */
static int procget(const char *host, int port, const char *kbuf, int ksiz, bool px, bool pz){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  int vsiz;
  char *vbuf = tcrdbget(rdb, kbuf, ksiz, &vsiz);
  if(vbuf){
    printdata(vbuf, vsiz, px);
    if(!pz) putchar('\n');
    tcfree(vbuf);
  } else {
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform mget command */
static int procmget(const char *host, int port, const TCLIST *keys, bool px){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  TCMAP *recs = tcmapnew();
  for(int i = 0; i < tclistnum(keys); i++){
    int ksiz;
    const char *kbuf = tclistval(keys, i, &ksiz);
    tcmapput(recs, kbuf, ksiz, "", 0);
  }
  if(tcrdbget3(rdb, recs)){
    tcmapiterinit(recs);
    const char *kbuf;
    int ksiz;
    while((kbuf = tcmapiternext(recs, &ksiz)) != NULL){
      int vsiz;
      const char *vbuf = tcmapiterval(kbuf, &vsiz);
      printdata(kbuf, ksiz, px);
      putchar('\t');
      printdata(vbuf, vsiz, px);
      putchar('\n');
    }
  } else {
    printerr(rdb);
    err = true;
  }
  tcmapdel(recs);
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform list command */
static int proclist(const char *host, int port, int max, bool pv, bool px, const char *fmstr){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(fmstr){
    TCLIST *keys = tcrdbfwmkeys2(rdb, fmstr, max);
    for(int i = 0; i < tclistnum(keys); i++){
      int ksiz;
      const char *kbuf = tclistval(keys, i, &ksiz);
      printdata(kbuf, ksiz, px);
      if(pv){
        int vsiz;
        char *vbuf = tcrdbget(rdb, kbuf, ksiz, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px);
          tcfree(vbuf);
        }
      }
      putchar('\n');
    }
    tclistdel(keys);
  } else {
    if(!tcrdbiterinit(rdb)){
      printerr(rdb);
      err = true;
    }
    int ksiz;
    char *kbuf;
    int cnt = 0;
    while((kbuf = tcrdbiternext(rdb, &ksiz)) != NULL){
      printdata(kbuf, ksiz, px);
      if(pv){
        int vsiz;
        char *vbuf = tcrdbget(rdb, kbuf, ksiz, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px);
          tcfree(vbuf);
        }
      }
      putchar('\n');
      tcfree(kbuf);
      if(max >= 0 && ++cnt >= max) break;
    }
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform sync command */
static int procsync(const char *host, int port){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbsync(rdb)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform vanish command */
static int procvanish(const char *host, int port){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbvanish(rdb)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform copy command */
static int proccopy(const char *host, int port, const char *dpath){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbcopy(rdb, dpath)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform restore command */
static int procrestore(const char *host, int port, const char *upath, uint64_t ts){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbrestore(rdb, upath, ts)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform setmst command */
static int procsetmst(const char *host, int port, const char *mhost, int mport){
  TCRDB *rdb = tcrdbnew();
  if(!tcrdbopen(rdb, host, port)){
    printerr(rdb);
    tcrdbdel(rdb);
    return 1;
  }
  bool err = false;
  if(!tcrdbsetmst(rdb, mhost, mport)){
    printerr(rdb);
    err = true;
  }
  if(!tcrdbclose(rdb)){
    if(!err) printerr(rdb);
    err = true;
  }
  tcrdbdel(rdb);
  return err ? 1 : 0;
}


/* perform repl command */
static int procrepl(const char *host, int port, uint64_t ts, uint32_t sid, bool ph){
  bool err = false;
  TCREPL *repl = tcreplnew();
  if(tcreplopen(repl, host, port, ts, sid)){
    const char *rbuf;
    int rsiz;
    uint64_t rts;
    uint32_t rsid;
    char stack[TTIOBUFSIZ];
    while((rbuf = tcreplread(repl, &rsiz, &rts, &rsid)) != NULL){
      if(rsiz < 1) continue;
      if(ph){
        printf("%llu\t%u\t", (unsigned long long)rts, (unsigned int)rsid);
        printdata(rbuf, rsiz, true);
        putchar('\n');
      } else {
        int msiz = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) * 2 + rsiz;
        char *mbuf = (msiz < TTIOBUFSIZ) ? stack : tcmalloc(msiz);
        unsigned char *wp = (unsigned char *)mbuf;
        *(wp++) = TCULMAGICNUM;
        uint64_t llnum = TTHTONLL(rts);
        memcpy(wp, &llnum, sizeof(llnum));
        wp += sizeof(llnum);
        uint32_t lnum = TTHTONL(rsid);
        memcpy(wp, &lnum, sizeof(lnum));
        wp += sizeof(lnum);
        lnum = TTHTONL(rsiz);
        memcpy(wp, &lnum, sizeof(lnum));
        wp += sizeof(lnum);
        memcpy(wp, rbuf, rsiz);
        fwrite(mbuf, 1, msiz, stdout);
        fflush(stdout);
        if(mbuf != stack) tcfree(mbuf);
      }
    }
    tcreplclose(repl);
  } else {
    fprintf(stderr, "%s: %s:%d could not be connected\n", g_progname, host, port);
    err = true;
  }
  tcrepldel(repl);
  return err ? 1 : 0;
}


/* perform http command */
static int prochttp(const char *url, TCMAP *hmap, bool ih){
  bool err = false;
  TCMAP *resheads = ih ? tcmapnew2(MINIBNUM) : NULL;
  TCXSTR *body = tcxstrnew();
  int code = tthttpfetch(url, hmap, resheads, body);
  if(code > 0){
    if(resheads){
      printf("%s\n", tcmapget2(resheads, "STATUS"));
      tcmapiterinit(resheads);
      const char *name;
      while((name = tcmapiternext2(resheads)) != NULL){
        if(*name >= 'A' && *name <= 'Z') continue;
        char *cap = tcstrdup(name);
        tcstrtolower(cap);
        char *wp = cap;
        bool head = true;
        while(*wp != '\0'){
          if(head && *wp >= 'a' && *wp <= 'z') *wp -= 'a' - 'A';
          head = *wp == '-' || *wp == ' ';
          wp++;
        }
        printf("%s: %s\n", cap, tcmapiterval2(name));
        tcfree(cap);
      }
      printf("\n");
    }
    fwrite(tcxstrptr(body), 1, tcxstrsize(body), stdout);
  } else {
    fprintf(stderr, "%s: %s: could not be connected\n", g_progname, url);
    err = true;
  }
  tcxstrdel(body);
  if(resheads) tcmapdel(resheads);
  return err ? 1 : 0;
}


/* perform version command */
static int procversion(void){
  printf("Tokyo Tyrant version %s (%d:%s)\n", ttversion, _TT_LIBVER, _TT_PROTVER);
  printf("Copyright (C) 2007-2008 Mikio Hirabayashi\n");
  return 0;
}



// END OF FILE
