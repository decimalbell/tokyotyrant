/*************************************************************************************************
 * The test cases of the remote database API
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


#include <tcrdb.h>
#include "myconf.h"

#define DEFPORT        1978              // default port
#define RECBUFSIZ      32                // buffer for records

typedef struct {                         // type of structure for write thread
  TCRDB *rdb;
  int rnum;
  bool nr;
  bool rnd;
  int id;
} TARGWRITE;

typedef struct {                         // type of structure for read thread
  TCRDB *rdb;
  int rnum;
  int mul;
  bool rnd;
  int id;
} TARGREAD;

typedef struct {                         // type of structure for remove thread
  TCRDB *rdb;
  int rnum;
  bool rnd;
  int id;
} TARGREMOVE;


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void eprint(TCRDB *rdb, const char *func);
static int myrand(int range);
static int myrandnd(int range);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int procwrite(const char *host, int port, int tnum, int rnum, bool nr, bool rnd);
static int procread(const char *host, int port, int tnum, int mul, bool rnd);
static int procremove(const char *host, int port, int tnum, bool rnd);
static void *threadwrite(void *targ);
static void *threadread(void *targ);
static void *threadremove(void *targ);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  srand((unsigned int)(tctime() * 1000) % UINT_MAX);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "remove")){
    rv = runremove(argc, argv);
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
  fprintf(stderr, "  %s write [-port num] [-tnum num] [-nr] [-rnd] host rnum\n", g_progname);
  fprintf(stderr, "  %s read [-port num] [-tnum num] [-mul num] host\n", g_progname);
  fprintf(stderr, "  %s remove [-port num] [-tnum num] host\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print formatted information string and flush the buffer */
static void iprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}


/* print error message of abstract database */
static void eprint(TCRDB *rdb, const char *func){
  int ecode = tcrdbecode(rdb);
  fprintf(stderr, "%s: %s: error: %d: %s\n", g_progname, func, ecode, tcrdberrmsg(ecode));
}


/* get a random number */
static int myrand(int range){
  return (int)((double)range * rand() / (RAND_MAX + 1.0));
}


/* get a random number based on normal distribution */
static int myrandnd(int range){
  int num = (int)tcdrandnd(range >> 1, range / 10);
  return (num < 0 || num >= range) ? 0 : num;
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *host = NULL;
  char *rstr = NULL;
  int port = DEFPORT;
  int tnum = 1;
  bool nr = false;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-tnum")){
        if(++i >= argc) usage();
        tnum = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-nr")){
        nr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!host || !rstr || tnum < 1) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procwrite(host, port, tnum, rnum, nr, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  int tnum = 1;
  int mul = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-tnum")){
        if(++i >= argc) usage();
        tnum = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-mul")){
        if(++i >= argc) usage();
        mul = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host || tnum < 1) usage();
  int rv = procread(host, port, tnum, mul, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *host = NULL;
  int port = DEFPORT;
  int tnum = 1;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!host && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-port")){
        if(++i >= argc) usage();
        port = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-tnum")){
        if(++i >= argc) usage();
        tnum = atoi(argv[i]);
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!host){
      host = argv[i];
    } else {
      usage();
    }
  }
  if(!host || tnum < 1) usage();
  int rv = procremove(host, port, tnum, rnd);
  return rv;
}


