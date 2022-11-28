/*************************************************************************************************
 * The remote database API of Tokyo Tyrant
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


#include "ttutil.h"
#include "tcrdb.h"
#include "myconf.h"



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Get the message string corresponding to an error code. */
const char *tcrdberrmsg(int ecode){
  switch(ecode){
  case TTESUCCESS: return "success";
  case TTEINVALID: return "invalid operation";
  case TTENOHOST: return "host not found";
  case TTEREFUSED: return "connection refused";
  case TTESEND: return "send error";
  case TTERECV: return "recv error";
  case TTEKEEP: return "existing record";
  case TTENOREC: return "no record found";
  case TTEMISC: return "miscellaneous error";
  }
  return "unknown error";
}


/* Create a remote database object. */
TCRDB *tcrdbnew(void){
  TCRDB *rdb = tcmalloc(sizeof(*rdb));
  rdb->fd = -1;
  rdb->sock = NULL;
  rdb->ecode = TTESUCCESS;
  return rdb;
}


/* Delete a remote database object. */
void tcrdbdel(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd >= 0) tcrdbclose(rdb);
  tcfree(rdb);
}


/* Get the last happened error code of a remote database object. */
int tcrdbecode(TCRDB *rdb){
  assert(rdb);
  return rdb->ecode;
}


