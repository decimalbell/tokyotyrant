/*************************************************************************************************
 * The utility API of Tokyo Tyrant
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
#include "myconf.h"


/*************************************************************************************************
 * basic utilities
 *************************************************************************************************/


#define SOCKPATHBUFSIZ 108               // size of a socket path buffer
#define HTTPLINEBUFSIZ 2048              // size of a line buffer of HTTP
#define HTTPBODYMAXSIZ (256*1024*1024)   // maximum size of the entity body of HTTP
#define TRILLIONNUM    1000000000000     // trillion number


/* String containing the version information. */
const char *ttversion = _TT_VERSION;


/* Get the primary name of the local host. */
bool ttgetlocalhostname(char *name){
  assert(name);
  if(gethostname(name, TTADDRBUFSIZ - 1) != 0){
    sprintf(name, "localhost");
    return false;
  }
  return true;
}


/* Get the address of a host. */
bool ttgethostaddr(const char *name, char *addr){
  assert(name && addr);
  struct addrinfo hints, *result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  if(getaddrinfo(name, NULL, &hints, &result) != 0){
    *addr = '\0';
    return false;
  }
  if(!result){
    freeaddrinfo(result);
    return false;
  }
  if(result->ai_addr->sa_family != AF_INET){
    freeaddrinfo(result);
    return false;
  }
  struct sockaddr_in *sain = (struct sockaddr_in *)result->ai_addr;
  uint32_t anum = sain->sin_addr.s_addr;
  char *wp = addr;
  for(int i = 0; i < sizeof(anum); i++){
    if(i > 0) *(wp++) = '.';
    wp += sprintf(wp, "%d", anum & 0xff);
    anum = anum >> 8;
  }
  freeaddrinfo(result);
  return true;
}


/* Open a client socket of TCP/IP stream to a server. */
int ttopensock(const char *addr, int port){
  assert(addr && port >= 0);
  struct sockaddr_in sain;
  memset(&sain, 0, sizeof(sain));
  sain.sin_family = AF_INET;
  if(inet_aton(addr, &sain.sin_addr) == 0) return -1;
  uint16_t snum = port;
  sain.sin_port = htons(snum);
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if(fd == -1) return -1;
  if(connect(fd, (struct sockaddr *)&sain, sizeof(sain)) != 0){
    close(fd);
    return -1;
  }
  return fd;
}


/* Open a client socket of UNIX domain stream to a server. */
int ttopensockunix(const char *path){
  assert(path);
  struct sockaddr_un saun;
  memset(&saun, 0, sizeof(saun));
  saun.sun_family = AF_UNIX;
  snprintf(saun.sun_path, SOCKPATHBUFSIZ, "%s", path);
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) return -1;
  if(connect(fd, (struct sockaddr *)&saun, sizeof(saun)) != 0){
    close(fd);
    return -1;
  }
  return fd;
}


/* Open a server socket of TCP/IP stream to clients. */
int ttopenservsock(const char *addr, int port){
  assert(port >= 0);
  struct sockaddr_in sain;
  memset(&sain, 0, sizeof(sain));
  sain.sin_family = AF_INET;
  if(inet_aton(addr ? addr : "0.0.0.0", &sain.sin_addr) == 0) return -1;
  uint16_t snum = port;
  sain.sin_port = htons(snum);
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if(fd == -1) return -1;
  int optone = 1;
  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optone, sizeof(optone)) != 0){
    close(fd);
    return -1;
  }
  if(bind(fd, (struct sockaddr *)&sain, sizeof(sain)) != 0 ||
     listen(fd, SOMAXCONN) != 0){
    close(fd);
    return -1;
  }
  return fd;
}


/* Open a server socket of UNIX domain stream to clients. */
int ttopenservsockunix(const char *path){
  assert(path);
  if(*path == '\0') return -1;
  struct sockaddr_un saun;
  memset(&saun, 0, sizeof(saun));
  saun.sun_family = AF_UNIX;
  snprintf(saun.sun_path, SOCKPATHBUFSIZ, "%s", path);
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) return -1;
  if(bind(fd, (struct sockaddr *)&saun, sizeof(saun)) != 0 ||
     listen(fd, SOMAXCONN) != 0){
    close(fd);
    return -1;
  }
  return fd;
}


