<?php
session_start();
error_reporting( E_ALL );
ini_set('display_errors', '1');

function getVisitorIP() {
    if (!empty($_SERVER['HTTP_CLIENT_IP'])) {
        return $_SERVER['HTTP_CLIENT_IP'];
    } elseif (!empty($_SERVER['HTTP_X_FORWARDED_FOR'])) {
        // Can contain multiple IPs: client, proxy1, proxy2
        $ips = explode(',', $_SERVER['HTTP_X_FORWARDED_FOR']);
        return trim($ips[0]);
    } else {
        return $_SERVER['REMOTE_ADDR'];
    }
}

function listTrafficFrom($szIp, $port)
{
        //NOTE! THE NEW FUNCTION 
//        print "IP: $szIp";
        //$szSQL = "select whoIsId, created, count, tag from traffic where ipFrom = inet_aton(?) and portFrom = ? limit 10";
        $szSQL = "select inet_ntoa(ipFrom) as aIpFrom, inet_ntoa(ipTo) as aIpTo, whoIsId, created, count, tag from traffic where ipFrom = inet_aton(?) limit 10";
        //$szSQL = "select inet_ntoa(ipFrom) as aIpFrom, inet_ntoa(ipTo) as aIpTo, whoIsId, created, count, tag from traffic order by trafficId desc limit 10";

        $conn = getConnection();
    	$stmt = $conn->prepare($szSQL);
        //$stmt->bind_param("si", $szIp, $port); 
        $stmt->bind_param("s", $szIp); 
        $stmt->execute();
    	$result = $stmt->get_result(); // get the mysqli result
        $nCount = 0;

	    while ($result && $row = $result->fetch_assoc())
	    {
            if (!$nCount)
                print "<table><tr><th>Time</th><th>Tilkobling</th><th>Count</th><th>Tag</th></tr>";

            $nCount++;
                print '<tr><td align="left">'.$row["aIpFrom"].'</td><td>'.$row["created"].'</td><td>'.$row["whoIsId"].'</td><td>'.$row["count"].'</td><td>'.$row["tag"].'</td></tr>';
        }

        if ($nCount)
            print "</table>";
        else
            print "No traffic found.. Wait a few seconds and refresh.. Or maybe your request is being blocked?..";

}


function guestInfo()
{
    $szIp = getVisitorIP();

    $port = $_SERVER['REMOTE_PORT'];
    //$szIp = "10.10.10.10";

    if ($szIp == "::1")
    {
        print "<br><br>You are using this computer. Connect a unit through wifi (if available) or visit the samplebank of other router to get more valuable info.";
        return;
    }

    print "<table>";
    print "<tr><td>Public IP address</td><td>".$szIp."</td></tr>";
    $szDbFuncFile = "../gatekeeper/dbfunc.php";

    //if (!file_exists($szDbFuncFile))

    include_once $szDbFuncFile;
    $conn = getConnection();
    $szSQL = "select P.name from partner P join partnerRouter PR on PR.partnerId = P.partnerId where PR.ip = inet_aton(?)";

    $stmt = $conn->prepare($szSQL);
    $stmt->bind_param("s", $szIp); 
    $stmt->execute();
	$result = $stmt->get_result(); // get the mysqli result

	if ($result && $row = $result->fetch_assoc())
    {
        print "<tr><td>Port number (HTTP)</td><td>".$port."</td></tr>";
        print '<tr><td>You\'re connected to</td><td>'.$row["name"].'</td></tr>';
    }
    else
        print '<tr><td colspan="2">Don\'t know where you\'re connected.</td></tr>';

    print '<tr><td colspan="2">Latest traffic from you:</td></tr>';

    print '<tr><td colspan="2">';
    listTrafficFrom($szIp, $port);
    print "</td></tr>";
    print "</table>";
}

?>