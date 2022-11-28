/*************************************************************************************************
 * System-dependent configurations of Tokyo Tyrant
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


#ifndef _MYCONF_H                        // duplication check
#define _MYCONF_H



/*************************************************************************************************
 * system discrimination
 *************************************************************************************************/


#if defined(__linux__)

#define _SYS_LINUX_
#define ESTSYSNAME  "Linux"

#elif defined(__FreeBSD__)

#define _SYS_FREEBSD_
#define ESTSYSNAME  "FreeBSD"

#elif defined(__NetBSD__)

#define _SYS_NETBSD_
#define ESTSYSNAME  "NetBSD"

#elif defined(__OpenBSD__)

#define _SYS_OPENBSD_
#define ESTSYSNAME  "OpenBSD"

#elif defined(__sun__)

#define _SYS_SUNOS_
#define ESTSYSNAME  "SunOS"

#elif defined(__hpux)

#define _SYS_HPUX_
#define ESTSYSNAME  "HP-UX"

#elif defined(__osf)

#define _SYS_TRU64_
#define ESTSYSNAME  "Tru64"

#elif defined(_AIX)

#define _SYS_AIX_
#define ESTSYSNAME  "AIX"

#elif defined(__APPLE__) && defined(__MACH__)

#define _SYS_MACOSX_
#define ESTSYSNAME  "Mac OS X"

#elif defined(_MSC_VER)

#define _SYS_MSVC_
#define ESTSYSNAME  "Windows (VC++)"

#elif defined(_WIN32)

#define _SYS_MINGW_
#define ESTSYSNAME  "Windows (MinGW)"

#elif defined(__CYGWIN__)

#define _SYS_CYGWIN_
#define ESTSYSNAME  "Windows (Cygwin)"

#else

#define _SYS_GENERIC_
#define ESTSYSNAME  "Generic"

#endif

#if !defined(_SYS_LINUX_) && !defined(_SYS_FREEBSD_) && !defined(_SYS_MACOSX_)
#error =======================================
#error Your platform is not supported.  Sorry.
#error =======================================
#endif



/*************************************************************************************************
 * common settings
 *************************************************************************************************/


#if defined(NDEBUG)
#define TTDODEBUG(TT_expr) \
  do { \
  } while(false)
#else
#define TTDODEBUG(TT_expr) \
  do { \
    TT_expr; \
  } while(false)
#endif

#define TTSWAB16(TT_num) \
  ( \
   ((TT_num & 0x00ffU) << 8) | \
   ((TT_num & 0xff00U) >> 8) \
  )

#define TTSWAB32(TT_num) \
  ( \
   ((TT_num & 0x000000ffUL) << 24) | \
   ((TT_num & 0x0000ff00UL) << 8) | \
   ((TT_num & 0x00ff0000UL) >> 8) | \
   ((TT_num & 0xff000000UL) >> 24) \
  )

#define TTSWAB64(TT_num) \
  ( \
   ((TT_num & 0x00000000000000ffULL) << 56) | \
   ((TT_num & 0x000000000000ff00ULL) << 40) | \
   ((TT_num & 0x0000000000ff0000ULL) << 24) | \
   ((TT_num & 0x00000000ff000000ULL) << 8) | \
   ((TT_num & 0x000000ff00000000ULL) >> 8) | \
   ((TT_num & 0x0000ff0000000000ULL) >> 24) | \
   ((TT_num & 0x00ff000000000000ULL) >> 40) | \
   ((TT_num & 0xff00000000000000ULL) >> 56) \
  )

#if defined(_MYBIGEND) || defined(_MYSWAB)
#define TTBIGEND       1
#define TTHTONS(TT_num)   (TT_num)
#define TTHTONL(TT_num)   (TT_num)
#define TTHTONLL(TT_num)  (TT_num)
#define TTNTOHS(TT_num)   (TT_num)
#define TTNTOHL(TT_num)   (TT_num)
#define TTNTOHLL(TT_num)  (TT_num)
#else
#define TTBIGEND       0
#define TTHTONS(TT_num)   TTSWAB16(TT_num)
#define TTHTONL(TT_num)   TTSWAB32(TT_num)
#define TTHTONLL(TT_num)  TTSWAB64(TT_num)
#define TTNTOHS(TT_num)   TTSWAB16(TT_num)
#define TTNTOHL(TT_num)   TTSWAB32(TT_num)
#define TTNTOHLL(TT_num)  TTSWAB64(TT_num)
#endif



/*************************************************************************************************
 * general headers
 *************************************************************************************************/


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dirent.h>
#include <aio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>
#include <tcbdb.h>
#include <tcadb.h>

