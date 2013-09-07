<?php
include 'inc/funcs.php';

if(isset($_GET['pw'])){
	print(generatehash($_GET['pw']));	
}
?>
