<?php
function unpack_code($fdesc){
	return unpack("l1",fread($fdesc,4))[1];
}

function unpack_token($fdesc){
	return fread($fdesc,16);
}

#Note code (and token) are part of the protocl but are read before
#Thus here are only functions for queries with parameters
function unpack_auth($fdesc){
	$data = fread($fdesc,4);
	$data = unpack("l1len",$data);
	$user = fread($fdesc,$data['len']);
	$data = fread($fdesc,4);
	$data = unpack("l1len",$data);
	$pass = fread($fdesc,$data['len']);
	$data = fread($fdesc,4);
	$clientver = unpack("l1ver",$data)['ver'];
	return array($user,$pass,$clientver);
}

function unpack_createsync($fdesc){
	$data = unpack("C1crypted/l1len",fread($fdesc,5));
	$crypted = ($data['crypted'] != 0);
	$name = fread($fdesc,$data['len']);
	return array($crypted,$name);
}

function unpack_delsync($fdesc){
	return unpack("l1sid",fread($fdesc,4))['sid'];
}

function unpack_listfilters($fdesc){
	return unpack("l1sid",fread($fdesc,4))['sid'];

}

function unpack_putfilter($fdesc){
	$result = array();
	$data = unpack("l1id/l1sid/C1files/C1directories/l1type/l1len",fread($fdesc,18));
	$result['id'] = $data['id'];
	$result['sid'] = $data['sid'];
	$result['files'] = ($data['files'] != 0);
	$result['directories'] = ($data['directories'] != 0);
	$result['type'] = $data['type'];
	$result['rule'] = fread($fdesc,$data['len']);
	return $result;
}

function unpack_delfilter($fdesc){
	return unpack("l1id",fread($fdesc,4))['id'];
}

function unpack_listdir($fdesc){
	return unpack("l1id",fread($fdesc,4))['id'];
}

function unpack_getfile($fdesc){
	$data = unpack("l1id/L2offset/L2blocksize",fread($fdesc,20));
	$id = $data['id'];
	$offset = $data['offset1'] + ($data['offset2'] << 32);
	$blocksize = $data['blocksize1'] + ($data['blocksize2'] << 32);
	$hash = fread($fdesc,16);
	return array($id,$offset,$blocksize,$hash);
}

function unpack_getoffset($fdesc){
	return unpack("l1id",fread($fdesc,4))['id'];
}

function unpack_putfile($fdesc){
	$result = array();
	$data = unpack("l1id/l1len",fread($fdesc,8));
	$result['id'] = $data['id'];
	$result['name'] = fread($fdesc,$data['len']);
	$data = unpack("L2ctime/L2mtime/L2size/C1is_dir/l1parent",fread($fdesc,29));
	$result['ctime'] = $data['ctime1'] + ($data['ctime2'] << 32);
	$result['mtime'] = $data['mtime1'] + ($data['mtime2'] << 32);
	$result['size'] = $data['size1'] + ($data['size2'] << 32);
	$result['is_dir'] = ($data['is_dir'] != 0);
	$result['parent'] = $data['parent'];
	$result['hash'] = fread($fdesc,16);
	$data = unpack("L2blocksize",fread($fdesc,8));
	$result['blocksize'] = $data['blocksize1'] + ($data['blocksize2'] << 32);
	return $result;
}

function unpack_addfile($fdesc){
	$data = unpack("l1id/L2offset/L2blocksize",fread($fdesc,20));
	$id = $data['id'];
	$offset = $data['offset1'] + ($data['offset2'] << 32);
	$blocksize = $data['blocksize1'] + ($data['blocksize2'] << 32);
	$hash = fread($fdesc,16);
	return array($id,$offset,$blocksize,$hash);
}

function unpack_patchfile($fdesc){
	$result = array();
	$data = unpack("l1id/l1len",fread($fdesc,8));
	$result['id'] = $data['id'];
	$result['name'] = fread($fdesc,$data['len']);
	$data = unpack("L2ctime/L2mtime/l1parent",fread($fdesc,20));
	$result['ctime'] = $data['ctime1'] + ($data['ctime2'] << 32);
	$result['mtime'] = $data['mtime1'] + ($data['mtime2'] << 32);
	$result['parent'] = $data['parent'];
	return $result;
}