/* Accept a TCP/IP connection from a client. */
int ttacceptsock(int fd, char *addr, int *pp){
  assert(fd >= 0);
  do {
    struct sockaddr_in sain;
    memset(&sain, 0, sizeof(sain));
    sain.sin_family = AF_INET;
    socklen_t slen = sizeof(sain);
    int cfd = accept(fd, (struct sockaddr *)&sain, &slen);
    if(cfd >= 0){
      int optone = 1;
      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&optone, sizeof(optone));
      if(addr){
        uint32_t anum = sain.sin_addr.s_addr;
        char *wp = addr;
        for(int i = 0; i < sizeof(anum); i++){
          if(i > 0) *(wp++) = '.';
          wp += sprintf(wp, "%d", anum & 0xff);
          anum = anum >> 8;
        }
      }
      if(pp) *pp = (int)ntohs(sain.sin_port);
      return cfd;
    }
  } while(errno == EINTR || errno == EAGAIN);
  return -1;
}


/* Accept a UNIX domain connection from a client. */
int ttacceptsockunix(int fd){
  assert(fd >= 0);
  do {
    int cfd = accept(fd, NULL, NULL);
    if(cfd >= 0) return cfd;
  } while(errno == EINTR || errno == EAGAIN);
  return -1;
}


/* Shutdown and close a socket. */
bool ttclosesock(int fd){
  assert(fd >= 0);
  bool err = false;
  if(shutdown(fd, 2) != 0 && errno != ENOTCONN && errno != ECONNRESET) err = true;
  if(close(fd) != 0 && errno != ENOTCONN && errno != ECONNRESET) err = true;
  return !err;
}


/* Create a socket object. */
TTSOCK *ttsocknew(int fd){
  assert(fd >= 0);
  TTSOCK *sock = tcmalloc(sizeof(*sock));
  sock->fd = fd;
  sock->rp = sock->buf;
  sock->ep = sock->buf;
  sock->end = false;
  return sock;
}


/* Delete a socket object. */
void ttsockdel(TTSOCK *sock){
  assert(sock);
  tcfree(sock);
}


/* Send data by a socket. */
bool ttsocksend(TTSOCK *sock, const void *buf, int size){
  assert(sock && buf && size >= 0);
  const char *rp = buf;
  do {
    int ocs = PTHREAD_CANCEL_DISABLE;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ocs);
    int wb = send(sock->fd, rp, size, 0);
    pthread_setcancelstate(ocs, NULL);
    switch(wb){
    case -1: if(errno != EINTR) return false;
    case 0: break;
    default:
      rp += wb;
      size -= wb;
      break;
    }
  } while(size > 0);
  return true;
}


/* Send formatted data by a socket. */
bool ttsockprintf(TTSOCK *sock, const char *format, ...){
  assert(sock && format);
  bool err = false;
  TCXSTR *xstr = tcxstrnew();
  pthread_cleanup_push((void (*)(void *))tcxstrdel, xstr);
  va_list ap;
  va_start(ap, format);
  while(*format != '\0'){
    if(*format == '%'){
      char cbuf[TTNUMBUFSIZ];
      cbuf[0] = '%';
      int cblen = 1;
      int lnum = 0;
      format++;
      while(strchr("0123456789 .+-hlLz", *format) && *format != '\0' &&
            cblen < TTNUMBUFSIZ - 1){
        if(*format == 'l' || *format == 'L') lnum++;
        cbuf[cblen++] = *(format++);
      }
      cbuf[cblen++] = *format;
      cbuf[cblen] = '\0';
      int tlen;
      char *tmp, tbuf[TTNUMBUFSIZ*2];
      switch(*format){
      case 's':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        tcxstrcat2(xstr, tmp);
        break;
      case 'd':
        if(lnum >= 2){
          tlen = sprintf(tbuf, cbuf, va_arg(ap, long long));
        } else if(lnum >= 1){
          tlen = sprintf(tbuf, cbuf, va_arg(ap, long));
        } else {
          tlen = sprintf(tbuf, cbuf, va_arg(ap, int));
        }
        tcxstrcat(xstr, tbuf, tlen);
        break;
      case 'o': case 'u': case 'x': case 'X': case 'c':
        if(lnum >= 2){
          tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned long long));
        } else if(lnum >= 1){
          tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned long));
        } else {
          tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned int));
        }
        tcxstrcat(xstr, tbuf, tlen);
        break;
      case 'e': case 'E': case 'f': case 'g': case 'G':
        if(lnum >= 1){
          tlen = sprintf(tbuf, cbuf, va_arg(ap, long double));
        } else {
          tlen = sprintf(tbuf, cbuf, va_arg(ap, double));
        }
        tcxstrcat(xstr, tbuf, tlen);
        break;
      case '@':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        while(*tmp){
          switch(*tmp){
          case '&': tcxstrcat(xstr, "&amp;", 5); break;
          case '<': tcxstrcat(xstr, "&lt;", 4); break;
          case '>': tcxstrcat(xstr, "&gt;", 4); break;
          case '"': tcxstrcat(xstr, "&quot;", 6); break;
          default:
            if(!((*tmp >= 0 && *tmp <= 0x8) || (*tmp >= 0x0e && *tmp <= 0x1f)))
              tcxstrcat(xstr, tmp, 1);
            break;
          }
          tmp++;
        }
        break;
      case '?':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        while(*tmp){
          unsigned char c = *(unsigned char *)tmp;
          if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || (c != '\0' && strchr("_-.", c))){
            tcxstrcat(xstr, tmp, 1);
          } else {
            tlen = sprintf(tbuf, "%%%02X", c);
            tcxstrcat(xstr, tbuf, tlen);
          }
          tmp++;
        }
        break;
      case '%':
        tcxstrcat(xstr, "%", 1);
        break;
      }
    } else {
      tcxstrcat(xstr, format, 1);
    }
    format++;
  }
  va_end(ap);
  if(!ttsocksend(sock, tcxstrptr(xstr), tcxstrsize(xstr))) err = true;
  pthread_cleanup_pop(1);
  return !err;
}


