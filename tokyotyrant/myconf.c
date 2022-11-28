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


#include "myconf.h"



/*************************************************************************************************
 * common settings
 *************************************************************************************************/


int _tt_dummyfunc(void){
  return 0;
}


int _tt_dummyfuncv(int a, ...){
  return 0;
}



/*************************************************************************************************
 * epoll emulation
 *************************************************************************************************/


#if TTUSEKQUEUE


#include <sys/event.h>


int _tt_epoll_create(int size){
  return kqueue();
}


int _tt_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event){
  if(op == EPOLL_CTL_ADD){
    struct kevent kev;
    int kfilt = 0;
    if(event->events & EPOLLIN) kfilt |= EVFILT_READ;
    if(event->events & EPOLLOUT) kfilt |= EVFILT_WRITE;
    int kflags = EV_ADD;
    if(event->events & EPOLLONESHOT) kflags |= EV_ONESHOT;
    EV_SET(&kev, fd, kfilt, kflags, 0, 0, NULL);
    return kevent(epfd, &kev, 1, NULL, 0, NULL) != -1 ? 0 : -1;
  } else if(op == EPOLL_CTL_MOD){
    struct kevent kev;
    int kfilt = 0;
    if(event->events & EPOLLIN) kfilt |= EVFILT_READ;
    if(event->events & EPOLLOUT) kfilt |= EVFILT_WRITE;
    int kflags = EV_ADD;
    if(event->events & EPOLLONESHOT) kflags |= EV_ONESHOT;
    EV_SET(&kev, fd, kfilt, kflags, 0, 0, NULL);
    return kevent(epfd, &kev, 1, NULL, 0, NULL) != -1 ? 0 : -1;
  } else if(op == EPOLL_CTL_DEL){
    struct kevent kev;
    int kfilt = EVFILT_READ | EVFILT_WRITE;
    EV_SET(&kev, fd, kfilt, EV_DELETE, 0, 0, NULL);
    return kevent(epfd, &kev, 1, NULL, 0, NULL) != -1 || errno == ENOENT ? 0 : -1;
  }
  return -1;
}


int _tt_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout){
  div_t td = div(timeout, 1000);
  struct timespec ts;
  ts.tv_sec = td.quot;
  ts.tv_nsec = td.rem * 1000000;
  struct kevent kevs[maxevents];
  int num = kevent(epfd, NULL, 0, kevs, maxevents, &ts);
  if(num == -1) return -1;
  for(int i = 0; i < num; i++){
    events[i].data.fd = kevs[i].ident;
  }
  return num;
}


#endif



// END OF FILE
