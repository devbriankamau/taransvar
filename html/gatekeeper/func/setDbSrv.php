<?php
//setDbSrv
require_once "gate_lib.php";

function setDbSrv()
{
    $szIP = lan_ipv4();
    print "should set $szIP as global DB server";

    $conn = getConnection();
    $szSQL = "update setup set globalDb1ip = inet_aton(?), adminIP = inet_aton(?)";     
    $stmt = $conn->prepare($szSQL);
   	$stmt->bind_param("ss", $szIP, $szIP); 
	$stmt->execute();

}

?>