/* perform write command */
static int procwrite(const char *host, int port, int tnum, int rnum, bool nr, bool rnd){
  iprintf("<Writing Test>\n  host=%s  port=%d  tnum=%d  rnum=%d  nr=%d  rnd=%d\n\n",
          host, port, tnum, rnum, nr, rnd);
  bool err = false;
  double stime = tctime();
  TCRDB *rdbs[tnum];
  for(int i = 0; i < tnum; i++){
    rdbs[i] = tcrdbnew();
    if(!tcrdbopen(rdbs[i], host, port)){
      eprint(rdbs[i], "tcrdbopen");
      err = true;
    }
  }
  TCRDB *rdb = rdbs[0];
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].rdb = rdbs[0];
    targs[0].rnum = rnum;
    targs[0].nr = nr;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].rdb = rdbs[i];
      targs[i].rnum = rnum;
      targs[i].nr = nr;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(rdb, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(rdb, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcrdbrnum(rdb));
  iprintf("size: %llu\n", (unsigned long long)tcrdbsize(rdb));
  for(int i = 0; i < tnum; i++){
    if(!tcrdbclose(rdbs[i])){
      eprint(rdbs[i], "tcrdbclose");
      err = true;
    }
    tcrdbdel(rdbs[i]);
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *host, int port, int tnum, int mul, bool rnd){
  iprintf("<Reading Test>\n  host=%s  port=%d  tnum=%d  mul=%d  rnd=%d\n\n",
          host, port, tnum, mul, rnd);
  bool err = false;
  double stime = tctime();
  TCRDB *rdbs[tnum];
  for(int i = 0; i < tnum; i++){
    rdbs[i] = tcrdbnew();
    if(!tcrdbopen(rdbs[i], host, port)){
      eprint(rdbs[i], "tcrdbopen");
      err = true;
    }
  }
  TCRDB *rdb = rdbs[0];
  int rnum = tcrdbrnum(rdb) / tnum;
  TARGREAD targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].rdb = rdbs[0];
    targs[0].rnum = rnum;
    targs[0].mul = mul;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].rdb = rdbs[i];
      targs[i].rnum = rnum;
      targs[i].mul = mul;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(rdb, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(rdb, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcrdbrnum(rdb));
  iprintf("size: %llu\n", (unsigned long long)tcrdbsize(rdb));
  for(int i = 0; i < tnum; i++){
    if(!tcrdbclose(rdbs[i])){
      eprint(rdbs[i], "tcrdbclose");
      err = true;
    }
    tcrdbdel(rdbs[i]);
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *host, int port, int tnum, bool rnd){
  iprintf("<Removing Test>\n  host=%s  port=%d  tnum=%d  rnd=%d\n\n", host, port, tnum, rnd);
  bool err = false;
  double stime = tctime();
  TCRDB *rdbs[tnum];
  for(int i = 0; i < tnum; i++){
    rdbs[i] = tcrdbnew();
    if(!tcrdbopen(rdbs[i], host, port)){
      eprint(rdbs[i], "tcrdbopen");
      err = true;
    }
  }
  TCRDB *rdb = rdbs[0];
  int rnum = tcrdbrnum(rdb) / tnum;
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].rdb = rdbs[0];
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].rdb = rdbs[i];
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(rdb, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(rdb, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcrdbrnum(rdb));
  iprintf("size: %llu\n", (unsigned long long)tcrdbsize(rdb));
  for(int i = 0; i < tnum; i++){
    if(!tcrdbclose(rdbs[i])){
      eprint(rdbs[i], "tcrdbclose");
      err = true;
    }
    tcrdbdel(rdbs[i]);
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* thread the write function */
static void *threadwrite(void *targ){
  TCRDB *rdb = ((TARGWRITE *)targ)->rdb;
  int rnum = ((TARGWRITE *)targ)->rnum;
  bool nr = ((TARGWRITE *)targ)->nr;
  bool rnd = ((TARGWRITE *)targ)->rnd;
  int id = ((TARGWRITE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + (rnd ? myrand(i) + 1 : i));
    if(nr){
      if(!tcrdbputnr(rdb, buf, len, buf, len)){
        eprint(rdb, "tcrdbputnr");
        err = true;
        break;
      }
    } else {
      if(!tcrdbput(rdb, buf, len, buf, len)){
        eprint(rdb, "tcrdbput");
        err = true;
        break;
      }
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the read function */
static void *threadread(void *targ){
  TCRDB *rdb = ((TARGREAD *)targ)->rdb;
  int rnum = ((TARGREAD *)targ)->rnum;
  int mul = ((TARGREAD *)targ)->mul;
  bool rnd = ((TARGREAD *)targ)->rnd;
  int id = ((TARGREAD *)targ)->id;
  bool err = false;
  int base = id * rnum;
  TCMAP *recs = mul > 1 ? tcmapnew() : NULL;
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrandnd(i) + 1 : i));
    if(mul > 1){
      tcmapput(recs, kbuf, ksiz, kbuf, ksiz);
      if(i % mul == 0){
        if(!tcrdbget3(rdb, recs)){
          eprint(rdb, "tcrdbget3");
          err = true;
          break;
        }
        tcmapclear(recs);
      }
    } else {
      int vsiz;
      char *vbuf = tcrdbget(rdb, kbuf, ksiz, &vsiz);
      if(!vbuf && !rnd){
        eprint(rdb, "tcrdbget");
        err = true;
        break;
      }
      tcfree(vbuf);
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(recs) tcmapdel(recs);
  return err ? "error" : NULL;
}


/* thread the remove function */
static void *threadremove(void *targ){
  TCRDB *rdb = ((TARGREMOVE *)targ)->rdb;
  int rnum = ((TARGREMOVE *)targ)->rnum;
  bool rnd = ((TARGREMOVE *)targ)->rnd;
  int id = ((TARGREMOVE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrandnd(i) + 1 : i));
    if(!tcrdbout(rdb, kbuf, ksiz) && !rnd){
      eprint(rdb, "tcrdbout");
      printf("[%s]\n", kbuf);
      err = true;
      break;
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}



// END OF FILE
