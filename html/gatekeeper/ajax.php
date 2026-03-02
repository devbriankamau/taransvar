<?php
session_start();

error_reporting( E_ALL );
ini_set('display_errors', '1');
ini_set('display_startup_errors', 1); 

include "genlib.php";
include "Basic.class.php";
include "System.class.php";
include "dbfunc.php";

if (!function_exists("isAjax"))
{
    function isAjax() {return true;}
}


function myId() 
{
    $nMyId = (isset($_SESSION['sess_adm_userid'])?$_SESSION['sess_adm_userid']:0)+0;

    if (!$nMyId)
    {

        if (isset($_SESSION["user"]))
        {
            $szUsername = $_SESSION["user"];

            if (strlen($szUsername))
            {
                $cFlds = array(":uname" => $szUsername);
                $nMyId = CDb::getString("select UserId from Users where Username = :uname", $cFlds);
            }
        }
    }


    return $nMyId;
}


if (!isset($pSystem))   //200327
    $pSystem = new CSystem;

if (!function_exists("getSystem"))
{
    function getSystem()
    {   
	    global $pGlobalSystem;
	    if (!$pGlobalSystem)
		    $pGlobalSystem = new CSystem;
	    return $pGlobalSystem; 
    }
}


include "XmlCommand.class.php";
include "gate_lib.php";

function partnerScan()
{
    //Turn off error reporting to avoid timeout message as 404
 //   error_reporting(0);
 //   ini_set('display_errors', '0');
 //   ini_set('display_startup_errors', 0); 

    //print "Doing partner scan";
    //CXmlCommand::alert("Doing it now..");
    $szMsg = "";

    //$server_ip = $_SERVER['SERVER_ADDR'];
    //$szMsg =  "The IP address of the server is: " . $server_ip."<br>";
    //$ipv4 = gethostbyname(gethostname());
    //$szMsg .= "IPv4: ".$ipv4;
    $szIpv4 = lan_ipv4();

    $ipv4 = ($szIpv4 ?? 'No LAN IPv4 found');
    $szMsg .= "My IP address: ".$szIpv4."<br>";

    $nLastDot = strrpos($szIpv4, ".", );
    if (!$nLastDot)
    {
        CXmlCommand::setInnerHTML("scanresult", "", "Unable to find IP address: ".$szIpv4);  //, $cMoreParamsArr = array()
        return;
    }

    //Clear the parter register
    $conn = getConnection();
    $szSQL = "delete from partnerRouter";     
    $stmt = $conn->prepare($szSQL);
	$stmt->execute();
    $szSQL = "delete from partner";     
    $stmt = $conn->prepare($szSQL);
	$stmt->execute();

    //Scan all IP addresses in same range and send message..
    $nStart= 2;
    $nStart = 15;
    $nEnd = 20;
    $cReplies = array();
    for ($n = $nStart; $n<=$nEnd; $n++)
    {

        $szTryIp = substr($szIpv4, 0, $nLastDot).".".$n;

        if ($szIpv4 == $szTryIp)
        {
            $szMsg .= "$current_timestamp - $szIpv4 - Skipping this computer<br>";
        }
        else
        {
            $url = "http://".$szTryIp."/gatekeeper/partnerscan.php";

            //$szReply = file_get_contents($url);
            $szReply = safe_file_get_contents($url);

            $current_timestamp = time();

            $context = stream_context_create([
                    'http' => [
                    'timeout' => 1,   // seconds
                    ]
                ]);

            $szReply = @file_get_contents($url, false, $context);
            //$szMsg .= print $szReply;

            if ($szReply !== false) {
                $szMsg .= "$current_timestamp - $url - $szReply<br>";
                $cReply = json_decode($szReply, 1);

                if (!$cReply)
                    $szMsg .= "Unable to decode reply: $szReply<br>";
                else
                {
                    $cReply["ip"] = $szTryIp;
                    savePartner($cReply["name"], $cReply["ip"]);

                    //Store a list of partners and send the full list to all after completion
                    $cReplies[] = $cReply;
                }

            }
            //else
            //    $szReply = "Request failed or timed out";
        }

        //$szMsg .= "$current_timestamp - trying: $szTryIp - $szReply<br>";
    }

    $szMsg .= "<br>Partner list: ".json_encode($cReplies);

    foreach ($cReplies as $cReply)
    {
        $url = "http://".$cReply["ip"]."/gatekeeper/partnerscan.php?res=".urlencode(json_encode($cReplies));
        $szMsg .= "Sending ".$url."<br>";
    }

    CXmlCommand::setInnerHTML("scanresult", "", $szMsg);  //, $cMoreParamsArr = array()
}


switch ($_GET["func"])
{
    case "partnerscan":
        partnerScan();
        break;
    default:
        print "unknown function";

}

    CXmlCommand::flushXml();

?>
