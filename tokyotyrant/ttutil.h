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


#ifndef _TTUTIL_H                        /* duplication check */
#define _TTUTIL_H

#if defined(__cplusplus)
#define __TTUTIL_CLINKAGEBEGIN extern "C" {
#define __TTUTIL_CLINKAGEEND }
#else
#define __TTUTIL_CLINKAGEBEGIN
#define __TTUTIL_CLINKAGEEND
#endif
__TTUTIL_CLINKAGEBEGIN


#include <tcutil.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>



/*************************************************************************************************
 * basic utilities
 *************************************************************************************************/


#define TTIOBUFSIZ     65536             /* size of an I/O buffer */
#define TTADDRBUFSIZ   1024              /* size of an address buffer */

typedef struct {                         /* type of structure for a socket */
  int fd;                                /* file descriptor */
  char buf[TTIOBUFSIZ];                  /* reading buffer */
  char *rp;                              /* reading pointer */
  char *ep;                              /* end pointer */
  bool end;                              /* end flag */
} TTSOCK;


/* String containing the version information. */
extern const char *ttversion;


/* Get the primary name of the local host.
   `name' specifies the pointer to the region into which the host name is written.  The size of
   the buffer should be equal to or more than `TTADDRBUFSIZ' bytes.
   If successful, the return value is true, else, it is false. */
bool ttgetlocalhostname(char *name);


/* Get the address of a host.
   `name' specifies the name of the host.
   `addr' specifies the pointer to the region into which the address is written.  The size of the
   buffer should be equal to or more than `TTADDRBUFSIZ' bytes.
   If successful, the return value is true, else, it is false. */
bool ttgethostaddr(const char *name, char *addr);


/* Open a client socket of TCP/IP stream to a server.
   `addr' specifies the address of the server.
   `port' specifies the port number of the server.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttopensock(const char *addr, int port);


/* Open a client socket of UNIX domain stream to a server.
   `path' specifies the path of the socket file.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttopensockunix(const char *path);


/* Open a server socket of TCP/IP stream to clients.
   `addr' specifies the address of the server.  If it is `NULL', every network address is binded.
   `port' specifies the port number of the server.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttopenservsock(const char *addr, int port);


/* Open a server socket of UNIX domain stream to clients.
   `addr' specifies the address of the server.  If it is `NULL', every network address is binded.
   `port' specifies the port number of the server.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttopenservsockunix(const char *path);


/* Accept a TCP/IP connection from a client.
   `fd' specifies the file descriptor.
   `addr' specifies the pointer to the region into which the client address is written.  The size
   of the buffer should be equal to or more than `TTADDRBUFSIZ' bytes.
   `pp' specifies the pointer to a variable to which the client port is assigned.  If it is
   `NULL', it is not used.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttacceptsock(int fd, char *addr, int *pp);


/* Accept a UNIX domain connection from a client.
   `fd' specifies the file descriptor.
   The return value is the file descriptor of the stream, or -1 on error. */
int ttacceptsockunix(int fd);


/* Shutdown and close a socket.
   `fd' specifies the file descriptor.
   If successful, the return value is true, else, it is false. */
bool ttclosesock(int fd);


/* Create a socket object.
   `fd' specifies the file descriptor.
   The return value is the socket object. */
TTSOCK *ttsocknew(int fd);


/* Delete a socket object.
   `sock' specifies the socket object. */
void ttsockdel(TTSOCK *sock);


/* Send data by a socket.
   `sock' specifies the socket object.
   `buf' specifies the pointer to the region of the data to send.
   `size' specifies the size of the buffer.
   If successful, the return value is true, else, it is false. */
bool ttsocksend(TTSOCK *sock, const void *buf, int size);


/* Send formatted data by a socket.
   `sock' specifies the socket object.
   `format' specifies the printf-like format string.
   The conversion character `%' can be used with such flag characters as `s', `d', `o', `u',
   `x', `X', `c', `e', `E', `f', `g', `G', `@', `?', `%'.  `@' works as with `s' but escapes meta
   characters of XML.  `?' works as with `s' but escapes meta characters of URL.  The other
   conversion character work as with each original.
   The other arguments are used according to the format string.
   If successful, the return value is true, else, it is false. */
bool ttsockprintf(TTSOCK *sock, const char *format, ...);


/* Receive data by a socket.
   `sock' specifies the socket object.
   `buf' specifies the pointer to the region of the data to be received.
   `size' specifies the size of the buffer.
   If successful, the return value is true, else, it is false.   False is returned if the socket
   is closed before receiving the specified size of data. */
bool ttsockrecv(TTSOCK *sock, char *buf, int size);


