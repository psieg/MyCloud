<?php
require_once 'const.php';

echo "<pre>";
if(isset($_GET['phase1'])){
	$queries = array(
		'CREATE TABLE mc_status (basedate BIGINT)',
		'CREATE TABLE mc_users (id INTEGER PRIMARY KEY AUTO_INCREMENT, name VARCHAR(255) NOT NULL, password VARCHAR(255) NOT NULL, token BINARY(16), lastseen BIGINT NOT NULL, lastregen BIGINT NOT NULL, keyring BLOB, UNIQUE(name)) COLLATE utf8_general_ci',
		'CREATE TABLE mc_syncs (id INTEGER PRIMARY KEY AUTO_INCREMENT, uid INTEGER NOT NULL, name VARCHAR(255) NOT NULL, filterversion INTEGER NOT NULL, shareversion INTEGER NOT NULL, crypted INTEGER NOT NULL, hash BINARY(16)) COLLATE utf8_general_ci',
		'CREATE TABLE mc_files (id INTEGER PRIMARY KEY AUTO_INCREMENT, uid INTEGER NOT NULL, sid INTEGER NOT NULL, name VARCHAR(511) NOT NULL, ctime BIGINT NOT NULL, mtime BIGINT NOT NULL, size BIGINT NOT NULL, is_dir BOOLEAN NOT NULL, parent INTEGER NOT NULL, status INTEGER NOT NULL, hash BINARY(16), INDEX(parent,uid,sid), INDEX(id,uid)) COLLATE utf8_general_ci',
		'CREATE TABLE mc_filters (id INTEGER PRIMARY KEY AUTO_INCREMENT, uid INTEGER NOT NULL, sid INTEGER NOT NULL, files INTEGER NOT NULL, directories INTEGER NOT NULL, type INTEGER NOT NULL, rule VARCHAR(255) NOT NULL, comment VARCHAR(255), INDEX(sid,uid)) COLLATE utf8_general_ci',
		'CREATE TABLE mc_shares (sid INTEGER NOT NULL, uid INTEGER NOT NULL)',
		#default filter rules
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,1,'.MC_FILTERT_REGEX_FULLNAME.',".*\\\.mc_conflict\\\.?.*","MyCloud conflicted files")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_FULLNAME.',"desktop.ini","desktop.ini (permission issues)")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_FULLNAME.',"Thumbs.db","Thumbs.db")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"ncb","VC IntelliSense Databases")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"sdf","VS2012 Cache Databases")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"opensdf","VS2012 Cache Databases")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"~dpr","Delphi Temp/Backup Files")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"~pas","Delphi Temp/Backup Files")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"~dfm","Delphi Temp/Backup Files")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"dcu","Delphi Compiled Unit")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"class","Java Class File")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"obj","VC Object File")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"manifest","VC Manifest File")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"pch","VC Precompiled Header File")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"ilk","VC Linker Files")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"idb","VC Incremental Linker DB")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"lastbuildstate","VC Last Build State")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"unsuccessfulbuild","VC Unsuccessful Build")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_MATCH_EXTENSION.',"tlog","VC Transaction Log")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,0,1,'.MC_FILTERT_MATCH_FULLNAME.',"GeneratedFiles","(Qt) Generated Files")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_REGEX_PATH.',".*/Debug/[^/]*\\\.(htm|manifest|res|ilk|obj|pch|idb|pdb|dep|tlog)","VC Compiled Files")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,0,'.MC_FILTERT_REGEX_NAME.',"~\\\$.*","MS Office-Backups")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,1,'.MC_FILTERT_MATCH_EXTENSION.',"tmp","Temporary Files")',
		'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,1,'.MC_FILTERT_MATCH_EXTENSION.',"swp","Swap Files")',
		#'INSERT INTO mc_filters (uid,sid,files,directories,type,rule,comment) VALUES (0,0,1,1,'.MC_FILTERT_.',"","")',
	
		#debug
		#'INSERT INTO  mc_users (name, password) VALUES ("user", "pass")',
		#'INSERT INTO  mc_syncs (uid,name,hash,filterversion) VALUES (-1,"Cloud", UNHEX("d41d8cd98f00b204e9800998ecf8427e"), 1)',
	);

	foreach($queries as $qry){
		echo $qry."\n";
		$q = $mysqli->query($qry);
		if(!$q){ echo "Error: ".$mysqli->error; break; }
	}

	echo "</pre>";
	echo "<a href=\"?phase2\">click to continue</a>";

} else if(isset($_GET['phase2'])){

	include 'scan.php';
	uscan(MC_FS_BASEDIR);

	echo "</pre>";
	echo "<a href=\"?phase3\">click to continue</a>";
} else if(isset($_GET['phase3'])){

	include 'scan.php';
	
	echo "If you exceeded max_execution_time, <a href=\"?phase3\">click here</a>\n";

	//hscan(MC_FS_BASEDIR);
	scan2(MC_FS_BASEDIR);

	echo "</pre>";
	echo "<a href=\"?phase4\">click to continue</a>";
} else if(isset($_GET['phase4'])){
/*
	include 'scan.php';
	
	//echo "If you exceeded max_execution_time, <a href=\"?phase3\">click here</a>\n";

	shscan();

	echo "</pre>";
	echo "<a href=\"?phase5\">click to continue</a>";

} else if(isset($_GET['phase5'])){ */
	$queries = array(
		#'UPDATE mc_syncs SET uid = -uid',
		'INSERT INTO mc_status VALUES ('.time().')',
	);

	foreach($queries as $qry){
		echo $qry."\n";
		$q = $mysqli->query($qry);
		if(!$q){ echo "Error: ".$mysqli->error; break; }
	}

	echo "</pre>";
	echo "<a href=\"?\">done</a>";
}

?>