/* Receive data by a socket. */
bool ttsockrecv(TTSOCK *sock, char *buf, int size){
  assert(sock && buf && size >= 0);
  bool err = false;
  char *wp = buf;
  while(size > 0){
    int c = ttsockgetc(sock);
    if(c == -1){
      err = true;
      break;
    }
    *(wp++) = c;
    size--;
  }
  return !err;
}


/* Receive one byte by a socket. */
int ttsockgetc(TTSOCK *sock){
  assert(sock);
  if(sock->rp < sock->ep) return *(unsigned char *)(sock->rp++);
  do {
    int ocs = PTHREAD_CANCEL_DISABLE;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ocs);
    int rv = recv(sock->fd, sock->buf, TTIOBUFSIZ, 0);
    pthread_setcancelstate(ocs, NULL);
    if(rv > 0){
      sock->rp = sock->buf + 1;
      sock->ep = sock->buf + rv;
      return *(unsigned char *)sock->buf;
    } else if(rv == 0){
      sock->end = true;
      return -1;
    }
  } while(errno == EINTR);
  sock->end = true;
  return -1;
}


/* Push a character back to a socket. */
void ttsockungetc(TTSOCK *sock, int c){
  assert(sock);
  if(sock->rp <= sock->buf) return;
  sock->rp--;
  *(unsigned char *)sock->rp = c;
}


/* Receive one line by a socket. */
bool ttsockgets(TTSOCK *sock, char *buf, int size){
  assert(sock && buf && size > 0);
  bool err = false;
  size--;
  char *wp = buf;
  while(size > 0){
    int c = ttsockgetc(sock);
    if(c == '\n') break;
    if(c == -1){
      err = true;
      break;
    }
    if(c != '\r'){
      *(wp++) = c;
      size--;
    }
  }
  *wp = '\0';
  return !err;
}


/* Receive an 32-bit integer by a socket. */
uint32_t ttsockgetint32(TTSOCK *sock){
  assert(sock);
  uint32_t num = ttsockgetc(sock) << 24;
  num |= ttsockgetc(sock) << 16;
  num |= ttsockgetc(sock) << 8;
  num |= ttsockgetc(sock);
  return num;
}


/* Receive an 64-bit integer by a socket. */
uint64_t ttsockgetint64(TTSOCK *sock){
  assert(sock);
  uint64_t num = (uint64_t)ttsockgetc(sock) << 56;
  num |= (uint64_t)ttsockgetc(sock) << 48;
  num |= (uint64_t)ttsockgetc(sock) << 40;
  num |= (uint64_t)ttsockgetc(sock) << 32;
  num |= (uint64_t)ttsockgetc(sock) << 24;
  num |= (uint64_t)ttsockgetc(sock) << 16;
  num |= (uint64_t)ttsockgetc(sock) << 8;
  num |= (uint64_t)ttsockgetc(sock);
  return num;
}


/* Check whehter a socket is end. */
bool ttsockcheckend(TTSOCK *sock){
  assert(sock);
  return sock->end;
}


/* Check the size of prefetched data in a socket. */
int ttsockcheckpfsiz(TTSOCK *sock){
  assert(sock);
  if(sock->end) return 0;
  return sock->ep - sock->rp;
}