/* Receive one byte by a socket.
   `sock' specifies the socket object.
   The return value is the received byte.  If some error occurs or the socket is closed by the
   server, -1 is returned. */
int ttsockgetc(TTSOCK *sock);


/* Push a character back to a socket.
   `sock' specifies the socket object.
   `c' specifies the character. */
void ttsockungetc(TTSOCK *sock, int c);


/* Receive one line by a socket.
   `sock' specifies the socket object.
   `buf' specifies the pointer to the region of the data to be received.
   `size' specifies the size of the buffer.
   If successful, the return value is true, else, it is false.   False is returned if the socket
   is closed before receiving linefeed. */
bool ttsockgets(TTSOCK *sock, char *buf, int size);


/* Receive an 32-bit integer by a socket.
   `sock' specifies the socket object.
   The return value is the 32-bit integer. */
uint32_t ttsockgetint32(TTSOCK *sock);


/* Receive an 64-bit integer by a socket.
   `sock' specifies the socket object.
   The return value is the 64-bit integer. */
uint64_t ttsockgetint64(TTSOCK *sock);


/* Check whehter a socket is end.
   `sock' specifies the socket object.
   The return value is true if the socket is end, else, it is false. */
bool ttsockcheckend(TTSOCK *sock);


/* Check the size of prefetched data in a socket.
   `sock' specifies the socket object.
   The return value is the size of the prefetched data. */
int ttsockcheckpfsiz(TTSOCK *sock);


/* Fetch the resource of a URL by HTTP.
   `url' specifies the URL.
   `reqheads' specifies a map object contains request header names and their values.  If it is
   NULL, it is not used.
   `resheads' specifies a map object to store response headers their values.  If it is NULL, it
   is not used.  Each key of the map is an uncapitalized header name.  The key "STATUS" means the
   status line.
   `resbody' specifies a extensible string object to store the entity body of the result.  If it
   is NULL, it is not used.
   The return value is the response code or -1 on network error. */
int tthttpfetch(const char *url, TCMAP *reqheads, TCMAP *resheads, TCXSTR *resbody);


/* Serialize a real number.
   `num' specifies a real number.
   `buf' specifies the pointer to the region into which the result is written.  The size of the
   buffer should be 16 bytes. */
void ttpackdouble(double num, char *buf);


/* Redintegrate a serialized real number.
   `buf' specifies the pointer to the region of the serialized real number.  The size of the
   buffer should be 16 bytes.
   The return value is the original real number. */
double ttunpackdouble(const char *buf);



/*************************************************************************************************
 * server utilities
 *************************************************************************************************/


#define TTMAGICNUM     0xc8              /* magic number of each command */
#define TTCMDPUT       0x10              /* ID of put command */
#define TTCMDPUTKEEP   0x11              /* ID of putkeep command */
#define TTCMDPUTCAT    0x12              /* ID of putcat command */
#define TTCMDPUTRTT    0x13              /* ID of putrtt command */
#define TTCMDPUTNR     0x18              /* ID of putnr command */
#define TTCMDOUT       0x20              /* ID of out command */
#define TTCMDGET       0x30              /* ID of get command */
#define TTCMDMGET      0x31              /* ID of mget command */
#define TTCMDVSIZ      0x38              /* ID of vsiz command */
#define TTCMDITERINIT  0x50              /* ID of iterinit command */
#define TTCMDITERNEXT  0x51              /* ID of iternext command */
#define TTCMDFWMKEYS   0x58              /* ID of fwmkeys command */
#define TTCMDADDINT    0x60              /* ID of addint command */
#define TTCMDADDDOUBLE 0x61              /* ID of adddouble command */
#define TTCMDSYNC      0x70              /* ID of sync command */
#define TTCMDVANISH    0x71              /* ID of vanish command */
#define TTCMDCOPY      0x72              /* ID of copy command */
#define TTCMDRESTORE   0x73              /* ID of restore command */
#define TTCMDSETMST    0x78              /* ID of setmst command */
#define TTCMDRNUM      0x80              /* ID of rnum command */
#define TTCMDSIZE      0x81              /* ID of size command */
#define TTCMDSTAT      0x88              /* ID of stat command */
#define TTCMDREPL      0xa0              /* ID of repl command */

typedef struct _TTTIMER {                /* type of structure for a timer */
  pthread_t thid;                        /* thread ID */
  bool alive;                            /* alive flag */
  struct _TTSERV *serv;                  /* server object */
} TTTIMER;

typedef struct _TTREQ {                  /* type of structure for a server */
  pthread_t thid;                        /* thread ID */
  bool alive;                            /* alive flag */
  struct _TTSERV *serv;                  /* server object */
  int epfd;                              /* polling file descriptor */
  double mtime;                          /* last modified time */
  bool keep;                             /* keep-alive flag */
} TTREQ;

