<?php
define('MC_SERVER_PROTOCOL_VERSION',9);
define('MC_MIN_CLIENT_PROTOCOL_VERSION',9);

define("MC_SRVQRY_STATUS",		100);	// What's your status
define("MC_SRVQRY_AUTH",		101);	// Authorize + Timecheck (all following queries require an AuthToken)
define("MC_SRVQRY_LISTSYNCS",	200);	// List all your syncs (so I can compare with mine)
define("MC_SRVQRY_CREATESYNC",	201);	// Create new sync
define("MC_SRVQRY_DELSYNC",		202);	// Delete sync
define("MC_SRVQRY_LISTFILTERS",	300);	// List all filters for async
define("MC_SRVQRY_PUTFILTER",	301);	// Add / Replace a filter
define("MC_SRVQRY_DELFILTER",	302);	// Delete a filter
define("MC_SRVQRY_LISTSHARES",	400);	// List all shares for async
define("MC_SRVQRY_PUTSHARE",	401);	// Add / Replace a share
define("MC_SRVQRY_DELSHARE",	402);	// Delete a share
define("MC_SRVQRY_LISTDIR",		501);	// List all files in a dir (with their stats)
define("MC_SRVQRY_GETFILE",		502);	// Get the content of a file
define("MC_SRVQRY_GETMETA",		503);	// Get a file's metadata
define("MC_SRVQRY_GETOFFSET",	504);	// Get the offset of an incomplete file (to complete it)
define("MC_SRVQRY_PUTFILE",		510);	// Add / Replace a file
define("MC_SRVQRY_ADDFILE",		511);	// Add the following content to a file
define("MC_SRVQRY_PATCHFILE",	512);	// Patch the metaddata of a file
define("MC_SRVQRY_DELFILE",		513);	// Delete a file
define("MC_SRVQRY_PURGEFILE",	520);	// Purge this file (it's been hit by an ignore list)
define("MC_SRVQRY_NOTIFYCHANGE",600);	// Return when something changes or after timeout
define("MC_SRVQRY_PASSCHANGE",	700);	// Change password to X

define("MC_SRVSTAT_OK",			100);	// I'm good / Query successful
define("MC_SRVSTAT_AUTHED",		101);	// QRY_AUTH succeeded, here's your AuthToken
define("MC_SRVSTAT_SYNCLIST",	200);	// Here's a list of my syncs
define("MC_SRVSTAT_SYNCID",		201);	// ID of sync just created
define("MC_SRVSTAT_FILTERLIST",	300);	// List of filters for sync
define("MC_SRVSTAT_FILTERID",	301);	// ID of created / updated filter
define("MC_SRVSTAT_SHARELIST",	400);	// List of shares for sync
define("MC_SRVSTAT_SHAREID",	401);	// ID of created / updated share
define("MC_SRVSTAT_DIRLIST",	500);	// Here's the directory listing you requested
define("MC_SRVSTAT_FILE",		501);	// Here are contents of the file
define("MC_SRVSTAT_FILEMETA",	502);	// Metadata of the file
define("MC_SRVSTAT_OFFSET",		503);	// Here's how much of the file I have
define("MC_SRVSTAT_FILEID",		510);	// ID of file just created
define("MC_SRVSTAT_CHANGE",		600);	// ID of the changed watched sync
define("MC_SRVSTAT_NOCHANGE",	601);	// None of the watched syncs has changed

define("MC_SRVSTAT_BADQRY",		900);	// I don't understand
define("MC_SRVSTAT_INTERROR",	901);	// Something went wrong
define("MC_SRVSTAT_NOTSETUP",	902);	// I'm not configured yet
define("MC_SRVSTAT_INCOMPATIBLE",903);	// Your Protocol is too old
define("MC_SRVSTAT_UNAUTH",		904);	// QRY_AUTH failed or your AuthToken is invalid
define("MC_SRVSTAT_NOEXIST",	910);	// File/Sync does not exist (for you)
define("MC_SRVSTAT_EXISTS",		911);	// File/Sync does exist already, ID is
define("MC_SRVSTAT_NOTEMPTY",	912);	// Item to be deleted is not empty
define("MC_SRVSTAT_VERIFYFAIL",	920);	// Verify-Hash mismatch
define("MC_SRVSTAT_OFFSETFAIL",	921);	// Offset mismatch

?>