/* Fetch the resource of a URL by HTTP. */
int tthttpfetch(const char *url, TCMAP *reqheads, TCMAP *resheads, TCXSTR *resbody){
  assert(url);
  int code = -1;
  TCMAP *elems = tcurlbreak(url);
  pthread_cleanup_push((void (*)(void *))tcmapdel, elems);
  const char *scheme = tcmapget2(elems, "scheme");
  const char *host = tcmapget2(elems, "host");
  const char *port = tcmapget2(elems, "port");
  const char *authority = tcmapget2(elems, "authority");
  const char *path = tcmapget2(elems, "path");
  const char *query = tcmapget2(elems, "query");
  if(scheme && !tcstricmp(scheme, "http") && host){
    if(*host == '\0') host = "127.0.0.1";
    int pnum = port ? atoi(port) : 80;
    if(pnum < 1) pnum = 80;
    if(!path) path = "/";
    char addr[TTADDRBUFSIZ];
    int fd;
    if(ttgethostaddr(host, addr) && (fd = ttopensock(addr, pnum)) != -1){
      pthread_cleanup_push((void (*)(void *))ttclosesock, (void *)(intptr_t)fd);
      TTSOCK *sock = ttsocknew(fd);
      pthread_cleanup_push((void (*)(void *))ttsockdel, sock);
      TCXSTR *obuf = tcxstrnew();
      pthread_cleanup_push((void (*)(void *))tcxstrdel, obuf);
      if(query){
        tcxstrprintf(obuf, "GET %s?%s HTTP/1.1\r\n", path, query);
      } else {
        tcxstrprintf(obuf, "GET %s HTTP/1.1\r\n", path);
      }
      if(pnum == 80){
        tcxstrprintf(obuf, "Host: %s\r\n", host);
      } else {
        tcxstrprintf(obuf, "Host: %s:%d\r\n", host, pnum);
      }
      tcxstrprintf(obuf, "Connection: close\r\n", host, port);
      if(authority){
        char *enc = tcbaseencode(authority, strlen(authority));
        tcxstrprintf(obuf, "Authorization: Basic %s\r\n", enc);
        tcfree(enc);
      }
      if(reqheads){
        tcmapiterinit(reqheads);
        const char *name;
        while((name = tcmapiternext2(reqheads)) != NULL){
          if(strchr(name, ':') || !tcstricmp(name, "connection")) continue;
          char *cap = tcstrdup(name);
          tcstrtolower(cap);
          char *wp = cap;
          bool head = true;
          while(*wp != '\0'){
            if(head && *wp >= 'a' && *wp <= 'z') *wp -= 'a' - 'A';
            head = *wp == '-' || *wp == ' ';
            wp++;
          }
          tcxstrprintf(obuf, "%s: %s\r\n", cap, tcmapget2(reqheads, name));
          tcfree(cap);
        }
      }
      tcxstrprintf(obuf, "\r\n", host);
      if(ttsocksend(sock, tcxstrptr(obuf), tcxstrsize(obuf))){
        char line[HTTPLINEBUFSIZ];
        if(ttsockgets(sock, line, HTTPLINEBUFSIZ) && tcstrfwm(line, "HTTP/")){
          tcstrsqzspc(line);
          const char *rp = strchr(line, ' ');
          code = atoi(rp + 1);
          if(resheads) tcmapput2(resheads, "STATUS", line);
        }
        if(code > 0){
          int clen = 0;
          bool chunked = false;
          while(ttsockgets(sock, line, HTTPLINEBUFSIZ) && *line != '\0'){
            tcstrsqzspc(line);
            char *pv = strchr(line, ':');
            if(!pv) continue;
            *(pv++) = '\0';
            while(*pv == ' '){
              pv++;
            }
            tcstrtolower(line);
            if(!strcmp(line, "content-length")){
              clen = atoi(pv);
            } else if(!strcmp(line, "transfer-encoding")){
              if(!tcstricmp(pv, "chunked")) chunked = true;
            }
            if(resheads) tcmapput2(resheads, line, pv);
          }
          if(!ttsockcheckend(sock) && resbody){
            bool err = false;
            char *body;
            int bsiz;
            if(code == 304){
              body = tcmemdup("", 0);
              bsiz = 0;
            } else if(chunked){
              int asiz = HTTPLINEBUFSIZ;
              body = tcmalloc(asiz);
              bsiz = 0;
              while(true){
                pthread_cleanup_push(free, body);
                if(!ttsockgets(sock, line, HTTPLINEBUFSIZ)) err = true;
                pthread_cleanup_pop(0);
                if(err || *line == '\0') break;
                int size = strtol(line, NULL, 16);
                if(bsiz + size > HTTPBODYMAXSIZ){
                  err = true;
                  break;
                }
                if(bsiz + size > asiz){
                  asiz = bsiz * 2 + size;
                  body = tcrealloc(body, asiz);
                }
                pthread_cleanup_push(free, body);
                if(size > 0) ttsockrecv(sock, body + bsiz, size);
                if(ttsockgetc(sock) != '\r' || ttsockgetc(sock) != '\n') err = true;
                pthread_cleanup_pop(0);
                if(err || size < 1) break;
                bsiz += size;
              }
            } else if(clen > 0){
              if(clen > HTTPBODYMAXSIZ){
                body = tcmemdup("", 0);
                bsiz = 0;
                err = true;
              } else {
                body = tcmalloc(clen);
                bsiz = 0;
                pthread_cleanup_push(free, body);
                if(ttsockrecv(sock, body, clen)){
                  bsiz = clen;
                } else {
                  err = true;
                }
                pthread_cleanup_pop(0);
              }
            } else {
              int asiz = HTTPLINEBUFSIZ;
              body = tcmalloc(asiz);
              bsiz = 0;
              while(true){
                int c;
                pthread_cleanup_push(free, body);
                c = ttsockgetc(sock);
                pthread_cleanup_pop(0);
                if(c == -1) break;
                if(bsiz >= HTTPBODYMAXSIZ){
                  err = true;
                  break;
                }
                if(bsiz >= asiz){
                  asiz = bsiz * 2;
                  body = tcrealloc(body, asiz);
                }
                body[bsiz++] = c;
              }
            }
            if(err){
              code = -1;
            } else if(resbody){
              tcxstrcat(resbody, body, bsiz);
            }
            tcfree(body);
          }
        }
      }
      pthread_cleanup_pop(1);
      pthread_cleanup_pop(1);
      pthread_cleanup_pop(1);
    }
  }
  pthread_cleanup_pop(1);
  return code;
}


