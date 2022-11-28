/*************************************************************************************************
 * The update log API of Tokyo Tyrant
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
#include "tculog.h"
#include "myconf.h"

#define TCULAIOCBNUM   64                // number of AIO tasks


/* private function prototypes */
static bool tculogflushaiocbp(struct aiocb *aiocbp);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Create an update log object. */
TCULOG *tculognew(void){
  TCULOG *ulog = tcmalloc(sizeof(*ulog));
  for(int i = 0; i < TCULRMTXNUM; i++){
    if(pthread_mutex_init(ulog->rmtxs + i, NULL) != 0) tcmyfatal("pthread_mutex_init failed");
  }
  if(pthread_rwlock_init(&ulog->rwlck, NULL) != 0) tcmyfatal("pthread_rwlock_init failed");
  if(pthread_cond_init(&ulog->cnd, NULL) != 0) tcmyfatal("pthread_cond_init failed");
  if(pthread_mutex_init(&ulog->wmtx, NULL) != 0) tcmyfatal("pthread_mutex_init failed");
  ulog->base = NULL;
  ulog->limsiz = 0;
  ulog->max = 0;
  ulog->fd = -1;
  ulog->size = 0;
  ulog->aiocbs = NULL;
  ulog->aiocbi = 0;
  ulog->aioend = 0;
  return ulog;
}


/* Delete an update log object. */
void tculogdel(TCULOG *ulog){
  assert(ulog);
  if(ulog->base) tculogclose(ulog);
  if(ulog->aiocbs) tcfree(ulog->aiocbs);
  pthread_mutex_destroy(&ulog->wmtx);
  pthread_cond_destroy(&ulog->cnd);
  pthread_rwlock_destroy(&ulog->rwlck);
  for(int i = TCULRMTXNUM - 1; i >= 0; i--){
    pthread_mutex_destroy(ulog->rmtxs + i);
  }
  tcfree(ulog);
}


/* Set AIO control of an update log object. */
bool tculogsetaio(TCULOG *ulog){
  assert(ulog);
  if(ulog->base || ulog->aiocbs) return false;
  struct aiocb *aiocbs = tcmalloc(TCULAIOCBNUM * sizeof(*aiocbs));
  for(int i = 0; i < TCULAIOCBNUM; i++){
    memset(aiocbs + i, 0, sizeof(*aiocbs));
  }
  ulog->aiocbs = aiocbs;
  return true;
}


/* Open files of an update log object. */
bool tculogopen(TCULOG *ulog, const char *base, uint64_t limsiz){
  assert(ulog && base);
  if(ulog->base) return false;
  struct stat sbuf;
  if(stat(base, &sbuf) == -1 || !S_ISDIR(sbuf.st_mode)) return false;
  TCLIST *names = tcreaddir(base);
  if(!names) return false;
  int ln = tclistnum(names);
  int max = 0;
  for(int i = 0; i < ln; i++){
    const char *name = tclistval2(names, i);
    if(!tcstrbwm(name, TCULSUFFIX)) continue;
    int id = atoi(name);
    char *path = tcsprintf("%s/%08d%s", base, id, TCULSUFFIX);
    if(stat(path, &sbuf) == 0 && S_ISREG(sbuf.st_mode) && id > max) max = id;
    tcfree(path);
  }
  tclistdel(names);
  if(max < 1) max = 1;
  ulog->base = tcstrdup(base);
  ulog->limsiz = (limsiz > 0) ? limsiz : INT64_MAX / 2;
  ulog->max = max;
  ulog->fd = -1;
  ulog->size = sbuf.st_size;
  struct aiocb *aiocbs = ulog->aiocbs;
  if(aiocbs){
    for(int i = 0; i < TCULAIOCBNUM; i++){
      struct aiocb *aiocbp = aiocbs + i;
      aiocbp->aio_fildes = 0;
      aiocbp->aio_buf = NULL;
      aiocbp->aio_nbytes = 0;
    }
  }
  ulog->aiocbi = 0;
  ulog->aioend = 0;
  return true;
}


/* Close files of an update log object. */
bool tculogclose(TCULOG *ulog){
  assert(ulog);
  if(!ulog->base) return false;
  bool err = false;
  struct aiocb *aiocbs = ulog->aiocbs;
  if(aiocbs){
    for(int i = 0; i < TCULAIOCBNUM; i++){
      if(!tculogflushaiocbp(aiocbs + i)) err = true;
    }
  }
  if(ulog->fd != -1 && close(ulog->fd) != 0) err = true;
  tcfree(ulog->base);
  ulog->base = NULL;
  return !err;
}


