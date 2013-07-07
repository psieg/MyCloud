<?php
if(isset($unsetup_confirm)){
	$mysqli->query("DROP TABLE IF EXISTS mc_status");
	$mysqli->query("DROP TABLE IF EXISTS mc_users");
	$mysqli->query("DROP TABLE IF EXISTS mc_syncs");
	$mysqli->query("DROP TABLE IF EXISTS mc_files");
	$mysqli->query("DROP TABLE IF EXISTS mc_filters");
	#exec("rm -r data/user/Cloud/*");
} else {
	echo "NOT unsetup";
}
?>
