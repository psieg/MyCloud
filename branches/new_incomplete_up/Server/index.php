<?php
include 'inc/const.php';
include 'config.php';


$ready = true; //TODO: Check wether db is ready
$mysqli = new mysqli("localhost", MC_DB_USER, MC_DB_PASS, MC_DB_DATABASE);
if($mysqli->connect_errno) $ready = false;
else {
//	$q = $mysqli->query("SELECT basedate FROM mc_status");
//	if(!$q) $ready = false;
}
echo "MyCloudServer ".MC_VERSION." (Protocol ".MC_SERVER_PROTOCOL_VERSION.")\n";

if($ready){
	echo "db works\n";

//	include 'inc/funcs.php';

//	for($i = 3801; $i < 4406; $i++){
//		updateHash(1913);
//		echo $i;
//		if($i % 100 == 0){
//			echo "\n";
//			ob_flush();
//		}
//	}


	if(isset($_GET['reset'])){
		echo "resetting\n";
		$unsetup_confirm = true;
		include 'inc/unsetup.php';
		echo "<a href=\"?phase1\">click to setup new</a>";	
	} else {
		include 'inc/setup.php';
	}
} else {
	echo "Failed to open Database. Check config.php";
}
?>