/* Get the mutex index of a record. */
int tculogrmtxidx(TCULOG *ulog, const char *kbuf, int ksiz){
  assert(ulog && kbuf && ksiz >= 0);
  if(!ulog->base || !ulog->aiocbs) return 0;
  uint32_t hash = 19780211;
  while(ksiz--){
    hash = hash * 41 + *(uint8_t *)kbuf++;
  }
  return hash % TCULRMTXNUM;
}


/* Begin the critical section of an update log object. */
bool tculogbegin(TCULOG *ulog, int idx){
  assert(ulog);
  if(!ulog->base) return false;
  if(idx < 0){
    for(int i = 0; i < TCULRMTXNUM; i++){
      if(pthread_mutex_lock(ulog->rmtxs + i) != 0){
        for(i--; i >= 0; i--){
          pthread_mutex_unlock(ulog->rmtxs + i);
        }
        return false;
      }
    }
    return true;
  }
  return pthread_mutex_lock(ulog->rmtxs + idx) == 0;
}


/* End the critical section of an update log object. */
bool tculogend(TCULOG *ulog, int idx){
  assert(ulog);
  if(idx < 0){
    bool err = false;
    for(int i = TCULRMTXNUM - 1; i >= 0; i--){
      if(pthread_mutex_unlock(ulog->rmtxs + i) != 0) err = true;
    }
    return !err;
  }
  return pthread_mutex_unlock(ulog->rmtxs + idx) == 0;
}


/* Write a message into an update log object. */
bool tculogwrite(TCULOG *ulog, uint64_t ts, uint32_t sid, const void *ptr, int size){
  assert(ulog && ptr && size >= 0);
  if(!ulog->base) return false;
  if(ts < 1) ts = (uint64_t)(tctime() * 1000000);
  bool err = false;
  if(pthread_rwlock_wrlock(&ulog->rwlck) != 0) return false;
  pthread_cleanup_push((void (*)(void *))pthread_rwlock_unlock, &ulog->rwlck);
  if(ulog->fd == -1){
    char *path = tcsprintf("%s/%08d%s", ulog->base, ulog->max, TCULSUFFIX);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 00644);
    tcfree(path);
    struct stat sbuf;
    if(fd != -1 && fstat(fd, &sbuf) == 0){
      ulog->fd = fd;
      ulog->size = sbuf.st_size;
    } else {
      err = true;
    }
  }
  int rsiz = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) * 2 + size;
  unsigned char stack[TTIOBUFSIZ];
  unsigned char *buf = (rsiz < TTIOBUFSIZ) ? stack : tcmalloc(rsiz);
  pthread_cleanup_push(free, (buf == stack) ? NULL : buf);
  unsigned char *wp = buf;
  *(wp++) = TCULMAGICNUM;
  uint64_t llnum = TTHTONLL(ts);
  memcpy(wp, &llnum, sizeof(llnum));
  wp += sizeof(llnum);
  uint32_t lnum = TTHTONL(sid);
  memcpy(wp, &lnum, sizeof(lnum));
  wp += sizeof(lnum);
  lnum = TTHTONL(size);
  memcpy(wp, &lnum, sizeof(lnum));
  wp += sizeof(lnum);
  memcpy(wp, ptr, size);
  if(ulog->fd != -1){
    struct aiocb *aiocbs = (struct aiocb *)ulog->aiocbs;
    if(aiocbs){
      struct aiocb *aiocbp = aiocbs + ulog->aiocbi;
      if(aiocbp->aio_buf){
        off_t aioend = aiocbp->aio_offset + aiocbp->aio_nbytes;
        if(tculogflushaiocbp(aiocbp)){
          ulog->aioend = aioend;
        } else {
          err = true;
        }
      }
      aiocbp->aio_fildes = ulog->fd;
      aiocbp->aio_offset = ulog->size;
      aiocbp->aio_buf = tcmemdup(buf, rsiz);
      aiocbp->aio_nbytes = rsiz;
      if(aio_write(aiocbp) != 0){
        tcfree((char *)aiocbp->aio_buf);
        aiocbp->aio_buf = NULL;
        err = true;
      }
      ulog->aiocbi = (ulog->aiocbi + 1) % TCULAIOCBNUM;
    } else {
      if(!tcwrite(ulog->fd, buf, rsiz)) err = true;
    }
    if(!err){
      ulog->size += rsiz;
      if(ulog->size >= ulog->limsiz){
        if(aiocbs){
          for(int i = 0; i < TCULAIOCBNUM; i++){
            if(!tculogflushaiocbp(aiocbs + i)) err = true;
          }
          ulog->aiocbi = 0;
          ulog->aioend = 0;
        }
        char *path = tcsprintf("%s/%08d%s", ulog->base, ulog->max + 1, TCULSUFFIX);
        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 00644);
        tcfree(path);
        if(fd != 0){
          if(close(ulog->fd) != 0) err = true;
          ulog->fd = fd;
          ulog->size = 0;
          ulog->max++;
        } else {
          err = true;
        }
      }
      if(pthread_cond_broadcast(&ulog->cnd) != 0) err = true;
    }
  } else {
    err = true;
  }
  pthread_cleanup_pop(1);
  pthread_cleanup_pop(1);
  return !err;
}