/* Serialize a real number. */
void ttpackdouble(double num, char *buf){
  assert(buf);
  double dinteg, dfract;
  dfract = modf(num, &dinteg);
  int64_t linteg, lfract;
  if(isnormal(dinteg)){
    linteg = dinteg;
    lfract = dfract * TRILLIONNUM;
  } else if(isinf(dinteg)){
    linteg = dinteg > 0 ? INT64_MAX : INT64_MIN;
    lfract = 0;
  } else {
    linteg = INT64_MIN;
    lfract = INT64_MIN;
  }
  memcpy(buf, &linteg, sizeof(linteg));
  memcpy(buf + sizeof(linteg), &lfract, sizeof(lfract));
}


/* Redintegrate a serialized real number. */
double ttunpackdouble(const char *buf){
  assert(buf);
  int64_t linteg, lfract;
  memcpy(&linteg, buf, sizeof(linteg));
  memcpy(&lfract, buf + sizeof(linteg), sizeof(lfract));
  if(lfract == INT64_MIN && linteg == INT64_MIN){
    return NAN;
  } else if(linteg == INT64_MAX){
    return INFINITY;
  } else if(linteg == INT64_MIN){
    return -INFINITY;
  }
  return linteg + (double)lfract / TRILLIONNUM;
}



/*************************************************************************************************
 * server utilities
 *************************************************************************************************/


#define TTADDRBUFSIZ   1024              // size of an address buffer
#define TTDEFTHNUM     5                 // default number of threads
#define TTEVENTMAX     256               // maximum number of events
#define TTWAITREQUEST  200               // waiting milliseconds for requests
#define TTWAITWORKER   100               // waiting milliseconds for finish of workers


/* private function prototypes */
static void *ttservtimer(void *argp);
static void ttservtask(TTSOCK *sock, TTREQ *req);
static void *ttservdeqtasks(void *argp);


/* Create a server object. */
TTSERV *ttservnew(void){
  TTSERV *serv = tcmalloc(sizeof(*serv));
  serv->host[0] = '\0';
  serv->addr[0] = '\0';
  serv->port = 0;
  serv->queue = tclistnew();
  if(pthread_mutex_init(&serv->qmtx, NULL) != 0) tcmyfatal("pthread_mutex_init failed");
  if(pthread_cond_init(&serv->qcnd, NULL) != 0) tcmyfatal("pthread_cond_init failed");
  serv->thnum = TTDEFTHNUM;
  serv->timeout = 0;
  serv->term = false;
  serv->do_log = NULL;
  serv->opq_log = NULL;
  serv->freq_timed = 0.0;
  serv->do_timed = NULL;
  serv->opq_timed = NULL;
  serv->do_task = NULL;
  serv->opq_task = NULL;
  return serv;
}


/* Delete a server object. */
void ttservdel(TTSERV *serv){
  assert(serv);
  pthread_mutex_destroy(&serv->qmtx);
  pthread_cond_destroy(&serv->qcnd);
  tclistdel(serv->queue);
  tcfree(serv);
}


