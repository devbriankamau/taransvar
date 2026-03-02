<?php
//partnerscan.php
error_reporting( E_ALL );
ini_set('display_errors', '1');
ini_set('display_startup_errors', 1); 

include "dbfunc.php";
include "gate_lib.php";

if (isset($_GET["res"]))
{
    $cPartners = json_decode($_GET["res"]);
    print "<table>";

    foreach( $cPartners as $cPartner)
    {
        print "<tr><td>$cPartner->name</td><td>$cPartner->ip</td></tr>";
        savePartner($cPartner->name, $cPartner->ip);
    }

    print "</table>";

    print "Supposed to store the partner list..";
    return;
}

$cArr = array("dbServer" => 0, "name" => gethostname());

print json_encode($cArr);
?>