#if defined(_SYS_FREEBSD_) || defined(_SYS_MACOSX_)
#define TTUSEKQUEUE    1
#else
#include <sys/epoll.h>
#define TTUSEKQUEUE    0
#endif



/*************************************************************************************************
 * miscellaneous hacks
 *************************************************************************************************/


#define sizeof(a)      ((int)sizeof(a))

int _tt_dummyfunc(void);
int _tt_dummyfuncv(int a, ...);



/*************************************************************************************************
 * notation of filesystems
 *************************************************************************************************/


#define MYPATHCHR       '/'
#define MYPATHSTR       "/"
#define MYEXTCHR        '.'
#define MYEXTSTR        "."
#define MYCDIRSTR       "."
#define MYPDIRSTR       ".."



/*************************************************************************************************
 * epoll emulation
 *************************************************************************************************/


#if TTUSEKQUEUE

struct epoll_event {
  uint32_t events;
  union {
    void *ptr;
    int fd;
  } data;
};

enum {
  EPOLLIN = 1 << 0,
  EPOLLOUT = 1 << 1,
  EPOLLONESHOT = 1 << 8
};

enum {
  EPOLL_CTL_ADD,
  EPOLL_CTL_MOD,
  EPOLL_CTL_DEL
};

int _tt_epoll_create(int size);
int _tt_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int _tt_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#define epoll_create   _tt_epoll_create
#define epoll_ctl      _tt_epoll_ctl
#define epoll_wait     _tt_epoll_wait

#endif



/*************************************************************************************************
 * utilities for implementation
 *************************************************************************************************/


#define TTNUMBUFSIZ    32                // size of a buffer for a number

/* set a buffer for a variable length number */
#define TTSETVNUMBUF(TT_len, TT_buf, TT_num) \
  do { \
    int _TT_num = (TT_num); \
    if(_TT_num == 0){ \
      ((signed char *)(TT_buf))[0] = 0; \
      (TT_len) = 1; \
    } else { \
      (TT_len) = 0; \
      while(_TT_num > 0){ \
        int _TT_rem = _TT_num & 0x7f; \
        _TT_num >>= 7; \
        if(_TT_num > 0){ \
          ((signed char *)(TT_buf))[(TT_len)] = -_TT_rem - 1; \
        } else { \
          ((signed char *)(TT_buf))[(TT_len)] = _TT_rem; \
        } \
        (TT_len)++; \
      } \
    } \
  } while(false)

/* set a buffer for a variable length number of 64-bit */
#define TTSETVNUMBUF64(TT_len, TT_buf, TT_num) \
  do { \
    long long int _TT_num = (TT_num); \
    if(_TT_num == 0){ \
      ((signed char *)(TT_buf))[0] = 0; \
      (TT_len) = 1; \
    } else { \
      (TT_len) = 0; \
      while(_TT_num > 0){ \
        int _TT_rem = _TT_num & 0x7f; \
        _TT_num >>= 7; \
        if(_TT_num > 0){ \
          ((signed char *)(TT_buf))[(TT_len)] = -_TT_rem - 1; \
        } else { \
          ((signed char *)(TT_buf))[(TT_len)] = _TT_rem; \
        } \
        (TT_len)++; \
      } \
    } \
  } while(false)

/* read a variable length buffer */
#define TTREADVNUMBUF(TT_buf, TT_num, TT_step) \
  do { \
    TT_num = 0; \
    int _TT_base = 1; \
    int _TT_i = 0; \
    while(true){ \
      if(((signed char *)(TT_buf))[_TT_i] >= 0){ \
        TT_num += ((signed char *)(TT_buf))[_TT_i] * _TT_base; \
        break; \
      } \
      TT_num += _TT_base * (((signed char *)(TT_buf))[_TT_i] + 1) * -1; \
      _TT_base <<= 7; \
      _TT_i++; \
    } \
    (TT_step) = _TT_i + 1; \
  } while(false)

/* read a variable length buffer */
#define TTREADVNUMBUF64(TT_buf, TT_num, TT_step) \
  do { \
    TT_num = 0; \
    long long int _TT_base = 1; \
    int _TT_i = 0; \
    while(true){ \
      if(((signed char *)(TT_buf))[_TT_i] >= 0){ \
        TT_num += ((signed char *)(TT_buf))[_TT_i] * _TT_base; \
        break; \
      } \
      TT_num += _TT_base * (((signed char *)(TT_buf))[_TT_i] + 1) * -1; \
      _TT_base <<= 7; \
      _TT_i++; \
    } \
    (TT_step) = _TT_i + 1; \
  } while(false)



#endif                                   // duplication check


// END OF FILE
