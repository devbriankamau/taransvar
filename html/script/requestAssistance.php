<?php
print "Hi from requestAssistance..";
ini_set('display_errors','1');
ini_set('display_startup_errors','1');
error_reporting(E_ALL);
//config_update.php
//Takes report from partners... 
//config_update.php?ip=<hexip>:<port>&f=hack


//Put this directly into database and process later... e.g in 10 minutes when dhsp leases and conntrack is loaded.... 
//
include "../dbfunc.php";

function getSenderIp()
{
if (!empty($_SERVER['HTTP_CLIENT_IP'])) {
    $ip = $_SERVER['HTTP_CLIENT_IP'];
} elseif (!empty($_SERVER['HTTP_X_FORWARDED_FOR'])) {
    $ip = $_SERVER['HTTP_X_FORWARDED_FOR'];
} else {
    $ip = $_SERVER['REMOTE_ADDR'];
    return $ip;
 }
} 

/*function hex_to_ip($hex) {
    // Remove potential '0x' prefix
    $hex = str_replace('0x', '', $hex);

    // Convert the hex string to a binary string
    // 'H*' unpacks a hex string, 'a*' packs as a string
    $binary = pack('H*', $hex);

    // Convert the binary string to a human-readable IP address
    $ip = inet_ntop($binary);

    return $ip;
}*/

function hex_to_ip($hex) {
    $hex = preg_replace('/^0x/i', '', trim($hex));
    $hex = str_pad($hex, 8, '0', STR_PAD_LEFT);

    $binary = pack('H*', $hex);
    return inet_ntop($binary);
}

// Example for IPv4 (C0A80101 is 192.168.1.1)

$szFromIp = getSenderIp();
$nFromPort = $_SERVER['REMOTE_PORT'];
 
if (isset($_GET["f"]) && $_GET["f"]=="request")
{
        if (!isset($_GET["ip"]) || !isset($_GET["port"]) || strlen($_GET["ip"])<6) {
                echo "(missing params)";
                exit;
        }

        $ipv4_hex = $_GET["ip"];
        $aIp = hex_to_ip($ipv4_hex); // Output: IPv4: 192.168.1.1
        print "  Reported IP converted: ".$ipv4_hex." -> ".$aIp."<br>";

        if(!filter_var($aIp, FILTER_VALIDATE_IP)){
                echo '(invalid ip: '.$aIp.')';
                exit;
        }
        if(!filter_var($szFromIp, FILTER_VALIDATE_IP) || $szFromIp == '::1'){
                $szFromIp = '127.0.0.1';
        }
        $conn = getConnection();
        $szQual = intval($_GET["qual"]);
        $szSpoofed = intval($_GET["sp"]);
	//$sql = "insert into assistanceRequest (purpose, ip, port, senderIp, senderPort, category, requestQuality, wantSpoofed) values ('forDistribution', inet_aton('".$_GET["ip"]."'), ".$_GET["port"].", inet_aton('".$szFromIp."'), ".$nFromPort.",'".$_GET["cat"]."', $szQual, $szSpoofed)";
	$sql = "insert into assistanceRequest (purpose, ip, port, senderIp, senderPort, category, requestQuality, wantSpoofed) values ('forDistribution', inet_aton(?), ?, inet_aton(?), ?, ?, ?, ?)";
	print "<br>$sql<br>";
        $stmt = $conn->prepare($sql);
	$stmt->bind_param("sisisii", $aIp, $_GET["port"], $szFromIp, $nFromPort, $_GET["cat"], $szQual, $szSpoofed); //$_GET["ip"], 
	$stmt->execute();
	//$result = $conn->query($sql) or die("(error storing)");
	print "ok";
	exit;
}

print "(error in parameters)";

// print "<br>Your ip is: ".$ip.", port: ".$nPort."<br>";
?>