/* Create a log reader object. */
TCULRD *tculrdnew(TCULOG *ulog, uint64_t ts){
  assert(ulog);
  if(!ulog->base) return NULL;
  if(pthread_rwlock_rdlock(&ulog->rwlck) != 0) return NULL;
  TCLIST *names = tcreaddir(ulog->base);
  if(!names){
    pthread_rwlock_unlock(&ulog->rwlck);
    return NULL;
  }
  int ln = tclistnum(names);
  int max = 0;
  for(int i = 0; i < ln; i++){
    const char *name = tclistval2(names, i);
    if(!tcstrbwm(name, TCULSUFFIX)) continue;
    int id = atoi(name);
    char *path = tcsprintf("%s/%08d%s", ulog->base, id, TCULSUFFIX);
    struct stat sbuf;
    if(stat(path, &sbuf) == 0 && S_ISREG(sbuf.st_mode) && id > max) max = id;
    tcfree(path);
  }
  tclistdel(names);
  if(max < 1) max = 1;
  int num = 0;
  for(int i = max; i > 0; i--){
    char *path = tcsprintf("%s/%08d%s", ulog->base, i, TCULSUFFIX);
    int fd = open(path, O_RDONLY, 00644);
    tcfree(path);
    if(fd == -1) break;
    int rsiz = sizeof(uint8_t) + sizeof(uint64_t);
    unsigned char buf[rsiz];
    uint64_t fts = INT64_MAX;
    if(tcread(fd, buf, rsiz)){
      memcpy(&fts, buf + sizeof(uint8_t), sizeof(ts));
      fts = TTNTOHLL(fts);
    }
    close(fd);
    if(ts >= fts){
      num = i;
      break;
    }
  }
  if(num < 1) num = 1;
  TCULRD *urld = tcmalloc(sizeof(*urld));
  urld->ulog = ulog;
  urld->ts = ts;
  urld->num = num;
  urld->fd = -1;
  urld->rbuf = tcmalloc(TTIOBUFSIZ);
  urld->rsiz = TTIOBUFSIZ;
  pthread_rwlock_unlock(&ulog->rwlck);
  return urld;
}


/* Delete a log reader object. */
void tculrddel(TCULRD *ulrd){
  assert(ulrd);
  if(ulrd->fd != -1) close(ulrd->fd);
  tcfree(ulrd->rbuf);
  tcfree(ulrd);
}


/* Wait the next message is written. */
void tculrdwait(TCULRD *ulrd){
  assert(ulrd);
  TCULOG *ulog = ulrd->ulog;
  if(pthread_mutex_lock(&ulog->wmtx) != 0) return;
  pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ulog->wmtx);
  int ocs = PTHREAD_CANCEL_DISABLE;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ocs);
  struct timeval tv;
  struct timespec ts;
  if(gettimeofday(&tv, NULL) == 0){
    ts.tv_sec = tv.tv_sec + 1;
    ts.tv_nsec = tv.tv_usec * 1000;
  } else {
    ts.tv_sec = (1ULL << (sizeof(time_t) * 8 - 1)) - 1;
    ts.tv_nsec = 0;
  }
  pthread_cond_timedwait(&ulog->cnd, &ulog->wmtx, &ts);
  pthread_setcancelstate(ocs, NULL);
  pthread_cleanup_pop(1);
}


