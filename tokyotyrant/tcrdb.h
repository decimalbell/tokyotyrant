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


#ifndef _TCRDB_H                         /* duplication check */
#define _TCRDB_H

#if defined(__cplusplus)
#define __TCRDB_CLINKAGEBEGIN extern "C" {
#define __TCRDB_CLINKAGEEND }
#else
#define __TCRDB_CLINKAGEBEGIN
#define __TCRDB_CLINKAGEEND
#endif
__TCRDB_CLINKAGEBEGIN


#include <tcutil.h>
#include <ttutil.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>



/*************************************************************************************************
 * API
 *************************************************************************************************/


typedef struct {                         /* type of structure for a remote database */
  int fd;                                /* file descriptor */
  TTSOCK *sock;                          /* socket object */
  int ecode;                             /* last happened error code */
} TCRDB;

enum {                                   /* enumeration for error codes */
  TTESUCCESS,                            /* success */
  TTEINVALID,                            /* invalid operation */
  TTENOHOST,                             /* host not found */
  TTEREFUSED,                            /* connection refused */
  TTESEND,                               /* send error */
  TTERECV,                               /* recv error */
  TTEKEEP,                               /* existing record */
  TTENOREC,                              /* no record found */
  TTEMISC = 9999                         /* miscellaneous error */
};


/* Get the message string corresponding to an error code.
   `ecode' specifies the error code.
   The return value is the message string of the error code. */
const char *tcrdberrmsg(int ecode);


/* Create a remote database object.
   The return value is the new remote database object. */
TCRDB *tcrdbnew(void);


/* Delete a remote database object.
   `rdb' specifies the remote database object. */
void tcrdbdel(TCRDB *rdb);


/* Get the last happened error code of a remote database object.
   `rdb' specifies the remote database object.
   The return value is the last happened error code.
   The following error code is defined: `TTESUCCESS' for success, `TTEINVALID' for invalid
   operation, `TTENOHOST' for host not found, `TTEREFUSED' for connection refused, `TTESEND' for
   send error, `TTERECV' for recv error, `TTEKEEP' for existing record, `TTENOREC' for no record
   found, `TTEMISC' for miscellaneous error. */
int tcrdbecode(TCRDB *rdb);


/* Open a remote database.
   `rdb' specifies the remote database object.
   `host' specifies the name or the address of the server.
   `port' specifies the port number.
   If successful, the return value is true, else, it is false. */
bool tcrdbopen(TCRDB *rdb, const char *host, int port);


/* Close a remote database object.
   `rdb' specifies the remote database object.
   If successful, the return value is true, else, it is false. */
bool tcrdbclose(TCRDB *rdb);


