<?php
// To be used in query() parameter strings only
function esc($data){
	global $mysqli;
	//return $mysqli->real_escape_string(str_replace(chr(145), 'X', $data));
	return $mysqli->real_escape_string($data);
}

// Consequently used for later implementation / change
// Provides full path and syncid for a file
function filedata($fid,$fname=NULL,$fparent=NULL){
	global $mysqli;
	// Cascade through files until we reach a uname, syncname and path to get the full path and sync id
	//TODO: Might want to speed this up by caching this value (ext. table?)
	if($fname===NULL || $fparent===NULL){
		$q = $mysqli->query("SELECT name,parent FROM mc_files WHERE id = ".$fid);
		if(!$q) print($mysqli->error);
		$res = $q->fetch_row();
		$full = $res[0];
		$parent = $res[1];
	} else {
		$full = $fname;
		$parent = $fparent;
	}
	while($parent >= 0){
		$q = $mysqli->query("SELECT name,parent FROM mc_files WHERE id = ".$parent);
		if(!$q) print($mysqli->error);
		$res = $q->fetch_row();
		$full = $res[0]."/".$full;
		$parent = $res[1];
	}
	$q = $mysqli->query("SELECT id,uid,name FROM mc_syncs WHERE id = ".-$parent);
	if(!$q) print($mysqli->error);
	$res = $q->fetch_row();
	$full = $res[2]."/".$full;
	$sid = $res[0];
	$q = $mysqli->query("SELECT name FROM mc_users WHERE id = ".$res[1]);
		if(!$q) print($mysqli->error);
	$res = $q->fetch_row();
	$full = MC_FS_BASEDIR."/".$res[0]."/".$full;
	return array($full,$sid);
}

// Compute and return the hash for directory $fid
function directoryHash($fid){
	global $mysqli;
	$q = $mysqli->query("SELECT id,name,ctime,mtime,size,is_dir,hash FROM mc_files WHERE parent = ".$fid." ORDER BY id");
	if(!$q) print($mysqli->error);
	$str = "";
	while($r = $q->fetch_row()){
		$str .= pack("l1",$r[0]).$r[1].
			//pack("L2",$r[2]&0xFFFFFFFF,($r[2]&0xFFFFFFFF00000000)>>32). //ctime is not an important / deciding criteria
			pack("L2",$r[3]&0xFFFFFFFF,($r[3]&0xFFFFFFFF00000000)>>32).
			pack("L2",$r[4]&0xFFFFFFFF,($r[4]&0xFFFFFFFF00000000)>>32).
			pack("C1",$r[5]).
			$r[6];
	}
	return md5($str,true);
	//return array(md5($str,true),$str);
}
// Cascade up the tree, updating directory hashes
// TODO: this whole thing is not actually threadsafe...
function updateHash($fid,$fparent=NULL,$fis_dir=NULL){
	global $mysqli;
	if($fparent===NULL || $fis_dir===NULL){
		$q = $mysqli->query("SELECT parent,is_dir FROM mc_files WHERE id = ".$fid);
		if(!$q) print($mysqli->error);
		$res = $q->fetch_row();
		$fparent = $res[0];
		$fis_dir = $res[1];
	}
	$id = ($fis_dir?$fid:$fparent);
	while($id >= 0){
		$hash = directoryHash($id);
		$q = $mysqli->query("UPDATE mc_files SET hash = '".esc($hash)."' WHERE id = ".$id);
		//$q = $mysqli->query("UPDATE mc_files SET hash = '".esc($hash[0])."', hashdata = '".esc($hash[1])."' WHERE id = ".$id);
		if(!$q) print($mysqli->error);
		$q = $mysqli->query("SELECT parent FROM mc_files WHERE id = ".$id);
		if(!$q) print($mysqli->error);
		$id = $q->fetch_row()[0];
	}
	$hash = directoryHash($id);
	$q = $mysqli->query("UPDATE mc_syncs SET hash = '".esc($hash)."' WHERE id = ".-$id);
	//$q = $mysqli->query("UPDATE mc_syncs SET hash = '".esc($hash[0])."', hashdata = '".esc($hash[1])."' WHERE id = ".-$id);
	if(!$q) print($mysqli->error);
}
?>