/* Configure a server object. */
bool ttservconf(TTSERV *serv, const char *host, int port){
  assert(serv);
  bool err = false;
  if(port < 1){
    if(!host || host[0] == '\0'){
      err = true;
      serv->addr[0] = '\0';
      ttservlog(serv, TTLOGERROR, "invalid socket path");
    }
  } else {
    if(host && !ttgethostaddr(host, serv->addr)){
      err = true;
      serv->addr[0] = '\0';
      ttservlog(serv, TTLOGERROR, "ttgethostaddr failed");
    }
  }
  snprintf(serv->host, sizeof(serv->host), "%s", host ? host : "");
  serv->port = port;
  return !err;
}


/* Set tuning parameters of a server object. */
void ttservtune(TTSERV *serv, int thnum, double timeout){
  assert(serv && thnum > 0);
  serv->thnum = thnum;
  serv->timeout = timeout;
}


/* Set the logging handler of a server object. */
void ttservsetloghandler(TTSERV *serv, void (*do_log)(int, const char *, void *), void *opq){
  assert(serv && do_log);
  serv->do_log = do_log;
  serv->opq_log = opq;
}


/* Set the timed handler of a server object. */
void ttservsettimedhandler(TTSERV *serv, double freq, void (*do_timed)(void *), void *opq){
  assert(serv && freq >= 0.0 && do_timed);
  serv->freq_timed = freq;
  serv->do_timed = do_timed;
  serv->opq_timed = opq;
}


/* Set the response handler of a server object. */
void ttservsettaskhandler(TTSERV *serv, void (*do_task)(TTSOCK *, void *, TTREQ *), void *opq){
  assert(serv && do_task);
  serv->do_task = do_task;
  serv->opq_task = opq;
}


