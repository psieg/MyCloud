<?php
define('MC_DB_USER',"www");
define('MC_DB_PASS',"linuxserverwww");
define('MC_DB_DATABASE',"cloud");
define('MC_FS_BASEDIR',"/www/cloud/data");
define('MC_BUFSIZE',1024*1024); // 1MB = ca. 1sec at avg speed
define('MC_MAXBUFSIZE',1024*1024);

define('MC_SESS_EXPIRE',300); // How long inactive until a new auth is neede
define('MC_TOKEN_REGEN',7200); // Maximum time one token is valid
?>