typedef struct _TTSERV {                 /* type of structure for a server */
  char host[TTADDRBUFSIZ];               /* host name */
  char addr[TTADDRBUFSIZ];               /* host address */
  uint16_t port;                         /* port number */
  TCLIST *queue;                         /* queue of requests */
  pthread_mutex_t qmtx;                  /* mutex for the queue */
  pthread_cond_t qcnd;                   /* condition variable for the queue */
  int thnum;                             /* number of threads */
  double timeout;                        /* timeout milliseconds of each task */
  bool term;                             /* terminate flag */
  void (*do_log)(int, const char *, void *);  /* call back function for logging */
  void *opq_log;                         /* opaque pointer for logging */
  double freq_timed;                     /* frequency of timed handler */
  void (*do_timed)(void *);              /* call back function for timed handler */
  void *opq_timed;                       /* opaque pointer for timed handler */
  void (*do_task)(TTSOCK *, void *, TTREQ *req);  /* call back function for task */
  void *opq_task;                        /* opaque pointer for task */
} TTSERV;

enum {                                   /* enumeration for logging levels */
  TTLOGDEBUG,                            /* debug */
  TTLOGINFO,                             /* information */
  TTLOGERROR,                            /* error */
  TTLOGSYSTEM                            /* system */
};


/* Create a server object.
   The return value is the server object. */
TTSERV *ttservnew(void);


/* Delete a server object.
   `serv' specifies the server object. */
void ttservdel(TTSERV *serv);


/* Configure a server object.
   `serv' specifies the server object.
   `host' specifies the name or the address.  If it is `NULL', If it is `NULL', every network
   address is binded.
   `port' specifies the port number.  If it is not less than 0, UNIX domain socket is binded and
   the host name is treated as the path of the socket file.
   If successful, the return value is true, else, it is false. */
bool ttservconf(TTSERV *serv, const char *host, int port);


/* Set tuning parameters of a server object.
   `serv' specifies the server object.
   `thnum' specifies the number of worker threads.  By default, the number is 5.
   `timeout' specifies the timeout seconds of each task.  If it is not more than 0, no timeout is
   specified.  By default, there is no timeout. */
void ttservtune(TTSERV *serv, int thnum, double timeout);


/* Set the logging handler of a server object.
   `serv' specifies the server object.
   `do_log' specifies the pointer to a function to do with a log message.  Its first parameter is
   the log level, one of `TTLOGDEBUG', `TTLOGINFO', `TTLOGERROR'.  Its second parameter is the
   message string.  Its third parameter is the opaque pointer.
   `opq' specifies the opaque pointer to be passed to the handler.  It can be `NULL'. */
void ttservsetloghandler(TTSERV *serv, void (*do_log)(int, const char *, void *), void *opq);


/* Set the timed handler of a server object.
   `serv' specifies the server object.
   `freq' specifies the frequency of execution in seconds.
   `do_timed' specifies the pointer to a function to do with a event.  Its parameter is the
   opaque pointer.
   `opq' specifies the opaque pointer to be passed to the handler.  It can be `NULL'. */
void ttservsettimedhandler(TTSERV *serv, double freq, void (*do_timed)(void *), void *opq);


/* Set the response handler of a server object.
   `serv' specifies the server object.
   `do_task' specifies the pointer to a function to do with a task.  Its first parameter is
   the socket object connected to the client.  Its second parameter is the opaque pointer.  Its
   third parameter is the request object.
   `opq' specifies the opaque pointer to be passed to the handler.  It can be `NULL'. */
void ttservsettaskhandler(TTSERV *serv, void (*do_task)(TTSOCK *, void *, TTREQ *), void *opq);


/* Start the service of a server object.
   `serv' specifies the server object.
   If successful, the return value is true, else, it is false. */
bool ttservstart(TTSERV *serv);


/* Send the terminate signal to a server object.
   `serv' specifies the server object.
   If successful, the return value is true, else, it is false. */
bool ttservkill(TTSERV *serv);


/* Call the logging function of a server object.
   `serv' specifies the server object.
   `level' specifies the logging level.
   `format' specifies the message format.
   The other arguments are used according to the format string. */
void ttservlog(TTSERV *serv, int level, const char *format, ...);


/* Check whether a server object is killed.
   `serv' specifies the server object.
   The return value is true if the server is killed, or false if not. */
bool ttserviskilled(TTSERV *serv);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


#define _TT_VERSION    "1.0.1"
#define _TT_LIBVER     117
#define _TT_PROTVER    "0.9"



__TTUTIL_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