/* Start the service of a server object. */
bool ttservstart(TTSERV *serv){
  assert(serv);
  int lfd;
  if(serv->port < 1){
    lfd = ttopenservsockunix(serv->host);
    if(lfd == -1){
      ttservlog(serv, TTLOGERROR, "ttopenservsockunix failed");
      return false;
    }
  } else {
    lfd = ttopenservsock(serv->addr[0] != '\0' ? serv->addr : NULL, serv->port);
    if(lfd == -1){
      ttservlog(serv, TTLOGERROR, "ttopenservsock failed");
      return false;
    }
  }
  int epfd = epoll_create(TTEVENTMAX);
  if(epfd == -1){
    close(lfd);
    ttservlog(serv, TTLOGERROR, "epoll_create failed");
    return false;
  }
  ttservlog(serv, TTLOGSYSTEM, "service started: %d", getpid());
  bool err = false;
  TTTIMER timer;
  timer.alive = false;
  timer.serv = serv;
  if(serv->do_timed){
    if(pthread_create(&timer.thid, NULL, ttservtimer, &timer) == 0){
      ttservlog(serv, TTLOGINFO, "timer thread started");
      timer.alive = true;
    } else {
      ttservlog(serv, TTLOGERROR, "pthread_create failed");
      err = true;
    }
  }
  int thnum = serv->thnum;
  TTREQ reqs[thnum];
  for(int i = 0; i < thnum; i++){
    reqs[i].alive = true;
    reqs[i].serv = serv;
    reqs[i].epfd = epfd;
    reqs[i].mtime = tctime();
    reqs[i].keep = false;
    if(pthread_create(&reqs[i].thid, NULL, ttservdeqtasks, reqs + i) == 0){
      ttservlog(serv, TTLOGINFO, "worker thread %d started", i + 1);
    } else {
      reqs[i].alive = false;
      err = true;
      ttservlog(serv, TTLOGERROR, "pthread_create failed");
    }
  }
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = lfd;
  if(epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "epoll_ctl failed");
  }
  ttservlog(serv, TTLOGSYSTEM, "listening started");
  while(!serv->term){
    struct epoll_event events[TTEVENTMAX];
    int fdnum = epoll_wait(epfd, events, TTEVENTMAX, TTWAITREQUEST);
    if(fdnum != -1){
      for(int i = 0; i < fdnum; i++){
        if(events[i].data.fd == lfd){
          char addr[TTADDRBUFSIZ];
          int port;
          int cfd;
          if(serv->port < 1){
            cfd = ttacceptsockunix(lfd);
            sprintf(addr, "(unix)");
            port = 0;
          } else {
            cfd = ttacceptsock(lfd, addr, &port);
          }
          if(cfd != -1){
            ttservlog(serv, TTLOGINFO, "connected: %s:%d", addr, port);
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN | EPOLLONESHOT;
            ev.data.fd = cfd;
            if(epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) != 0){
              close(cfd);
              err = true;
              ttservlog(serv, TTLOGERROR, "epoll_ctl failed");
            }
          } else {
            err = true;
            ttservlog(serv, TTLOGERROR, "ttacceptsock failed");
          }
        } else {
          int cfd = events[i].data.fd;
          if(pthread_mutex_lock(&serv->qmtx) == 0){
            tclistpush(serv->queue, &cfd, sizeof(cfd));
            if(pthread_mutex_unlock(&serv->qmtx) != 0){
              err = true;
              ttservlog(serv, TTLOGERROR, "pthread_mutex_unlock failed");
            }
            if(pthread_cond_signal(&serv->qcnd) != 0){
              err = true;
              ttservlog(serv, TTLOGERROR, "pthread_cond_signal failed");
            }
          } else {
            err = true;
            ttservlog(serv, TTLOGERROR, "pthread_mutex_lock failed");
          }
        }
      }
    } else {
      if(errno == EINTR){
        ttservlog(serv, TTLOGINFO, "signal interruption");
      } else {
        err = true;
        ttservlog(serv, TTLOGERROR, "epoll_wait failed");
      }
    }
    if(serv->timeout > 0){
      double ctime = tctime();
      for(int i = 0; i < thnum; i++){
        double itime = ctime - reqs[i].mtime;
        if(itime > serv->timeout && pthread_cancel(reqs[i].thid) == 0){
          ttservlog(serv, TTLOGINFO, "worker thread %d canceled by timeout", i + 1);
          void *rv;
          if(pthread_join(reqs[i].thid, &rv) == 0){
            if(rv && rv != PTHREAD_CANCELED) err = true;
            reqs[i].mtime = tctime();
            if(pthread_create(&reqs[i].thid, NULL, ttservdeqtasks, reqs + i) != 0){
              reqs[i].alive = false;
              err = true;
              ttservlog(serv, TTLOGERROR, "pthread_create failed");
            } else {
              ttservlog(serv, TTLOGINFO, "worker thread %d started", i + 1);
            }
          } else {
            reqs[i].alive = false;
            err = true;
            ttservlog(serv, TTLOGERROR, "pthread_join failed");
          }
        }
      }
    }
  }
  ttservlog(serv, TTLOGSYSTEM, "listening finished");
  if(pthread_cond_broadcast(&serv->qcnd) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "pthread_cond_broadcast failed");
  }
  usleep(TTWAITWORKER * 1000);
  for(int i = 0; i < thnum; i++){
    if(!reqs[i].alive) continue;
    if(pthread_cancel(reqs[i].thid) == 0)
      ttservlog(serv, TTLOGINFO, "worker thread %d was canceled", i + 1);
    void *rv;
    if(pthread_join(reqs[i].thid, &rv) == 0){
      ttservlog(serv, TTLOGINFO, "worker thread %d finished", i + 1);
      if(rv && rv != PTHREAD_CANCELED) err = true;
    } else {
      err = true;
      ttservlog(serv, TTLOGERROR, "pthread_join failed");
    }
  }
  if(tclistnum(serv->queue) > 0)
    ttservlog(serv, TTLOGINFO, "%d requests discarded", tclistnum(serv->queue));
  tclistclear(serv->queue);
  if(timer.alive){
    void *rv;
    if(pthread_cancel(timer.thid) == 0)
      ttservlog(serv, TTLOGINFO, "timer thread was canceled");
    if(pthread_join(timer.thid, &rv) == 0){
      ttservlog(serv, TTLOGINFO, "timer thread finished");
      if(rv && rv != PTHREAD_CANCELED) err = true;
    } else {
      err = true;
      ttservlog(serv, TTLOGERROR, "pthread_join failed");
    }
  }
  if(close(epfd) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "close failed");
  }
  if(serv->port < 1 && unlink(serv->host) == -1){
    err = true;
    ttservlog(serv, TTLOGERROR, "unlink failed");
  }
  if(close(lfd) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "close failed");
  }
  ttservlog(serv, TTLOGSYSTEM, "service finished");
  serv->term = false;
  return !err;
}


/* Send the terminate signal to a server object. */
bool ttservkill(TTSERV *serv){
  assert(serv);
  serv->term = true;
  return true;
}


/* Call the logging function of a server object. */
void ttservlog(TTSERV *serv, int level, const char *format, ...){
  assert(serv && format);
  if(!serv->do_log) return;
  char buf[TTIOBUFSIZ];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, TTIOBUFSIZ, format, ap);
  va_end(ap);
  serv->do_log(level, buf, serv->opq_log);
}