/* Open a remote database. */
bool tcrdbopen(TCRDB *rdb, const char *host, int port){
  assert(rdb && host);
  if(rdb->fd >= 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  int fd;
  if(port < 1){
    fd = ttopensockunix(host);
  } else {
    char addr[TTADDRBUFSIZ];
    if(!ttgethostaddr(host, addr)){
      rdb->ecode = TTENOHOST;
      return false;
    }
    fd = ttopensock(addr, port);
  }
  if(fd == -1){
    rdb->ecode = TTEREFUSED;
    return false;
  }
  rdb->fd = fd;
  rdb->sock = ttsocknew(fd);
  return true;
}


/* Close a remote database object. */
bool tcrdbclose(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  ttsockdel(rdb->sock);
  if(!ttclosesock(rdb->fd)){
    rdb->ecode = TTEMISC;
    err = true;
  }
  rdb->fd = -1;
  rdb->sock = NULL;
  return !err;
}


/* Store a record into a remote database object. */
bool tcrdbput(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(rdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) * 2 + ksiz + vsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDPUT;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)vsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  wp += vsiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  } else {
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Store a string record into a remote object. */
bool tcrdbput2(TCRDB *rdb, const char *kstr, const char *vstr){
  assert(rdb && kstr && vstr);
  return tcrdbput(rdb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Store a new record into a remote database object. */
bool tcrdbputkeep(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(rdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) * 2 + ksiz + vsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDPUTKEEP;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)vsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  wp += vsiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  } else {
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEKEEP;
      err = true;
    }
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Store a new string record into a remote database object. */
bool tcrdbputkeep2(TCRDB *rdb, const char *kstr, const char *vstr){
  assert(rdb && kstr && vstr);
  return tcrdbputkeep(rdb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Concatenate a value at the end of the existing record in a remote database object. */
bool tcrdbputcat(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(rdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) * 2 + ksiz + vsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDPUTCAT;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)vsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  wp += vsiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  } else {
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Concatenate a string value at the end of the existing record in a remote database object. */
bool tcrdbputcat2(TCRDB *rdb, const char *kstr, const char *vstr){
  assert(rdb && kstr && vstr);
  return tcrdbputcat(rdb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Concatenate a value at the end of the existing record and rotate it to the left. */
bool tcrdbputrtt(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz, int width){
  assert(rdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0 && width >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) * 3 + ksiz + vsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDPUTRTT;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)vsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)width);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  wp += vsiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  } else {
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Concatenate a string value at the end of the existing record and rotate it to the left. */
bool tcrdbputrtt2(TCRDB *rdb, const char *kstr, const char *vstr, int width){
  assert(rdb && kstr && vstr);
  return tcrdbputrtt(rdb, kstr, strlen(kstr), vstr, strlen(vstr), width);
}


/* Store a record into a remote database object without repsponse from the server. */
bool tcrdbputnr(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(rdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) * 2 + ksiz + vsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDPUTNR;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)vsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  wp += vsiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Store a string record into a remote object without response from the server. */
bool tcrdbputnr2(TCRDB *rdb, const char *kstr, const char *vstr){
  assert(rdb && kstr && vstr);
  return tcrdbputnr(rdb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Remove a record of a remote database object. */
bool tcrdbout(TCRDB *rdb, const void *kbuf, int ksiz){
  assert(rdb && kbuf && ksiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  bool err = false;
  int rsiz = 2 + sizeof(uint32_t) + ksiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDOUT;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  if(!ttsocksend(rdb->sock, buf, wp - buf)){
    rdb->ecode = TTESEND;
    err = true;
  } else {
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
      err = true;
    }
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Remove a string record of a remote database object. */
bool tcrdbout2(TCRDB *rdb, const char *kstr){
  assert(rdb && kstr);
  return tcrdbout(rdb, kstr, strlen(kstr));
}


/* Retrieve a record in a remote database object. */
void *tcrdbget(TCRDB *rdb, const void *kbuf, int ksiz, int *sp){
  assert(rdb && kbuf && ksiz >= 0 && sp);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  char *vbuf = NULL;
  int rsiz = 2 + sizeof(uint32_t) + ksiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDGET;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      int vsiz = ttsockgetint32(rdb->sock);
      if(!ttsockcheckend(rdb->sock) && vsiz >= 0){
        vbuf = tcmalloc(vsiz + 1);
        if(ttsockrecv(rdb->sock, vbuf, vsiz)){
          vbuf[vsiz] = '\0';
          *sp = vsiz;
        } else {
          rdb->ecode = TTERECV;
          tcfree(vbuf);
          vbuf = NULL;
        }
      } else {
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  pthread_cleanup_pop(1);
  return vbuf;
}


/* Retrieve a string record in a remote database object. */
char *tcrdbget2(TCRDB *rdb, const char *kstr){
  assert(rdb && kstr);
  int vsiz;
  return tcrdbget(rdb, kstr, strlen(kstr), &vsiz);
}


/* Retrieve records in a remote database object. */
bool tcrdbget3(TCRDB *rdb, TCMAP *recs){
  assert(rdb && recs);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  bool err = false;
  TCXSTR *xstr = tcxstrnew();
  pthread_cleanup_push((void (*)(void *))tcxstrdel, xstr);
  uint8_t magic[2];
  magic[0] = TTMAGICNUM;
  magic[1] = TTCMDMGET;
  tcxstrcat(xstr, magic, sizeof(magic));
  uint32_t num;
  num = (uint32_t)tcmaprnum(recs);
  num = TTHTONL(num);
  tcxstrcat(xstr, &num, sizeof(num));
  tcmapiterinit(recs);
  const char *kbuf;
  int ksiz;
  while((kbuf = tcmapiternext(recs, &ksiz)) != NULL){
    num = TTHTONL(ksiz);
    tcxstrcat(xstr, &num, sizeof(num));
    tcxstrcat(xstr, kbuf, ksiz);
  }
  tcmapclear(recs);
  char stack[TTIOBUFSIZ];
  if(ttsocksend(rdb->sock, tcxstrptr(xstr), tcxstrsize(xstr))){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      int rnum = ttsockgetint32(rdb->sock);
      if(!ttsockcheckend(rdb->sock) && rnum >= 0){
        for(int i = 0; i < rnum; i++){
          int rksiz = ttsockgetint32(rdb->sock);
          int rvsiz = ttsockgetint32(rdb->sock);
          if(ttsockcheckend(rdb->sock)){
            rdb->ecode = TTERECV;
            err = true;
            break;
          }
          int rsiz = rksiz + rvsiz;
          char *rbuf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz + 1);
          if(ttsockrecv(rdb->sock, rbuf, rsiz)){
            tcmapput(recs, rbuf, rksiz, rbuf + rksiz, rvsiz);
          } else {
            rdb->ecode = TTERECV;
            err = true;
          }
          if(rbuf != stack) tcfree(rbuf);
        }
      } else {
        rdb->ecode = TTERECV;
        err = true;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Get the size of the value of a record in a remote database object. */
int tcrdbvsiz(TCRDB *rdb, const void *kbuf, int ksiz){
  assert(rdb && kbuf && ksiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return -1;
  }
  int vsiz = -1;
  int rsiz = 2 + sizeof(uint32_t) + ksiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDVSIZ;
  uint32_t num;
  num = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      vsiz = ttsockgetint32(rdb->sock);
      if(ttsockcheckend(rdb->sock)){
        rdb->ecode = TTERECV;
        vsiz = -1;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  pthread_cleanup_pop(1);
  return vsiz;
}


/* Get the size of the value of a string record in a remote database object. */
int tcrdbvsiz2(TCRDB *rdb, const char *kstr){
  assert(rdb && kstr);
  return tcrdbvsiz(rdb, kstr, strlen(kstr));
}


/* Initialize the iterator of a remote database object. */
bool tcrdbiterinit(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  bool err = false;
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDITERINIT;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  return !err;
}


/* Get the next key of the iterator of a remote database object. */
void *tcrdbiternext(TCRDB *rdb, int *sp){
  assert(rdb && sp);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  char *vbuf = NULL;
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDITERNEXT;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      int vsiz = ttsockgetint32(rdb->sock);
      if(!ttsockcheckend(rdb->sock) && vsiz >= 0){
        vbuf = tcmalloc(vsiz + 1);
        if(ttsockrecv(rdb->sock, vbuf, vsiz)){
          vbuf[vsiz] = '\0';
          *sp = vsiz;
        } else {
          rdb->ecode = TTERECV;
          tcfree(vbuf);
          vbuf = NULL;
        }
      } else {
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  return vbuf;
}


/* Get the next key string of the iterator of a remote database object. */
char *tcrdbiternext2(TCRDB *rdb){
  assert(rdb);
  int vsiz;
  return tcrdbiternext(rdb, &vsiz);
}


/* Get forward matching keys in a remote database object. */
TCLIST *tcrdbfwmkeys(TCRDB *rdb, const void *pbuf, int psiz, int max){
  assert(rdb && pbuf && psiz >= 0);
  TCLIST *keys = tclistnew();
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return keys;
  }
  int rsiz = 2 + sizeof(uint32_t) * 2 + psiz;
  if(max < 0) max = INT_MAX;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDFWMKEYS;
  uint32_t num;
  num = TTHTONL((uint32_t)psiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)max);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, pbuf, psiz);
  wp += psiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      int knum = ttsockgetint32(rdb->sock);
      if(!ttsockcheckend(rdb->sock) && knum >= 0){
        for(int i = 0; i < knum; i++){
          int ksiz = ttsockgetint32(rdb->sock);
          if(ttsockcheckend(rdb->sock)){
            rdb->ecode = TTERECV;
            break;
          }
          char *kbuf = (ksiz < TTIOBUFSIZ) ? stack : tcmalloc(ksiz + 1);
          if(ttsockrecv(rdb->sock, kbuf, ksiz)){
            tclistpush(keys, kbuf, ksiz);
          } else {
            rdb->ecode = TTERECV;
          }
          if(kbuf != (char *)stack) tcfree(kbuf);
        }
      } else {
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTENOREC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  pthread_cleanup_pop(1);
  return keys;
}


/* Get forward matching string keys in a remote database object. */
TCLIST *tcrdbfwmkeys2(TCRDB *rdb, const char *pstr, int max){
  assert(rdb && pstr);
  return tcrdbfwmkeys(rdb, pstr, strlen(pstr), max);
}


/* Add an integer to a record in a remote database object. */
int tcrdbaddint(TCRDB *rdb, const void *kbuf, int ksiz, int num){
  assert(rdb && kbuf && ksiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  int sum = INT_MIN;
  int rsiz = 2 + sizeof(uint32_t) * 2 + ksiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDADDINT;
  uint32_t lnum;
  lnum = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &lnum, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  lnum = TTHTONL((uint32_t)num);
  memcpy(wp, &lnum, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      sum = ttsockgetint32(rdb->sock);
      if(ttsockcheckend(rdb->sock)){
        rdb->ecode = TTERECV;
        sum = -1;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTEKEEP;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  pthread_cleanup_pop(1);
  return sum;
}


/* Add a real number to a record in a remote database object. */
double tcrdbadddouble(TCRDB *rdb, const void *kbuf, int ksiz, double num){
  assert(rdb && kbuf && ksiz >= 0);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return false;
  }
  double sum = NAN;
  int rsiz = 2 + sizeof(uint32_t) + sizeof(uint64_t) * 2 + ksiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDADDDOUBLE;
  uint32_t lnum;
  lnum = TTHTONL((uint32_t)ksiz);
  memcpy(wp, &lnum, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  char dbuf[sizeof(uint64_t)*2];
  ttpackdouble(num, (char *)wp);
  wp += sizeof(dbuf);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      if(ttsockrecv(rdb->sock, dbuf, sizeof(dbuf)) && !ttsockcheckend(rdb->sock)){
        sum = ttunpackdouble(dbuf);
      } else {
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTEKEEP;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  pthread_cleanup_pop(1);
  return sum;
}


/* Synchronize updated contents of a remote database object with the file and the device. */
bool tcrdbsync(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  bool err = false;
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDSYNC;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  return !err;
}


/* Remove all records of a remote database object. */
bool tcrdbvanish(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  bool err = false;
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDVANISH;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  return !err;
}


/* Copy the database file of a remote database object. */
bool tcrdbcopy(TCRDB *rdb, const char *path){
  assert(rdb && path);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  bool err = false;
  int psiz = strlen(path);
  int rsiz = 2 + sizeof(uint32_t) + psiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDCOPY;
  uint32_t num;
  num = TTHTONL((uint32_t)psiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, path, psiz);
  wp += psiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Restore the database file of a remote database object from the update log. */
bool tcrdbrestore(TCRDB *rdb, const char *path, uint64_t ts){
  assert(rdb && path);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  bool err = false;
  int psiz = strlen(path);
  int rsiz = 2 + sizeof(uint32_t) + sizeof(uint64_t) + psiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDRESTORE;
  uint32_t lnum = TTHTONL((uint32_t)psiz);
  memcpy(wp, &lnum, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  uint64_t llnum = TTHTONLL(ts);
  memcpy(wp, &llnum, sizeof(uint64_t));
  wp += sizeof(uint64_t);
  memcpy(wp, path, psiz);
  wp += psiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Set the replication master of a remote database object from the update log. */
bool tcrdbsetmst(TCRDB *rdb, const char *host, int port){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return NULL;
  }
  if(!host) host = "";
  if(port < 0) port = 0;
  bool err = false;
  int hsiz = strlen(host);
  int rsiz = 2 + sizeof(uint32_t) * 2 + hsiz;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDSETMST;
  uint32_t num;
  num = TTHTONL((uint32_t)hsiz);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  num = TTHTONL((uint32_t)port);
  memcpy(wp, &num, sizeof(uint32_t));
  wp += sizeof(uint32_t);
  memcpy(wp, host, hsiz);
  wp += hsiz;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code != 0){
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
      err = true;
    }
  } else {
    rdb->ecode = TTESEND;
    err = true;
  }
  pthread_cleanup_pop(1);
  return !err;
}


/* Get the number of records of a remote database object. */
uint64_t tcrdbrnum(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDRNUM;
  uint64_t rnum = 0;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      rnum = ttsockgetint64(rdb->sock);
      if(ttsockcheckend(rdb->sock)){
        rnum = 0;
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  return rnum;
}


/* Get the size of the database of a remote database object. */
uint64_t tcrdbsize(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDSIZE;
  uint64_t size = 0;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      size = ttsockgetint64(rdb->sock);
      if(ttsockcheckend(rdb->sock)){
        size = 0;
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  return size;
}


/* Get the status string of the database of a remote database object. */
char *tcrdbstat(TCRDB *rdb){
  assert(rdb);
  if(rdb->fd < 0){
    rdb->ecode = TTEINVALID;
    return 0;
  }
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDSTAT;
  uint32_t size = 0;
  if(ttsocksend(rdb->sock, buf, wp - buf)){
    int code = ttsockgetc(rdb->sock);
    if(code == 0){
      size = ttsockgetint32(rdb->sock);
      if(ttsockcheckend(rdb->sock) || size >= TTIOBUFSIZ ||
         !ttsockrecv(rdb->sock, (char *)buf, size)){
        size = 0;
        rdb->ecode = TTERECV;
      }
    } else {
      rdb->ecode = (code == -1) ? TTERECV : TTEMISC;
    }
  } else {
    rdb->ecode = TTESEND;
  }
  if(size < 1) return NULL;
  return tcmemdup(buf, size);
}



// END OF FILE