function unpack_delfile($fdesc){
	$data = unpack("l1id/L2mtime",fread($fdesc,12));
	$id = $data['id'];
	$mtime = $data['mtime1'] + ($data['mtime2'] << 32);
	return array($id,$mtime);
}

function unpack_getmeta($fdesc){
	return unpack("l1id",fread($fdesc,4))['id'];
}

function unpack_purgefile($fdesc){
	return unpack("l1id",fread($fdesc,4))['id'];
}

function unpack_notifychange($fdesc){
	$l = array();
	while(!feof($fdesc)){
		$l[unpack("l1id",fread($fdesc,4))['id']] = fread($fdesc,16);
	}
	return $l;
}


function pack_code($code){
	return pack("l1",$code);
}

function pack_interror($msg){
	return pack("l1",MC_SRVSTAT_INTERROR).$msg;
}

function pack_exists($id){
	return pack("l2",MC_SRVSTAT_EXISTS,$id);
}

function pack_authed($token,$timestamp,$basedate){
	return pack("l1",MC_SRVSTAT_AUTHED).$token
		.pack("L2",$timestamp&0xFFFFFFFF,($timestamp&0xFFFFFFFF00000000)>>32)
		.pack("L2",$basedate&0xFFFFFFFF,($basedate&0xFFFFFFFF00000000)>>32)
		.pack("l1",MC_SERVER_PROTOCOL_VERSION);
}

function pack_synclist($list){
	$r = pack("l1",MC_SRVSTAT_SYNCLIST);
	foreach($list as $sync){
		if($sync[4] === NULL) $sync[4] = str_repeat("\0",16);
		$r .= pack("l2",$sync[0],strlen($sync[1])).$sync[1].
			pack("l1C1",$sync[2],$sync[3]).$sync[4];
	}
	return $r;
}

function pack_syncid($sid){
	return pack("l2",MC_SRVSTAT_SYNCID,$sid);
}

function pack_filterlist($list){
	$r = pack("l1",MC_SRVSTAT_FILTERLIST);
	foreach($list as $filter){
		$r .= pack("l2C2l2",$filter[0],$filter[1],$filter[2],$filter[3],$filter[4],strlen($filter[5])).$filter[5];
	}
	return $r;
}

function pack_filterid($fid){
	return pack("l2",MC_SRVSTAT_FILTERID,$fid);
}

function pack_dirlist($list){
	$r = pack("l1",MC_SRVSTAT_DIRLIST);
	foreach($list as $file){
		$r .= pack("l2",$file[0],strlen($file[1])).$file[1].
			pack("L2",$file[2]&0xFFFFFFFF,($file[2]&0xFFFFFFFF00000000)>>32).
			pack("L2",$file[3]&0xFFFFFFFF,($file[3]&0xFFFFFFFF00000000)>>32).
			pack("L2",$file[4]&0xFFFFFFFF,($file[4]&0xFFFFFFFF00000000)>>32).
			pack("Cll",$file[5],$file[6],$file[7]).
			$file[8];
	}
	return $r;
}

function pack_file($blocksize,$buf){
	return pack("l1L2",MC_SRVSTAT_FILE,$blocksize&0xFFFFFFFF,($blocksize&0xFFFFFFFF00000000)>>32).$buf;
}

function pack_fileid($fid){
	return pack("l2",MC_SRVSTAT_FILEID,$fid);
}

function pack_offset($offset){
	return pack("l1L2",MC_SRVSTAT_OFFSET,$offset&0xFFFFFFFF,($offset&0xFFFFFFFF00000000)>>32);
}

function pack_filemeta($file){
	$r = pack("l3",MC_SRVSTAT_FILEMETA,$file[0],strlen($file[1])).$file[1].
			pack("L2",$file[2]&0xFFFFFFFF,($file[2]&0xFFFFFFFF00000000)>>32).
			pack("L2",$file[3]&0xFFFFFFFF,($file[3]&0xFFFFFFFF00000000)>>32).
			pack("L2",$file[4]&0xFFFFFFFF,($file[4]&0xFFFFFFFF00000000)>>32).
			pack("Cll",$file[5],$file[6],$file[7]).
			$file[8];
	return $r;
}

function pack_change($sid){
	return pack("l2",MC_SRVSTAT_CHANGE,$sid);
}
?>
