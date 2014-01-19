<?php
define('MC_DB_USER',"www");
define('MC_DB_PASS',"linuxserverwww");
define('MC_DB_DATABASE',"cloud");
define('MC_FS_BASEDIR',"/www/cloud/data");
define('MC_BUFSIZE',1024*1024); // size of blocks used when receiving 
define('MC_MAXBUFSIZE',10*1024*1024); // size of block used when sending

define('MC_SESS_EXPIRE',605); // How long inactive until a new auth is neede
define('MC_TOKEN_REGEN',21600); // Maximum time one token is valid
?>