/* Read a message from a log reader object. */
const void *tculrdread(TCULRD *ulrd, int *sp, uint64_t *tsp, uint32_t *sidp){
  assert(ulrd && sp && tsp && sidp);
  TCULOG *ulog = ulrd->ulog;
  if(pthread_rwlock_rdlock(&ulog->rwlck) != 0) return NULL;
  if(ulrd->fd == -1){
    char *path = tcsprintf("%s/%08d%s", ulog->base, ulrd->num, TCULSUFFIX);
    ulrd->fd = open(path, O_RDONLY, 00644);
    tcfree(path);
    if(ulrd->fd == -1){
      pthread_rwlock_unlock(&ulog->rwlck);
      return NULL;
    }
  }
  int rsiz = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) * 2;
  unsigned char buf[rsiz];
  uint64_t ts;
  uint32_t sid, size;
  while(true){
    if(ulog->aiocbs && ulrd->num == ulog->max){
      struct stat sbuf;
      if(fstat(ulrd->fd, &sbuf) == -1 ||
         (sbuf.st_size < ulog->size && sbuf.st_size >= ulog->aioend)){
        pthread_rwlock_unlock(&ulog->rwlck);
        return NULL;
      }
    }
    if(!tcread(ulrd->fd, buf, rsiz)){
      if(ulrd->num < ulog->max){
        close(ulrd->fd);
        ulrd->num++;
        char *path = tcsprintf("%s/%08d%s", ulog->base, ulrd->num, TCULSUFFIX);
        ulrd->fd = open(path, O_RDONLY, 00644);
        tcfree(path);
        if(ulrd->fd == -1){
          pthread_rwlock_unlock(&ulog->rwlck);
          return NULL;
        }
        continue;
      }
      pthread_rwlock_unlock(&ulog->rwlck);
      return NULL;
    }
    const unsigned char *rp = buf;
    if(*rp != TCULMAGICNUM){
      pthread_rwlock_unlock(&ulog->rwlck);
      return NULL;
    }
    rp += sizeof(uint8_t);
    memcpy(&ts, rp, sizeof(ts));
    ts = TTNTOHLL(ts);
    rp += sizeof(ts);
    memcpy(&sid, rp, sizeof(sid));
    sid = TTNTOHL(sid);
    rp += sizeof(sid);
    memcpy(&size, rp, sizeof(size));
    size = TTNTOHL(size);
    rp += sizeof(size);
    if(ulrd->rsiz < size + 1){
      ulrd->rbuf = tcrealloc(ulrd->rbuf, size + 1);
      ulrd->rsiz = size + 1;
    }
    if(!tcread(ulrd->fd, ulrd->rbuf, size)){
      pthread_rwlock_unlock(&ulog->rwlck);
      return NULL;
    }
    if(ts < ulrd->ts) continue;
    break;
  }
  *sp = size;
  *tsp = ts;
  *sidp = sid;
  ulrd->rbuf[size] = '\0';
  pthread_rwlock_unlock(&ulog->rwlck);
  return ulrd->rbuf;
}


