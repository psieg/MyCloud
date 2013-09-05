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
define("MC_SRVQRY_LISTDIR",		401);	// List all files in a dir (with their stats)
define("MC_SRVQRY_GETFILE",		402);	// Get the content of a file
define("MC_SRVQRY_GETMETA",		403);	// Get a file's metadata
define("MC_SRVQRY_GETOFFSET",	404);	// Get the offset of an incomplete file (to complete it)
define("MC_SRVQRY_PUTFILE",		410);	// Add / Replace a file
define("MC_SRVQRY_ADDFILE",		411);	// Add the following content to a file
define("MC_SRVQRY_PATCHFILE",	412);	// Patch the metaddata of a file
define("MC_SRVQRY_DELFILE",		413);	// Delete a file
define("MC_SRVQRY_PURGEFILE",	420);	// Purge this file (it's been hit by an ignore list)
define("MC_SRVQRY_NOTIFYCHANGE",500);	// Return when something changes or after timeout
define("MC_SRVQRY_PASSCHANGE",	600);	// Change password to X

define("MC_SRVSTAT_OK",			100);	// I'm good / Query successful
define("MC_SRVSTAT_AUTHED",		101);	// QRY_AUTH succeeded, here's your AuthToken
define("MC_SRVSTAT_SYNCLIST",	200);	// Here's a list of my syncs
define("MC_SRVSTAT_SYNCID",		201);	// ID of sync just created
define("MC_SRVSTAT_FILTERLIST",	300);	// List of filters for sync
define("MC_SRVSTAT_FILTERID",	301);	// ID of created / updated filter
define("MC_SRVSTAT_DIRLIST",	400);	// Here's the directory listing you requested
define("MC_SRVSTAT_FILE",		401);	// Here are contents of the file
define("MC_SRVSTAT_FILEMETA",	402);	// Metadata of the file
define("MC_SRVSTAT_OFFSET",		403);	// Here's how much of the file I have
define("MC_SRVSTAT_FILEID",		410);	// ID of file just created
define("MC_SRVSTAT_CHANGE",		500);	// ID of the changed watched sync
define("MC_SRVSTAT_NOCHANGE",	501);	// None of the watched syncs has changed

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