/* Store a record into a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, it is overwritten. */
bool tcrdbput(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a string record into a remote object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, it is overwritten. */
bool tcrdbput2(TCRDB *rdb, const char *kstr, const char *vstr);


/* Store a new record into a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, this function has no effect. */
bool tcrdbputkeep(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a new string record into a remote database object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, this function has no effect. */
bool tcrdbputkeep2(TCRDB *rdb, const char *kstr, const char *vstr);


/* Concatenate a value at the end of the existing record in a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If there is no corresponding record, a new record is created. */
bool tcrdbputcat(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Concatenate a string value at the end of the existing record in a remote database object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If there is no corresponding record, a new record is created. */
bool tcrdbputcat2(TCRDB *rdb, const char *kstr, const char *vstr);


/* Concatenate a value at the end of the existing record and rotate it to the left.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   `width' specifies the width of the record.
   If successful, the return value is true, else, it is false.
   If there is no corresponding record, a new record is created. */
bool tcrdbputrtt(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz, int width);


/* Concatenate a string value at the end of the existing record and rotate it to the left.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   `width' specifies the width of the record.
   If successful, the return value is true, else, it is false.
   If there is no corresponding record, a new record is created. */
bool tcrdbputrtt2(TCRDB *rdb, const char *kstr, const char *vstr, int width);


/* Store a record into a remote database object without response from the server.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, it is overwritten. */
bool tcrdbputnr(TCRDB *rdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a string record into a remote object without response from the server.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, it is overwritten. */
bool tcrdbputnr2(TCRDB *rdb, const char *kstr, const char *vstr);


/* Remove a record of a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is true, else, it is false. */
bool tcrdbout(TCRDB *rdb, const void *kbuf, int ksiz);


/* Remove a string record of a remote database object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   If successful, the return value is true, else, it is false. */
bool tcrdbout2(TCRDB *rdb, const char *kstr);


/* Retrieve a record in a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the corresponding
   record.  `NULL' is returned if no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
void *tcrdbget(TCRDB *rdb, const void *kbuf, int ksiz, int *sp);


/* Retrieve a string record in a remote database object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   If successful, the return value is the string of the value of the corresponding record.
   `NULL' is returned if no record corresponds.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcrdbget2(TCRDB *rdb, const char *kstr);


/* Retrieve records in a remote database object.
   `rdb' specifies the remote database object.
   `recs' specifies a map object containing the retrieval keys.  As a result of this function,
   keys existing in the database have the corresponding values and keys not existing in the
   database are removed.
   If successful, the return value is true, else, it is false. */
bool tcrdbget3(TCRDB *rdb, TCMAP *recs);


/* Get the size of the value of a record in a remote database object.
   `rdb' specifies the remote database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1. */
int tcrdbvsiz(TCRDB *rdb, const void *kbuf, int ksiz);


/* Get the size of the value of a string record in a remote database object.
   `rdb' specifies the remote database object.
   `kstr' specifies the string of the key.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1. */
int tcrdbvsiz2(TCRDB *rdb, const char *kstr);


/* Initialize the iterator of a remote database object.
   `rdb' specifies the remote database object.
   If successful, the return value is true, else, it is false.
   The iterator is used in order to access the key of every record stored in a database. */
bool tcrdbiterinit(TCRDB *rdb);


/* Get the next key of the iterator of a remote database object.
   `rdb' specifies the remote database object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.  `NULL' is returned when no record is to be get out of the iterator.
   Because an additional zero code is appended at the end of the region of the return value, the
   return value can be treated as a character string.  Because the region of the return value is
   allocated with the `malloc' call, it should be released with the `free' call when it is no
   longer in use.  The iterator can be updated by multiple connections and then it is not assured
   that every record is traversed. */
void *tcrdbiternext(TCRDB *rdb, int *sp);


/* Get the next key string of the iterator of a remote database object.
   `rdb' specifies the remote database object.
   If successful, the return value is the string of the next key, else, it is `NULL'.  `NULL' is
   returned when no record is to be get out of the iterator.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use.  The iterator can be updated by
   multiple connections and then it is not assured that every record is traversed. */
char *tcrdbiternext2(TCRDB *rdb);


/* Get forward matching keys in a remote database object.
   `rdb' specifies the remote database object.
   `pbuf' specifies the pointer to the region of the prefix.
   `psiz' specifies the size of the region of the prefix.
   `max' specifies the maximum number of keys to be fetched.  If it is negative, no limit is
   specified.
   The return value is a list object of the corresponding keys.  This function does never fail
   and return an empty list even if no key corresponds.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcrdbfwmkeys(TCRDB *rdb, const void *pbuf, int psiz, int max);


/* Get forward matching string keys in a remote database object.
   `rdb' specifies the remote database object.
   `pstr' specifies the string of the prefix.
   `max' specifies the maximum number of keys to be fetched.  If it is negative, no limit is
   specified.
   The return value is a list object of the corresponding keys.  This function does never fail
   and return an empty list even if no key corresponds.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcrdbfwmkeys2(TCRDB *rdb, const char *pstr, int max);


/* Synchronize updated contents of a remote database object with the file and the device.
   `rdb' specifies the remote database object.
   If successful, the return value is true, else, it is false.
   This function fails and has no effect for on-memory database. */
bool tcrdbsync(TCRDB *rdb);


/* Remove all records of a remote database object.
   `rdb' specifies the remote database object.
   If successful, the return value is true, else, it is false. */
bool tcrdbvanish(TCRDB *rdb);


/* Copy the database file of a remote database object.
   `rdb' specifies the remote database object.
   `path' specifies the path of the destination file.  If it begins with `@', the trailing
   substring is executed as a command line.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code.
   The database file is assured to be kept synchronized and not modified while the copying or
   executing operation is in progress.  So, this function is useful to create a backup file of
   the database file.  This function fails and has no effect for on-memory database. */
bool tcrdbcopy(TCRDB *rdb, const char *path);


/* Restore the database file of a remote database object from the update log.
   `rdb' specifies the remote database object.
   `path' specifies the path of the update log directory.  If it begins with `+', the trailing
   substring is treated as the path and consistency checking is omitted.
   `ts' specifies the beginning timestamp in microseconds.
   If successful, the return value is true, else, it is false. */
bool tcrdbrestore(TCRDB *rdb, const char *path, uint64_t ts);


/* Set the replication master of a remote database object from the update log.
   `rdb' specifies the remote database object.
   `host' specifies the name or the address of the server.  If it is `NULL', replication of the
   database is disabled.
   `port' specifies the port number.
   If successful, the return value is true, else, it is false. */
bool tcrdbsetmst(TCRDB *rdb, const char *host, int port);


/* Get the number of records of a remote database object.
   `rdb' specifies the remote database object.
   The return value is the number of records or 0 if the object does not connect to any database
   server. */
uint64_t tcrdbrnum(TCRDB *rdb);


/* Get the size of the database of a remote database object.
   `rdb' specifies the remote database object.
   The return value is the size of the database or 0 if the object does not connect to any
   database server. */
uint64_t tcrdbsize(TCRDB *rdb);


/* Get the status string of the database of a remote database object.
   `rdb' specifies the remote database object.
   The return value is the status message of the database or `NULL' if the object does not
   connect to any database server.  The message format is TSV.  The first field of each line
   means the parameter name and the second field means the value.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcrdbstat(TCRDB *rdb);



__TCRDB_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
