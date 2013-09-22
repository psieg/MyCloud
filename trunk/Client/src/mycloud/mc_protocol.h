#ifndef MC_PROTOCOL_H
#define MC_PROTOCOL_H
#include "mc.h"
/* This unit defines the Codes used in our protocol and functions to pack/unpack messages */

#define MC_CLIENT_PROTOCOL_VERSION		9
#define MC_MIN_SERVER_PROTOCOL_VERSION	9

typedef int MC_SRVQUERY;
#define MC_SRVQRY_STATUS		100	// What's your status
#define MC_SRVQRY_AUTH			101	// Authorize + Timecheck (all following queries require an AuthToken)
#define MC_SRVQRY_LISTSYNCS		200	// List all your syncs (so I can compare with mine)
#define MC_SRVQRY_CREATESYNC	201 // Create new sync
#define MC_SRVQRY_DELSYNC		202 // Delete sync
#define MC_SRVQRY_LISTFILTERS	300 // List all filters for a sync
#define MC_SRVQRY_PUTFILTER		301 // Add / Replace a filter
#define MC_SRVQRY_DELFILTER		302 // Delete a filter
#define MC_SRVQRY_LISTSHARES	400 // List all shares for a sync
#define MC_SRVQRY_PUTSHARE		401 // Add / Replace a share
#define MC_SRVQRY_DELSHARE		402 // Delete a share
#define MC_SRVQRY_LISTDIR		501	// List all files in a dir (with their stats)
#define MC_SRVQRY_GETFILE		502	// Get the content of a file
#define MC_SRVQRY_GETMETA		503	// Get a file's metadata
#define MC_SRVQRY_GETOFFSET		504	// Get the offset of an incomplete file (to complete it)
#define MC_SRVQRY_PUTFILE		510	// Add / Replace a file
#define MC_SRVQRY_ADDFILE		511	// Add the following content to a file
#define MC_SRVQRY_PATCHFILE		512	// Patch the metaddata of a file
#define MC_SRVQRY_DELFILE		513	// Delete a file
#define MC_SRVQRY_PURGEFILE		520	// Purge this file (it's been hit by an ignore list)
#define MC_SRVQRY_NOTIYCHANGE	600	// Return when something changes or after timeout
#define MC_SRVQRY_PASSCHANGE	700	// Change password to X

typedef int MC_SRVSTATUS; 
#define MC_SRVSTAT_OK			100	// I'm good / Query successful
#define MC_SRVSTAT_AUTHED		101	// QRY_AUTH succeeded, here's your AuthToken
#define MC_SRVSTAT_SYNCLIST		200	// Here's a list of my syncs
#define MC_SRVSTAT_SYNCID		201	// ID of sync just created
#define MC_SRVSTAT_FILTERLIST	300 // List of filters for sync
#define MC_SRVSTAT_FILTERID		301 // ID of created / updated filter
#define MC_SRVSTAT_SHARELIST	400 // List of shares for sync
#define MC_SRVSTAT_SHAREID		401 // ID of created / updated share
#define MC_SRVSTAT_DIRLIST		500	// Here's the directory listing you requested
#define MC_SRVSTAT_FILE			501	// Here are contents of the file
#define MC_SRVSTAT_FILEMETA		502	// Metadata of the file
#define MC_SRVSTAT_OFFSET		503	// Here's how much of the file I have
#define MC_SRVSTAT_FILEID		510	// ID of file just created
#define MC_SRVSTAT_CHANGE		600 // ID of the changed watched sync
#define MC_SRVSTAT_NOCHANGE		601 // None of the watched syncs has changed 

#define MC_SRVSTAT_BADQRY		900	// I don't understand
#define MC_SRVSTAT_INTERROR		901	// Something went wrong
#define MC_SRVSTAT_NOTSETUP		902	// I'm not configured yet
#define MC_SRVSTAT_INCOMPATIBLE	903 // Your Protocol is too old
#define MC_SRVSTAT_UNAUTH		904	// QRY_AUTH failed or your AuthToken is invalid
#define MC_SRVSTAT_NOEXIST		910	// File/Sync does not exist (for you)
#define MC_SRVSTAT_EXISTS		911	// File/Sync does exist already, ID is
#define MC_SRVSTAT_NOTEMPTY		912	// Item to be deleted is not empty
#define MC_SRVSTAT_VERIFYFAIL	920	// Verify-Hash mismatch
#define MC_SRVSTAT_OFFSETFAIL	921	// Offset mismatch

/* These functions fill buf with the respective request body */
void pack_code(mc_buf *buf, int code);
void pack_auth(mc_buf *buf, string user, string passwd);
void pack_listsyncs(mc_buf *buf, unsigned char authtoken[16]);
void pack_createsync(mc_buf *buf, unsigned char authtoken[16], const string& name, bool crypted);
void pack_delsync(mc_buf *buf, unsigned char authtoken[16], int id);
void pack_listfilters(mc_buf *buf, unsigned char authtoken[16], int syncid);
void pack_putfilter(mc_buf *buf, unsigned char authtoken[16], mc_filter *filter);
void pack_delfilter(mc_buf *buf, unsigned char authtoken[16], int id);
void pack_listshares(mc_buf *buf, unsigned char authtoken[16], int syncid);
void pack_putshare(mc_buf *buf, unsigned char authtoken[16], mc_share *share);
void pack_delshare(mc_buf *buf, unsigned char authtoken[16], mc_share *share);
void pack_listdir(mc_buf *buf, unsigned char authtoken[16], int parent);
void pack_getfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 offset, int64 blocksize, unsigned char hash[16]);
void pack_getoffset(mc_buf *buf, unsigned char authtoken[16], int id);
void pack_putfile(mc_buf *buf, unsigned char authtoken[16], mc_file *file, int64 blocksize);
void pack_addfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 offset, int64 blocksize, unsigned char hash[16]);
void pack_patchfile(mc_buf *buf, unsigned char authtoken[16], mc_file *file);
void pack_delfile(mc_buf *buf, unsigned char authtoken[16], int id, int64 mtime);
void pack_getmeta(mc_buf *buf, unsigned char authtoken[16], int id);
void pack_purgefile(mc_buf *buf, unsigned char authtoken[16], int id);
void pack_notifychange(mc_buf *buf, unsigned char authtoken[16], list<mc_sync_db> *l);
void pack_passchange(mc_buf *buf, unsigned char authtoken[16], const string& newpass);

/* These functions fill the params with the response buffer's contents */
void unpack_authed(mc_buf *buf, int *version, unsigned char authtoken[16], int64 *time, int64 *basedate, int *uid);
void unpack_synclist(mc_buf *buf, list<mc_sync> *l);
void unpack_syncid(mc_buf *buf,  int *id);
void unpack_filterlist(mc_buf *buf, list<mc_filter> *l);
void unpack_filterid(mc_buf *buf, int *id);
void unpack_sharelist(mc_buf *buf, list<mc_share> *l);
void unpack_shareid(mc_buf *buf, int *id);
void unpack_dirlist(mc_buf *buf, list<mc_file> *l);
void unpack_file(mc_buf *buf, int64 *offset);
void unpack_offset(mc_buf *buf, int64 *offset);
void unpack_fileid(mc_buf *buf, int *id);
void unpack_filemeta(mc_buf *buf, mc_file *file);
void unpack_change(mc_buf *buf, int *id);

#endif /* MC_PROTOCOL_H */