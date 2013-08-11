<?php
require_once 'const.php';
require_once 'funcs.php';
require_once 'protocol_funcs.php';

function handle_listsyncs($ibuf,$uid){
	global $mysqli;
	$q = $mysqli->query("SELECT id,name,filterversion,crypted,hash FROM mc_syncs WHERE uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	$l = $q->fetch_all();
	//$l = array();
	//while($r = $q->fetch_row()){ $l[] = $r; }
	return pack_synclist($l);
}

function handle_listfilters($ibuf,$uid){
	global $mysqli;
	$sid = unpack_listfilters($ibuf);
	// sid 0 = rules for all syncs, not editable
	if($sid == 0){
		$q = $mysqli->query("SELECT id,sid,files,directories,type,rule FROM mc_filters WHERE sid = ".$sid); //." AND (uid = ".$uid." OR uid = 0)"); //no user-global syncs
	} else {
		$q = $mysqli->query("SELECT id,sid,files,directories,type,rule FROM mc_filters WHERE sid = ".$sid." AND uid = ".$uid);
	}
	if(!$q) return pack_interror($mysqli->error);
	$l = array();
	while($r = $q->fetch_row()){ $l[]= $r; }
	return pack_filterlist($l);
}

function handle_putfilter($ibuf,$uid){
	global $mysqli;
	$qry = unpack_putfilter($ibuf);
	if($qry['id'] == MC_FILTERID_NONE){ //new
		//sync must exist and be owned by user (unless 0 which would mean all my syncs)
		if($qry['sid'] != 0){
			$q = $mysqli->query("SELECT uid FROM mc_syncs WHERE id = ".$qry['sid']." AND uid = ".$uid);
			if(!$q) return pack_interror($mysqli->error);
			if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
		} else {
			return pack_interror(MC_SRVSTAT_BADQRY); //No (user-global-syncs at the moment)
		}
		
		//insert into table
		$q = $mysqli->query("INSERT INTO mc_filters (uid,sid,files,directories,type,rule) VALUES ".
			"(".$uid.", ".$qry['sid'].", ".($qry['files']?1:0).", ".($qry['directories']?1:0).", ".$qry['type'].", '".esc($qry['rule'])."')");
		if(!$q) return pack_interror($mysqli->error);
		$fid = $mysqli->insert_id;
	} else { //update
		//check old filter
		$q = $mysqli->query("SELECT id,sid FROM mc_filters WHERE id = ".$qry['id']." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
		if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
		$res = $q->fetch_row();
		if($res[1] != $qry['sid']) return pack_code(MC_SRVSTAT_BADQRY); //not allowed to switch to another sync

		//update table, uid,sid can't change
		$q = $mysqli->query("UPDATE mc_filters SET files = ".($qry['files']?1:0).", directories = ".($qry['directories']?1:0).", ".
			"type = ".$qry['type'].", rule = '".esc($qry['rule'])."' WHERE id = ".$qry['id']." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
		$fid = $res[0];
	}
	//update filterversion
	if($qry['sid'] == 0){ //update all my syncs
		$q = $mysqli->query("UPDATE mc_syncs SET filterversion = filterversion + 1 WHERE uid = ".$uid);
	} else { //update the sync
		$q = $mysqli->query("UPDATE mc_syncs SET filterversion = filterversion + 1 WHERE id = ".$qry['sid']." AND uid = ".$uid);
	}
	if(!$q) return pack_interror($mysqli->error);

	return pack_filterid($fid);
}

function handle_delfilter($ibuf,$uid){
	global $mysqli;
	$id = unpack_delfilter($ibuf);
	//check old filter
	$q = $mysqli->query("SELECT id,sid FROM mc_filters WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();

	//delete filter
	$q = $mysqli->query("DELETE FROM mc_filters WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);

	//update filterversion
	if($res[1] == 0){ //update all my syncs
		$q = $mysqli->query("UPDATE mc_syncs SET filterversion = filterversion + 1 WHERE uid = ".$uid);
	} else { //update the sync
		$q = $mysqli->query("UPDATE mc_syncs SET filterversion = filterversion + 1 WHERE id = ".$res[1]." AND uid = ".$uid);
	}
	if(!$q) return pack_interror($mysqli->error);

	return pack_code(MC_SRVSTAT_OK);
}

function handle_listdir($ibuf,$uid){
	global $mysqli;
	$parent = unpack_listdir($ibuf);
	$q = $mysqli->query("SELECT id,name,ctime,mtime,size,is_dir,parent,status,hash FROM mc_files WHERE parent = ".$parent." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	$l = array();
	while($r = $q->fetch_row()){ $l[] = $r; }
	return pack_dirlist($l);
}

function handle_getfile($ibuf,$uid){
	global $mysqli;
	$qry = unpack_getfile($ibuf);
	//Verify the file exists
	$q = $mysqli->query("SELECT id,is_dir,name,parent,size,status,hash FROM mc_files WHERE id = ".$qry[0]." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();
	if($res[1]) return pack_code(MC_SRVSTAT_BADQRY); // Can't get a directory, it has no contents
	if(strncmp($res[6],$qry[3],16)) return pack_code(MC_SRVSTAT_VERIFYFAIL);
	if($qry[2] < 0) return pack_code(MC_SRVSTAT_BADQRY); //Negative blocksize
	//if($qry[2] > $res[4]-$qry[1]) return pack_code(MC_SRVSTAT_BADQRY); //Can't get more than there is //can, just read whats there
	if($qry[2] > MC_MAXBUFSIZE) return pack_code(MC_SRVSTAT_BADQRY); //Exceeds servers's max bufsize
	if($qry[1] >= $res[4]) return pack_code(MC_SRVSTAT_BADQRY); // Offset behind end of file
	

	$filedata = filedata($res[0],$res[2],$res[3]);
	$fdesc = fopen($filedata[0],"rb");
	if(!$fdesc) return pack_interror("Failed to open file");
	fseek($fdesc,$qry[1],SEEK_SET);
	$readbytes = 0;
	$startoffset = $qry[1];
	$resbuf = fread($fdesc,$qry[2]);
	$readbytes = ftell($fdesc)-$startoffset;
	fclose($fdesc);

	return pack_file($readbytes,$resbuf);
}

function handle_getoffset($ibuf,$uid){
	global $mysqli;
	$id = unpack_getoffset($ibuf);
	$q = $mysqli->query("SELECT id,is_dir,name,parent,size,status FROM mc_files WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();
	if($res[1]) return pack_code(MC_SRVSTAT_BADQRY); // A directory can't be incomplete, covered by next
	//if($res[5] != MC_FILESTAT_INCOMPLETE_UP) return pack_code(MC_SRVSTAT_BADQRY); //Offset only relevant for incomplete files
	
	$filedata = filedata($res[0],$res[2],$res[3]);
	$fdesc = fopen($filedata[0],"rb");
	if(!$fdesc) return pack_interror("Failed to open file");
	fseek($fdesc,0,SEEK_END);
	$offset = ftell($fdesc);
	fclose($fdesc);

	return pack_offset($offset);
	
}

function handle_putfile($ibuf,$uid){
	global $mysqli;
	$qry = unpack_putfile($ibuf);
	
	if($qry['id'] == MC_FILEID_NONE){ //New file
		if($qry['is_dir']) $qry['hash'] = "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e"; //empty md5
		//Verify parent
		if($qry['parent'] < 0){
			$q = $mysqli->query("SELECT id FROM mc_syncs WHERE id = ".-$qry['parent']." AND uid = ".$uid);
			if(!$q) return pack_interror($mysqli->error);
			if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
			$res = $q->fetch_row();
		} else {
			$q = $mysqli->query("SELECT id,is_dir,status FROM mc_files WHERE id = ".$qry['parent']." AND uid = ".$uid);
			if(!$q) return pack_interror($mysqli->error);
			if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
			$res = $q->fetch_row();
			if($res[1] != 1) return pack_code(MC_SRVSTAT_BADQRY); //parent must of course point to a dir
			if($res[2] != MC_FILESTAT_COMPLETE) return pack_code(MC_SRVSTAT_BADQRY); //parent must not be deleted or anything
		}

		//Verify file does not exist already
		$q = $mysqli->query("SELECT id FROM mc_files WHERE parent = ".$qry['parent']." AND uid = ".$uid." AND LOWER(name) = LOWER('".esc($qry['name'])."')");
		if(!$q) return pack_interror($mysqli->error);
		$res = $q->fetch_row();
		if($res) return pack_exists($res[0]);

		$q = $mysqli->query("INSERT INTO mc_files (uid,name,ctime,mtime,size,is_dir,parent,hash,status) VALUES ".
			"(".$uid.", '".esc($qry['name'])."', ".$qry['ctime'].", ".$qry['mtime'].", ".$qry['size'].", ".
			($qry['is_dir']?1:0).", ".$qry['parent'].", '".esc($qry['hash'])."', ".
			($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." )");
		if(!$q) return pack_interror($mysqli->error);

		//return pack_interror("INSERT INTO mc_files(uid,name,ctime,mtime,size,is_dir,parent,hash,status) VALUES ".
		//	"(".$uid.", '".esc($qry['name'])."', ".$qry['ctime'].", ".$qry['mtime'].", ".$qry['size'].", ".
		//	($qry['is_dir']?1:0).", ".$qry['parent'].", '".esc($qry['hash'])."', ".
		//	($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." )");
		$fid = $mysqli->insert_id;
		$filedata = filedata($fid,$qry['name'],$qry['parent']);
		if($qry['is_dir']){
			$rc = mkdir($filedata[0]);
			if(!$rc) return pack_interror("Failed to create directory");
		}
	} else { //Modification
		//Source exists?
		$q = $mysqli->query("SELECT name,parent,is_dir,status FROM mc_files WHERE id = ".$qry['id']." AND uid = ".$uid);
		if(!$q) pack_interror($mysqli->error);
		if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
		$res = $q->fetch_row();
		if(($res[2]==1) != $qry['is_dir']) return pack_code(MC_SRVSTAT_BADQRY); //Modifying from dir to file not allowed
		$oldfiledata = filedata($qry['id'],$res[0],$res[1]);


		if(($qry['parent'] != $res[1]) || ($qry['name'] != $res[0])){ //If it's a move (=parent change)
			if($qry['is_dir']) $qry['hash'] = "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e"; //empty md5
			//Verify parent
			if($qry['parent'] != $res[1]){
				if($qry['parent'] < 0){
					$q = $mysqli->query("SELECT id FROM mc_syncs WHERE id = ".-$qry['parent']." AND uid = ".$uid);
					if(!$q) return pack_interror($mysqli->error);
					if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
					$res = $q->fetch_row();
				} else {
					$q = $mysqli->query("SELECT id,is_dir,status FROM mc_files WHERE id = ".$qry['parent']." AND uid = ".$uid);
					if(!$q) return pack_interror($mysqli->error);
					if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
					$res = $q->fetch_row();
					if($res[1] != 1) return pack_code(MC_SRVSTAT_BADQRY); //parent must of course point to a dir
					if($res[2] != MC_FILESTAT_COMPLETE) return pack_code(MC_SRVSTAT_BADQRY); //parent must not be deleted or anything
				}
			}

			//Target exists?
			$q = $mysqli->query("SELECT id FROM mc_files WHERE parent = ".$qry['parent']." AND uid = ".$uid." AND LOWER(name) = LOWER('".esc($qry['name'])."')");
			if(!$q) return pack_interror($mysqli->error);
			$res = $q->fetch_row();
			if($res) pack_exists($res[0]);

			$filedata = filedata($qry['id'],$qry['name'],$qry['parent']);
		
			return pack_interror("This is a move. You don't want that.");
	
			$rc = rename($oldfiledata[0],$filedata[1]);
			if(!$rc) return pack_interror("Failed to rename file");

			$q = $mysqli->query("UPDATE mc_files SET status = ".MC_FILESTAT_MOVED." ".
				"WHERE id = ".$qry['id']." AND uid = ".$uid);
			if(!$q) return pack_interror($mysqli->error);

			if($qry['is_dir'])
				$q = $mysqli->query("INSERT INTO mc_files(uid,name,ctime,mtime,size,is_dir,parent,status) VALUES ".
					"(".$uid.", '".esc($qry['name'])."', ".$qry['ctime'].", ".$qry['mtime'].", ".$qry['size'].", ".
					($qry['is_dir']?1:0).", ".$qry['parent'].", '".
					($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." )");
			else
				$q = $mysqli->query("INSERT INTO mc_files(uid,name,ctime,mtime,size,is_dir,parent,hash,status) VALUES ".
					"(".$uid.", '".esc($qry['name'])."', ".$qry['ctime'].", ".$qry['mtime'].", ".$qry['size'].", ".
					($qry['is_dir']?1:0).", ".$qry['parent'].", '".esc($qry['hash'])."', ".
					($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." )");
			if(!$q) return pack_interror($mysqli->error);
			$fid = $mysqli->insert_id;
		} else { //Just a content change
			$filedata = filedata($qry['id'],$qry['name'],$qry['parent']);
			if($qry['is_dir'])
				$q = $mysqli->query("UPDATE mc_files SET ctime = ".$qry['ctime'].", ".
					"mtime = ".$qry['mtime'].", size = ".$qry['size'].", ".
					"is_dir = ".($qry['is_dir']?1:0).", ".
					"status = ".MC_FILESTAT_COMPLETE." ".
					"WHERE id = ".$qry['id']." AND uid = ".$uid);
			else
				$q = $mysqli->query("UPDATE mc_files SET ctime = ".$qry['ctime'].", ".
					"mtime = ".$qry['mtime'].", size = ".$qry['size'].", ".
					"is_dir = ".($qry['is_dir']?1:0).", hash = '".esc($qry['hash'])."', ".
					"status = ".($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." ".
					"WHERE id = ".$qry['id']." AND uid = ".$uid);
			if(!$q) return pack_interror($mysqli->error);
			$fid = $qry['id'];
		}
		if($qry['is_dir'] && $res[3] == MC_FILESTAT_DELETED){
			$rc = mkdir($filedata[0]);
			if(!$rc) return pack_interror("Failed to create directory");			
		}

	}
	if(!$qry['is_dir']){
		$fdesc = fopen($filedata[0],"wb");
		if(!$fdesc) return pack_interror("Failed to open file");
		$readbytes = 0;
		$startoffset = ftell($fdesc);
		while($readbytes < $qry['blocksize']){
			fwrite($fdesc,fread($ibuf,MC_BUFSIZE),MC_BUFSIZE);
			$readbytes = ftell($fdesc)-$startoffset;
			if(feof($ibuf) && $readbytes < $qry['blocksize']) return pack_code(MC_SRVSTAT_BADQRY);
		}
		fclose($fdesc);
	}
	$rc = touch($filedata[0],$qry['mtime']);
	if(!$rc) return pack_interror("Failed to set mtime");
	
	if(isset($oldfiledata) && ($oldfiledata[1] != $filedata[1])){
		$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$oldfiledata[1]);
		if(!$q) return pack_interror($mysqli->error);
	}

	updateHash($fid);	

	//$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$filedata[1]);
	//if(!$q) return pack_interror($mysqli->error);

	return pack_fileid($fid);
}

function handle_addfile($ibuf,$uid){
	global $mysqli;
	$qry = unpack_addfile($ibuf);
	// Verify the file exists
	$q = $mysqli->query("SELECT name,mtime,size,is_dir,parent,hash FROM mc_files WHERE id = ".$qry[0]." AND uid = ".$uid);
	if(!$q) pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();
	//if($res[3] != 0) return pack_code(MC_SRVSTAT_BADQRY); // We do not add data do a dir
	if($res[3] != 0) return pack_interror("We dont add data to a dir".$res[3]);
	if(strncmp($res[5],$qry[3],16)) return pack_code(MC_SRVSTAT_VERIFYFAIL);
	$filedata = filedata($qry[0],$res[0],$res[4]);
	$fdesc = fopen($filedata[0],"ab");
	if(!$fdesc) return pack_interror("Failed to open file");
	fseek($fdesc,0,SEEK_END);
	$offset = ftell($fdesc);
	if($offset != $qry[1]) return pack_code(MC_SRVSTAT_OFFSETFAIL); // Add at end pls
	$readbytes = 0;
	while($readbytes < $qry[2]){
		fwrite($fdesc,fread($ibuf,MC_BUFSIZE),MC_BUFSIZE);
		$readbytes = ftell($fdesc)-$offset;
		if(feof($ibuf) && $readbytes < $qry[2]) return pack_code(MC_SRVSTAT_BADQRY);
	}
	if(ftell($fdesc) == $res[2]){
		$q = $mysqli->query("UPDATE mc_files SET status = ".MC_FILESTAT_COMPLETE." WHERE id = ".$qry[0]." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
		updateHash($qry[0]);
	}
	fclose($fdesc);
	$rc = touch($filedata[0],$res[1]);
	if(!$rc) return pack_interror("Failed to set mtime");

	//$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$filedata[1]);
	//if(!$q) return pack_interror($mysqli->error);
	return pack_code(MC_SRVSTAT_OK);
}

function handle_patchfile($ibuf,$uid){
	global $mysqli;
	$qry = unpack_patchfile($ibuf);
	//Source exists?
	$q = $mysqli->query("SELECT name,parent,is_dir,status FROM mc_files WHERE id = ".$qry['id']." AND uid = ".$uid);
	if(!$q) pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();
	$oldfiledata = filedata($qry['id'],$res[0],$res[1]);

	if(($qry['parent'] != $res[1]) || (strcasecmp($qry['name'],$res[0]) != 0)){ //If it's a move (=parent change)
		return pack_interror("Not Implemented / tested: ".$qry['name']."/".$res[0]);

		//Verify parent
		if($qry['parent'] != $res[1]){
			if($qry['parent'] < 0){
				$q = $mysqli->query("SELECT id FROM mc_syncs WHERE id = ".-$qry['parent']." AND uid = ".$uid);
				if(!$q) return pack_interror($mysqli->error);
				if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
				$res = $q->fetch_row();
			} else {
				$q = $mysqli->query("SELECT id,is_dir,status FROM mc_files WHERE id = ".$qry['parent']." AND uid = ".$uid);
				if(!$q) return pack_interror($mysqli->error);
				if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
				$res = $q->fetch_row();
				if($res[1] != 1) return pack_code(MC_SRVSTAT_BADQRY); //parent must of course point to a dir
				if($res[2] != MC_FILESTAT_COMPLETE) return pack_code(MC_SRVSTAT_BADQRY); //parent must not be deleted or anything
			}
		}

		//Target exists?
		$q = $mysqli->query("SELECT id FROM mc_files WHERE parent = ".$qry['parent']." AND uid = ".$uid." AND LOWER(name) = LOWER('".esc($qry['name'])."')");
		if(!$q) return pack_interror($mysqli->error);
		$res = $q->fetch_row();
		if($res) pack_exists($res[0]);

		$filedata = filedata($qry['id'],$qry['name'],$qry['parent']);
	
		return pack_interror("This is a move. You don't want that.");	

		$rc = rename($oldfiledata[0],$filedata[1]);
		if(!$rc) return pack_interror("Failed to rename file");

		$q = $mysqli->query("UPDATE mc_files SET status = ".MC_FILESTAT_MOVED." ".
			"WHERE id = ".$qry['id']." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
		
		$q = $mysqli->query("INSERT INTO mc_files(uid,name,ctime,mtime,size,is_dir,parent,hash,status) VALUES ".
			"(".$uid.", '".esc($qry['name'])."', ".$qry['ctime'].", ".$qry['mtime'].", ".$qry['size'].", ".
			($qry['is_dir']?1:0).", ".$qry['parent'].", '".esc($qry['hash'])."', ".
			($qry['size']==$qry['blocksize']?MC_FILESTAT_COMPLETE:MC_FILESTAT_INCOMPLETE_UP)." )");
		if(!$q) return pack_interror($mysqli->error);
	} else {
		$filedata = filedata($qry['id'],$qry['name'],$qry['parent']);
		if($res[2] && $res[3] != MC_FILESTAT_DELETED){
			$rc = mkdir($filedata[0]);
			if(!$rc) return pack_interror("Failed to create directory");			
		}
	}
	$q = $mysqli->query("UPDATE mc_files SET name = '".esc($qry['name'])."', ctime = ".$qry['ctime'].", ".
			"mtime = ".$qry['mtime'].", parent = ".$qry['parent']." ".
			"WHERE id = ".$qry['id']." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);

	if($res[3] =! MC_FILESTAT_DELETED){
		$rc = touch($filedata[0],$qry['mtime']);
		if(!$rc) return pack_interror("Failed to set mtime");
	}
		
	//if(isset($oldfiledata) && ($oldfiledata[1] != $filedata[1])){
	//	$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$oldfiledata[1]);
	//	if(!$q) return pack_interror($mysqli->error);
	//}

	updateHash($qry['id']);

	//$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$filedata[1]);
	//if(!$q) return pack_interror($mysqli->error);
	return pack_code(MC_SRVSTAT_OK);
}

function handle_delfile($ibuf,$uid){
	global $mysqli;
	$qry = unpack_delfile($ibuf);
	$q = $mysqli->query("SELECT id FROM mc_files WHERE parent = ".$qry[0]." AND uid = ".$uid." AND status != ".MC_FILESTAT_DELETED);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows != 0) return pack_code(MC_SRVSTAT_NOTEMPTY);

	$q = $mysqli->query("SELECT status FROM mc_files WHERE id = ".$qry[0]);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();

	if($res[0] == MC_FILESTAT_DELETED){
		$q = $mysqli->query("UPDATE mc_files  SET mtime = ".$qry[1]." WHERE id = ".$qry[0]." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
	} else {
		$q = $mysqli->query("UPDATE mc_files SET mtime = ".$qry[1].", status = ".MC_FILESTAT_DELETED." ".
			"WHERE id = ".$qry[0]." AND uid = ".$uid);
		if(!$q) return pack_interror($mysqli->error);
		$filedata = filedata($qry[0]);
		if(is_dir($filedata[0])){
			$rc = rmdir($filedata[0]);
			if(!$rc) return pack_interror("Failed to rmdir the file");
		} else {
			$rc = unlink($filedata[0]);
			if(!$rc) return pack_interror("Failed to unlink the file");
		}
	}
	updateHash($qry[0]);

	//$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$filedata[1]);
	//if(!$q) return pack_interror($mysqli->error);
	return pack_code(MC_SRVSTAT_OK);
	
}

function handle_getmeta($ibuf,$uid){
	global $mysqli;
	$id = unpack_getmeta($ibuf);
	$q = $mysqli->query("SELECT id,name,ctime,mtime,size,is_dir,parent,status,hash FROM mc_files WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$f = $q->fetch_row();
	return pack_filemeta($f);
}

function handle_purgefile($ibuf,$uid){
	global $mysqli;
	$id = unpack_purgefile($ibuf);
	$q = $mysqli->query("SELECT id FROM mc_files WHERE parent = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);

	$q = $mysqli->query("SELECT status,parent FROM mc_files WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);
	if($q->num_rows == 0) return pack_code(MC_SRVSTAT_NOEXIST);
	$res = $q->fetch_row();
	$status = $res[0];
	$parent = $res[1];

	$filedata = filedata($id); // must get filedata before dropping the entry

	$q = $mysqli->query("DELETE FROM mc_files WHERE id = ".$id." AND uid = ".$uid);
	if(!$q) return pack_interror($mysqli->error);

	//If file deletion failed (normally because the file doesnt exist) we want the db entry removed
	if($status != MC_FILESTAT_DELETED){
		if(is_dir($filedata[0])){
			$rc = rmdir($filedata[0]);
			if(!$rc) return pack_interror("Failed to rmdir the file");
		} else {
			$rc = unlink($filedata[0]);
			if(!$rc) return pack_interror("Failed to unlink the file");
		}
	}

	updateHash($parent);

	//$q = $mysqli->query("UPDATE mc_syncs SET synctime = ".time()." WHERE id = ".$filedata[1]);
	//if(!$q) return pack_interror($mysqli->error);
	return pack_code(MC_SRVSTAT_OK);
}

function handle_notifychange($ibuf,$uid){
	global $mysqli;
	$list = unpack_notifychange($ibuf);
	if(count($list) == 0) return pack_code(MC_SRVSTAT_BADQRY);
	ksort($list); //by id
	$t = time(NULL);
	$s = "";
	foreach(array_keys($list) as $sid) $s .= $sid.",";
	$s = substr($s,0,strlen($s)-1);
//	return pack_interror($s); 

	while(time(NULL) - $t < 30){
		$q = $mysqli->query("SELECT id,hash FROM mc_syncs WHERE id IN (".$s.") ORDER BY id");
		if(!$q) return pack_interror($mysqli->error);
		while($res = $q->fetch_row()){
			if($list[$res[0]] != $res[1]) 
				//return pack_interror("Mismatch: ".bin2hex($list[$res[0]])."/".bin2hex($res[1]));
				return pack_change($res[0]);
		}
		sleep(2);
	}
	return pack_code(MC_SRVSTAT_NOCHANGE);
}
?>
<?php