/* Store a record into an abstract database object. */
bool tculogadbput(TCULOG *ulog, uint32_t sid, TCADB *adb,
                  const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(ulog && adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  int rmidx = tculogrmtxidx(ulog, kbuf, ksiz);
  bool dolog = tculogbegin(ulog, rmidx);
  if(!tcadbput(adb, kbuf, ksiz, vbuf, vsiz)) err = true;
  if(dolog){
    unsigned char mstack[TTIOBUFSIZ];
    int msiz = sizeof(uint8_t) * 3 + sizeof(uint32_t) * 2 + ksiz + vsiz;
    unsigned char *mbuf = (msiz < TTIOBUFSIZ) ? mstack : tcmalloc(msiz + 1);
    unsigned char *wp = mbuf;
    *(wp++) = TTMAGICNUM;
    *(wp++) = TTCMDPUT;
    uint32_t lnum;
    lnum = TTHTONL(ksiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    lnum = TTHTONL(vsiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    memcpy(wp, kbuf, ksiz);
    wp += ksiz;
    memcpy(wp, vbuf, vsiz);
    wp += vsiz;
    *(wp++) = err ? 1 : 0;
    if(!tculogwrite(ulog, 0, sid, mbuf, msiz)) err = true;
    if(mbuf != mstack) tcfree(mbuf);
    tculogend(ulog, rmidx);
  }
  return !err;
}


/* Store a new record into an abstract database object. */
bool tculogadbputkeep(TCULOG *ulog, uint32_t sid, TCADB *adb,
                      const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(ulog && adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  int rmidx = tculogrmtxidx(ulog, kbuf, ksiz);
  bool dolog = tculogbegin(ulog, rmidx);
  if(!tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz)) err = true;
  if(dolog){
    unsigned char mstack[TTIOBUFSIZ];
    int msiz = sizeof(uint8_t) * 3 + sizeof(uint32_t) * 2 + ksiz + vsiz;
    unsigned char *mbuf = (msiz < TTIOBUFSIZ) ? mstack : tcmalloc(msiz + 1);
    unsigned char *wp = mbuf;
    *(wp++) = TTMAGICNUM;
    *(wp++) = TTCMDPUTKEEP;
    uint32_t lnum;
    lnum = TTHTONL(ksiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    lnum = TTHTONL(vsiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    memcpy(wp, kbuf, ksiz);
    wp += ksiz;
    memcpy(wp, vbuf, vsiz);
    wp += vsiz;
    *(wp++) = err ? 1 : 0;
    if(!tculogwrite(ulog, 0, sid, mbuf, msiz)) err = true;
    if(mbuf != mstack) tcfree(mbuf);
    tculogend(ulog, rmidx);
  }
  return !err;
}


/* Concatenate a value at the end of the existing record in an abstract database object. */
bool tculogadbputcat(TCULOG *ulog, uint32_t sid, TCADB *adb,
                     const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(ulog && adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  int rmidx = tculogrmtxidx(ulog, kbuf, ksiz);
  bool dolog = tculogbegin(ulog, rmidx);
  if(!tcadbputcat(adb, kbuf, ksiz, vbuf, vsiz)) err = true;
  if(dolog){
    unsigned char mstack[TTIOBUFSIZ];
    int msiz = sizeof(uint8_t) * 3 + sizeof(uint32_t) * 2 + ksiz + vsiz;
    unsigned char *mbuf = (msiz < TTIOBUFSIZ) ? mstack : tcmalloc(msiz + 1);
    unsigned char *wp = mbuf;
    *(wp++) = TTMAGICNUM;
    *(wp++) = TTCMDPUTCAT;
    uint32_t lnum;
    lnum = TTHTONL(ksiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    lnum = TTHTONL(vsiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    memcpy(wp, kbuf, ksiz);
    wp += ksiz;
    memcpy(wp, vbuf, vsiz);
    wp += vsiz;
    *(wp++) = err ? 1 : 0;
    if(!tculogwrite(ulog, 0, sid, mbuf, msiz)) err = true;
    if(mbuf != mstack) tcfree(mbuf);
    tculogend(ulog, rmidx);
  }
  return !err;
}


/* Remove a record of an abstract database object. */
bool tculogadbout(TCULOG *ulog, uint32_t sid, TCADB *adb, const void *kbuf, int ksiz){
  assert(ulog && adb && kbuf && ksiz >= 0);
  bool err = false;
  int rmidx = tculogrmtxidx(ulog, kbuf, ksiz);
  bool dolog = tculogbegin(ulog, rmidx);
  if(!tcadbout(adb, kbuf, ksiz)) err = true;
  if(dolog){
    unsigned char mstack[TTIOBUFSIZ];
    int msiz = sizeof(uint8_t) * 3 + sizeof(uint32_t) + ksiz;
    unsigned char *mbuf = (msiz < TTIOBUFSIZ) ? mstack : tcmalloc(msiz + 1);
    unsigned char *wp = mbuf;
    *(wp++) = TTMAGICNUM;
    *(wp++) = TTCMDOUT;
    uint32_t lnum;
    lnum = TTHTONL(ksiz);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    memcpy(wp, kbuf, ksiz);
    wp += ksiz;
    *(wp++) = err ? 1 : 0;
    if(!tculogwrite(ulog, 0, sid, mbuf, msiz)) err = true;
    if(mbuf != mstack) tcfree(mbuf);
    tculogend(ulog, rmidx);
  }
  return !err;
}


/* Remove all records of an abstract database object. */
bool tculogadbvanish(TCULOG *ulog, uint32_t sid, TCADB *adb){
  assert(ulog && adb);
  bool err = false;
  bool dolog = tculogbegin(ulog, -1);
  if(!tcadbvanish(adb)) err = true;
  if(dolog){
    unsigned char mbuf[sizeof(uint8_t)*3];
    unsigned char *wp = mbuf;
    *(wp++) = TTMAGICNUM;
    *(wp++) = TTCMDVANISH;
    *(wp++) = err ? 1 : 0;
    if(!tculogwrite(ulog, 0, sid, mbuf, wp - mbuf)) err = true;
    tculogend(ulog, -1);
  }
  return !err;
}


/* Restore an abstract database object. */
bool tculogadbrestore(TCADB *adb, const char *path, uint64_t ts, bool con, TCULOG *ulog){
  assert(adb && path);
  bool err = false;
  TCULOG *sulog = tculognew();
  if(tculogopen(sulog, path, 0)){
    TCULRD *ulrd = tculrdnew(sulog, ts);
    if(ulrd){
      const char *rbuf;
      int rsiz;
      uint64_t rts;
      uint32_t rsid;
      while((rbuf = tculrdread(ulrd, &rsiz, &rts, &rsid)) != NULL){
        if(!tculogadbredo(adb, rbuf, rsiz, con, ulog, rsid)){
          err = true;
          break;
        }
      }
      tculrddel(ulrd);
    } else {
      err = true;
    }
    if(!tculogclose(sulog)) err = true;
  } else {
    err = true;
  }
  tculogdel(sulog);
  return !err;
}


/* Redo an update log message. */
bool tculogadbredo(TCADB *adb, const char *ptr, int size, bool con, TCULOG *ulog, uint32_t sid){
  assert(adb && ptr && size >= 0);
  if(size < sizeof(uint8_t) * 3) return false;
  const unsigned char *rp = (unsigned char *)ptr;
  int magic = *(rp++);
  int cmd = *(rp++);
  bool exp = (((unsigned char *)ptr)[size-1] == 0) ? true : false;
  size -= sizeof(uint8_t) * 3;
  if(magic != TTMAGICNUM) return false;
  bool err = false;
  switch(cmd){
  case TTCMDPUT:
    if(size >= sizeof(uint32_t) * 2){
      uint32_t ksiz;
      memcpy(&ksiz, rp, sizeof(ksiz));
      ksiz = TTNTOHL(ksiz);
      rp += sizeof(ksiz);
      uint32_t vsiz;
      memcpy(&vsiz, rp, sizeof(vsiz));
      vsiz = TTNTOHL(vsiz);
      rp += sizeof(vsiz);
      if(tculogadbput(ulog, sid, adb, rp, ksiz, rp + ksiz, vsiz) != exp && con) err = true;
    } else {
      err = true;
    }
    break;
  case TTCMDPUTKEEP:
    if(size >= sizeof(uint32_t) * 2){
      uint32_t ksiz;
      memcpy(&ksiz, rp, sizeof(ksiz));
      ksiz = TTNTOHL(ksiz);
      rp += sizeof(ksiz);
      uint32_t vsiz;
      memcpy(&vsiz, rp, sizeof(vsiz));
      vsiz = TTNTOHL(vsiz);
      rp += sizeof(vsiz);
      if(tculogadbputkeep(ulog, sid, adb, rp, ksiz, rp + ksiz, vsiz) != exp && con) err = true;
    } else {
      err = true;
    }
    break;
  case TTCMDPUTCAT:
    if(size >= sizeof(uint32_t) * 2){
      uint32_t ksiz;
      memcpy(&ksiz, rp, sizeof(ksiz));
      ksiz = TTNTOHL(ksiz);
      rp += sizeof(ksiz);
      uint32_t vsiz;
      memcpy(&vsiz, rp, sizeof(vsiz));
      vsiz = TTNTOHL(vsiz);
      rp += sizeof(vsiz);
      if(tculogadbputcat(ulog, sid, adb, rp, ksiz, rp + ksiz, vsiz) != exp && con) err = true;
    } else {
      err = true;
    }
    break;
  case TTCMDOUT:
    if(size >= sizeof(uint32_t)){
      uint32_t ksiz;
      memcpy(&ksiz, rp, sizeof(ksiz));
      ksiz = TTNTOHL(ksiz);
      rp += sizeof(ksiz);
      if(tculogadbout(ulog, sid, adb, rp, ksiz) != exp && con) err = true;
    } else {
      err = true;
    }
    break;
  case TTCMDVANISH:
    if(size == 0){
      if(tculogadbvanish(ulog, sid, adb) != exp && con) err = true;
    } else {
      err = true;
    }
    break;
  default:
    err = true;
    break;
  }
  return !err;
}


/* Create a replicatoin object. */
TCREPL *tcreplnew(void){
  TCREPL *repl = tcmalloc(sizeof(*repl));
  repl->fd = -1;
  repl->sock = NULL;
  return repl;
}


/* Delete a replication object. */
void tcrepldel(TCREPL *repl){
  assert(repl);
  if(repl->fd >= 0) tcreplclose(repl);
  tcfree(repl);
}


/* Open a replication object. */
bool tcreplopen(TCREPL *repl, const char *host, int port, uint64_t ts, uint32_t sid){
  assert(repl && host && port >= 0);
  if(repl->fd >= 0) return false;
  char addr[TTADDRBUFSIZ];
  if(!ttgethostaddr(host, addr)) return false;
  int fd = ttopensock(addr, port);
  if(fd == -1) return false;
  unsigned char buf[TTIOBUFSIZ];
  unsigned char *wp = buf;
  *(wp++) = TTMAGICNUM;
  *(wp++) = TTCMDREPL;
  uint64_t llnum = TTHTONLL(ts);
  memcpy(wp, &llnum, sizeof(llnum));
  wp += sizeof(llnum);
  uint64_t lnum = TTHTONL(sid);
  memcpy(wp, &lnum, sizeof(lnum));
  wp += sizeof(lnum);
  repl->fd = fd;
  repl->sock = ttsocknew(fd);
  repl->rbuf = tcmalloc(TTIOBUFSIZ);
  repl->rsiz = TTIOBUFSIZ;
  if(!ttsocksend(repl->sock, buf, wp - buf)){
    tcreplclose(repl);
    return false;
  }
  return true;
}


/* Close a remote database object. */
bool tcreplclose(TCREPL *repl){
  assert(repl);
  if(repl->fd < 0) return false;
  bool err = false;
  tcfree(repl->rbuf);
  ttsockdel(repl->sock);
  if(!ttclosesock(repl->fd)) err = true;
  repl->fd = -1;
  repl->sock = NULL;
  return !err;
}


/* Read a message from a replication object. */
const char *tcreplread(TCREPL *repl, int *sp, uint64_t *tsp, uint32_t *sidp){
  assert(repl && sp && tsp);
  int ocs = PTHREAD_CANCEL_DISABLE;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ocs);
  int c = ttsockgetc(repl->sock);
  if(c == TCULMAGICNOP){
    *sp = 0;
    *tsp = 0;
    *sidp = 0;
    return "";
  }
  if(c != TCULMAGICNUM){
    pthread_setcancelstate(ocs, NULL);
    return NULL;
  }
  uint64_t ts = ttsockgetint64(repl->sock);
  uint32_t sid = ttsockgetint32(repl->sock);
  uint32_t rsiz = ttsockgetint32(repl->sock);
  if(repl->rsiz < rsiz + 1){
    repl->rbuf = tcrealloc(repl->rbuf, rsiz + 1);
    repl->rsiz = rsiz + 1;
  }
  if(ttsockcheckend(repl->sock) || !ttsockrecv(repl->sock, repl->rbuf, rsiz) ||
     ttsockcheckend(repl->sock)){
    pthread_setcancelstate(ocs, NULL);
    return NULL;
  }
  *sp = rsiz;
  *tsp = ts;
  *sidp = sid;
  pthread_setcancelstate(ocs, NULL);
  return repl->rbuf;
}


/* Flush a AIO task.
   `aiocbp' specifies the pointer to the AIO task object.
   If successful, the return value is true, else, it is false. */
static bool tculogflushaiocbp(struct aiocb *aiocbp){
  assert(aiocbp);
  if(!aiocbp->aio_buf) return true;
  bool err = false;
  while(true){
    int rv = aio_error(aiocbp);
    if(rv == 0) break;
    if(rv != EINPROGRESS){
      err = true;
      break;
    }
    if(aio_suspend((void *)&aiocbp, 1, NULL) == -1) err = true;
  }
  tcfree((char *)aiocbp->aio_buf);
  aiocbp->aio_buf = NULL;
  if(aio_return(aiocbp) != aiocbp->aio_nbytes) err = true;
  return !err;
}



// END OF FILE
