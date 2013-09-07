<?php
require_once 'config.php';
require_once 'inc/funcs.php';
require_once 'inc/protocol.php';
require_once 'inc/protocol_funcs.php';
require_once 'inc/protocol_handlers.php';

header("Content-Type: application/octet-stream");
$ibuf = fopen("php://input",'rb');
$qrycode = unpack_code($ibuf);

$ready = true;
$mysqli = new mysqli("localhost", MC_DB_USER, MC_DB_PASS, MC_DB_DATABASE);
if($mysqli->connect_errno) $ready = false;
else {
	$q = $mysqli->query("SELECT basedate FROM mc_status");
	if(!$q) $ready = false;
	if($q->num_rows != 1) $ready = false;
	else $basedate = $q->fetch_row()[0];
}


if($ready){
	switch($qrycode){
		case MC_SRVQRY_STATUS:
			print(pack_code(MC_SRVSTAT_OK));
			break;
		case MC_SRVQRY_AUTH:
			$qry = unpack_auth($ibuf);
			if($qry[2] < MC_MIN_CLIENT_PROTOCOL_VERSION) print(pack_code(MC_SRVSTAT_INCOMPATIBLE));
			//Verify auth
			$q = $mysqli->query("SELECT id,password,token,lastseen,lastregen FROM mc_users WHERE name = '".esc($qry[0])."'");
			$res = $q->fetch_row();
			if($res){
				if(crypt($qry[1],$res[1]) == $res[1]){
					if((time()-$res[4] <= MC_TOKEN_REGEN) && (time()-$res[3] <= MC_SESS_EXPIRE)){
						$mysqli->query("UPDATE mc_users SET lastseen = ".time()." WHERE id = ".$res[0]);
						print(pack_authed($res[2],time(),$basedate,$res[0]));
					} else {
						$newtoken = md5("token".$res[0].time().$res[3].$res[2],true);
						$mysqli->query("UPDATE mc_users SET token = '".esc($newtoken)."', lastseen = ".time().", lastregen = ".time()." WHERE id = ".$res[0]);
						print(pack_authed($newtoken,time(),$basedate,$res[0]));
					}
				} else {
					print(pack_code(MC_SRVSTAT_UNAUTH));
				}
			} else {
				print(pack_code(MC_SRVSTAT_UNAUTH));
			}
			break;
		case MC_SRVQRY_LISTSYNCS:
		case MC_SRVQRY_CREATESYNC:
		case MC_SRVQRY_DELSYNC:
		case MC_SRVQRY_LISTFILTERS:
		case MC_SRVQRY_PUTFILTER:
		case MC_SRVQRY_DELFILTER:
		case MC_SRVQRY_LISTDIR:
		case MC_SRVQRY_GETFILE:
		case MC_SRVQRY_GETOFFSET:
		case MC_SRVQRY_PUTFILE:
		case MC_SRVQRY_ADDFILE:
		case MC_SRVQRY_PATCHFILE:
		case MC_SRVQRY_DELFILE:
		case MC_SRVQRY_GETMETA:
		case MC_SRVQRY_PURGEFILE:
		case MC_SRVQRY_NOTIFYCHANGE:
		case MC_SRVQRY_PASSCHANGE:
			try {
				$token = unpack_token($ibuf);
				$q = $mysqli->query("SELECT id FROM mc_users WHERE token = '".esc($token)."' AND lastseen >= ".(time()-MC_SESS_EXPIRE)." AND lastregen >= ".(time()-MC_TOKEN_REGEN));
				$uid = $q->fetch_row();
				if($uid){
					$uid = $uid[0];
					switch($qrycode){
						case MC_SRVQRY_LISTSYNCS:
							$res = handle_listsyncs($ibuf,$uid);
							break;
						case MC_SRVQRY_CREATESYNC:
							$res = handle_createsync($ibuf,$uid);
							break;
						case MC_SRVQRY_DELSYNC:
							$res = handle_delsync($ibuf,$uid);
							break;
						case MC_SRVQRY_LISTFILTERS:
							$res = handle_listfilters($ibuf,$uid);
							break;
						case MC_SRVQRY_PUTFILTER:
							$res = handle_putfilter($ibuf,$uid);
							break;
						case MC_SRVQRY_DELFILTER:
							$res = handle_delfilter($ibuf,$uid);
							break;
						case MC_SRVQRY_LISTDIR:
							$res = handle_listdir($ibuf,$uid);
							break;
						case MC_SRVQRY_GETFILE:
							$res = handle_getfile($ibuf,$uid);
							break;
						case MC_SRVQRY_GETOFFSET:
							$res = handle_getoffset($ibuf,$uid);
							break;
						case MC_SRVQRY_PUTFILE:
							$res = handle_putfile($ibuf,$uid);
							break;
						case MC_SRVQRY_ADDFILE:
							$res = handle_addfile($ibuf,$uid);
							break;
						case MC_SRVQRY_PATCHFILE:
							$res = handle_patchfile($ibuf,$uid);
							break;
						case MC_SRVQRY_DELFILE:
							$res = handle_delfile($ibuf,$uid);
							break;
						case MC_SRVQRY_GETMETA:
							$res = handle_getmeta($ibuf,$uid);
							break;
						case MC_SRVQRY_PURGEFILE:
							$res = handle_purgefile($ibuf,$uid);
							break;
						case MC_SRVQRY_NOTIFYCHANGE:
							$res = handle_notifychange($ibuf,$uid);
							break;
						case MC_SRVQRY_PASSCHANGE:
							$res = handle_passchange($ibuf,$uid);
							break;
						default:
							$res = pack_interror("Not Implemented");
					}
					$mysqli->query("UPDATE mc_users SET lastseen = ".time()." WHERE id = ".$uid);
					print($res);
				} else {
					print(pack_code(MC_SRVSTAT_UNAUTH));
				}
			} catch (Exception $e){
				print(pack_interror($e->getMessage()));
				#print(pack_code(MC_SRVSTAT_BADQRY)); //if we can't work with it its invalid
			}
			break;
		default:
			print(pack_code(MC_SRVSTAT_BADQRY));
			break;
		}
	$mysqli->close();
} else {
	print(pack_code(MC_SRVSTAT_NOTSETUP));
}
?>
