<?php
//partnerscan.php
error_reporting( E_ALL );
ini_set('display_errors', '1');
ini_set('display_startup_errors', 1); 

include "dbfunc.php";
include "gate_lib.php";

if (isset($_GET["res"]))
{
    //This is the list of partner routers sent from Global DB server
    //Delete already registered DB servers
    $conn = getConnection();
    $szSQL = "delete from partnerRouter";     
    $stmt = $conn->prepare($szSQL);
	$stmt->execute();
    $szSQL = "delete from partner";     
    $stmt = $conn->prepare($szSQL);
	$stmt->execute();

    //Set sender as global DB Server
    $server_ip = $_SERVER['REMOTE_ADDR'];   
    $szSQL = "update setup set globalDb1ip = inet_aton(?)";     
    $stmt = $conn->prepare($szSQL);
    $stmt->bind_param("s", $server_ip); 
	$stmt->execute();
    print "Global DB Server set to $server_ip<br>";

    $cPartners = json_decode($_GET["res"]);
    print "<table>";
    $szMyIp = lan_ipv4();

    foreach( $cPartners as $cPartner)
    {
        if ($cPartner->ip == $szMyIp)
            print "<tr><td>[Skipping myself]</td><td>$cPartner->ip</td></tr>";
        else
        {
            print "<tr><td>$cPartner->name</td><td>$cPartner->ip</td></tr>";
            savePartner($cPartner->name, $cPartner->ip);
        }
    }

    print "</table>";

    print "Supposed to store the partner list..";
    return;
}

$cArr = array("dbServer" => 0, "name" => gethostname());

print json_encode($cArr);
?>