/* Check whether a server object is killed. */
bool ttserviskilled(TTSERV *serv){
  assert(serv);
  return serv->term;
}


/* Call the timed function of a server object.
   `argp' specifies the argument structure of the server object.
   The return value is `NULL' on success and other on failure. */
static void *ttservtimer(void *argp){
  TTTIMER *timer = argp;
  TTSERV *serv = timer->serv;
  bool err = false;
  if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "pthread_setcancelstate failed");
  }
  usleep(100000);
  while(!serv->term){
    serv->do_timed(serv->opq_timed);
    usleep(serv->freq_timed * 1000000);
  }
  return err ? "error" : NULL;
}


/* Call the task function of a server object.
   `req' specifies the request object.
   `sock' specifies the socket object. */
static void ttservtask(TTSOCK *sock, TTREQ *req){
  TTSERV *serv = req->serv;
  if(!serv->do_task) return;
  serv->do_task(sock, serv->opq_task, req);
}


/* Dequeue tasks of a server object and dispatch them.
   `argp' specifies the argument structure of the server object.
   The return value is `NULL' on success and other on failure. */
static void *ttservdeqtasks(void *argp){
  TTREQ *req = argp;
  TTSERV *serv = req->serv;
  bool err = false;
  if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "pthread_setcancelstate failed");
  }
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGPIPE);
  sigset_t oldsigset;
  sigemptyset(&sigset);
  if(pthread_sigmask(SIG_BLOCK, &sigset, &oldsigset) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "pthread_sigmask failed");
  }
  bool empty = false;
  while(!serv->term){
    if(pthread_mutex_lock(&serv->qmtx) == 0){
      struct timeval tv;
      struct timespec ts;
      if(gettimeofday(&tv, NULL) == 0){
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000 + TTWAITREQUEST * 1000000;
        if(ts.tv_nsec >= 1000000000){
          ts.tv_nsec -= 1000000000;
          ts.tv_sec++;
        }
      } else {
        ts.tv_sec = (1ULL << (sizeof(time_t) * 8 - 1)) - 1;
        ts.tv_nsec = 0;
      }
      int code = empty ? pthread_cond_timedwait(&serv->qcnd, &serv->qmtx, &ts) : 0;
      if(code == 0 || code == ETIMEDOUT || code == EINTR){
        void *val = tclistshift2(serv->queue);
        if(pthread_mutex_unlock(&serv->qmtx) != 0){
          err = true;
          ttservlog(serv, TTLOGERROR, "pthread_mutex_unlock failed");
        }
        if(val){
          empty = false;
          int cfd = *(int *)val;
          tcfree(val);
          pthread_cleanup_push((void (*)(void *))close, (void *)(intptr_t)cfd);
          TTSOCK *sock = ttsocknew(cfd);
          pthread_cleanup_push((void (*)(void *))ttsockdel, sock);
          bool reuse;
          do {
            req->mtime = tctime();
            req->keep = false;
            ttservtask(sock, req);
            reuse = false;
            if(sock->end){
              req->keep = false;
            } else if(sock->ep > sock->rp){
              reuse = true;
            }
          } while(reuse);
          pthread_cleanup_pop(1);
          pthread_cleanup_pop(0);
          if(req->keep){
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN | EPOLLONESHOT;
            ev.data.fd = cfd;
            if(epoll_ctl(req->epfd, EPOLL_CTL_MOD, cfd, &ev) != 0){
              close(cfd);
              err = true;
              ttservlog(serv, TTLOGERROR, "epoll_ctl failed");
            }
          } else {
            if(epoll_ctl(req->epfd, EPOLL_CTL_DEL, cfd, NULL) != 0){
              err = true;
              ttservlog(serv, TTLOGERROR, "epoll_ctl failed");
            }
            if(!ttclosesock(cfd)){
              err = true;
              ttservlog(serv, TTLOGERROR, "close failed");
            }
            ttservlog(serv, TTLOGINFO, "connection finished");
          }
        } else {
          empty = true;
        }
      } else {
        pthread_mutex_unlock(&serv->qmtx);
        err = true;
        ttservlog(serv, TTLOGERROR, "pthread_cond_timedwait failed");
      }
    } else {
      err = true;
      ttservlog(serv, TTLOGERROR, "pthread_mutex_lock failed");
    }
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    req->mtime = tctime();
  }
  if(pthread_sigmask(SIG_SETMASK, &oldsigset, NULL) != 0){
    err = true;
    ttservlog(serv, TTLOGERROR, "pthread_sigmask failed");
  }
  return err ? "error" : NULL;
}



// END OF FILE
