<?php

require_once 'funcs.php';
require_once 'const.php';

function rscan2($uid,$sid,$parentid,$path){
	global $mysqli;
	$q = $mysqli->query("SELECT id,name,is_dir,hash FROM mc_files WHERE parent = ".$parentid);
	if(!$q){ echo "Error: ".$mysqli->error; return; }
	$db = $q->fetch_all();
	$fs = scandir($path);
	if(count($db)>0 || count($fs)-2 == 0){ //Been there
		if(count($fs)-2 != count($db)){ //Aborted here
			//Clear inconsistent results and start over
			$q2 = $mysqli->query("DELETE FROM mc_files WHERE parent = ".$parentid);
			if(!$q2){ echo "Error: ".$mysqli->error; return; }
			return rscan2($uid,$sid,$parentid,$path);
		}else { //All known, add/check hashes
			foreach($db as $d){
				if($d[3] === NULL){
					if($d[2]){
						 rscan2($uid,$sid,$d[0],$path.'/'.$d[1]);
						$h = directoryHash($d[0]);
					} else {
						$h = md5_file($path.'/'.$d[1],true);
					}
					$q2 = $mysqli->query("UPDATE mc_files SET hash = '".esc($h)."' WHERE id = ".$d[0]);
					if(!$q2){ echo "Error: ".$mysqli->error; break; }
					echo "hash: ".$path.'/'.$d[1]."\n";
				}
			}
		}		
	} else { //Not scanned yet
		//First add all, then add hashes
		foreach($fs as $f){
			if($f == "." || $f == "..") continue;
			$info = stat($path.'/'.$f);
			$is_dir = ($info['mode'] & 0170000) == 040000;		/* no such thing as ctime on unix*/
			$q = $mysqli->query("INSERT INTO mc_files (uid,sid,name,ctime,mtime,size,is_dir,parent,status) VALUES ".				/* Assumption - can only hope */
			"(".$uid.", ".$sid.", '".esc($f)."', ".$info['mtime'].", ".$info['mtime'].", ".($is_dir?0:$info['size']).", ".($is_dir?1:0).", ".$parentid.", ".MC_FILESTAT_COMPLETE.")");
			if(!$q){ echo "Error: ".$mysqli->error; break; }
		}
		echo "scan:".$path."\n";
		return rscan2($uid,$sid,$parentid,$path);
	}
}

function scan2($path){
	global $mysqli;
	$q = $mysqli->query("SELECT s.id,s.uid,s.name,u.name FROM (mc_syncs AS s INNER JOIN mc_users AS u ON s.uid = u.id) WHERE s.hash IS NULL");
	if(!$q){ echo "Error: ".$mysqli->error; return; }
	$syncs = $q->fetch_all();
	foreach($syncs as $sync){
		rscan2($sync[1],$sync[0],-$sync[0],$path.'/'.$sync[3].'/'.$sync[2]);
		$h = directoryHash(-$sync[0]);
		$q2 = $mysqli->query("UPDATE mc_syncs SET hash = '".esc($h)."' WHERE id = ".$sync[0]);
		if(!$q2){ echo "Error: ".$mysqli->error; break; }
	}
}

//Create syncs from directories
function sscan($uid,$path){
	global $mysqli;
	$l = scandir($path);
	foreach($l as $s){
		if(is_dir($path.'/'.$s) && $s != "." && $s != ".."){
			$cr = file_exists($path.'/'.$s.".crypted");
			echo "INSERT INTO mc_syncs (uid,name,filterversion,shareversion,crypted) VALUES ('".$uid."','".esc($s)."',1,1,".($cr?1:0).")"."\n";
			$q = $mysqli->query("INSERT INTO mc_syncs (uid,name,filterversion,shareversion,crypted) VALUES ('".$uid."','".esc($s)."',1,1,".($cr?1:0).")");
			if(!$q){ echo "Error: ".$mysqli->error; break; }
			echo ($cr?"Encrypted ":"")."Sync added: ".$s." (".$mysqli->insert_id.")\n";
			//fscan($uid,-$mysqli->insert_id,$path.'/'.$s);
		}
	}
}
//Create users from directories
function uscan($path){
	global $mysqli;
	$l = scandir($path);
	foreach($l as $u){
		if(is_dir($path.'/'.$u) && $u != "." && $u != ".."){
			$pwplain = md5(mt_rand(0,32).time());
			$pw = generatehash($pwplain);
			echo "INSERT INTO mc_users (name,password,lastseen,lastregen) VALUES ('".esc($u)."','".$pw."',0,0)"."\n"; 
			$q = $mysqli->query("INSERT INTO mc_users (name,password,lastseen,lastregen) VALUES ('".esc($u)."','".$pw."',0,0)");
			if(!$q){ echo "Error: ".$mysqli->error; break; }
			echo "User added: ".$u.":".$pwplain." (".$mysqli->insert_id.")\n";
			sscan($mysqli->insert_id,$path.'/'.$u);
		}
	}
}

?>
