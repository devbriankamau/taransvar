<?php
session_start();

    error_reporting( E_ALL );
    ini_set('display_errors', '1');
    ini_set('display_startup_errors', 1); 


function defaultErrorHandling()
{
}

defaultErrorHandling();

include "genlib.php";
include "Basic.class.php";
include "System.class.php";

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

function lan_ipv4(): ?string
{
    $cmd = "ip -4 route get 1.1.1.1 2>/dev/null";
    $output = shell_exec($cmd);

    if (!$output) {
        return null;
    }

    if (preg_match('/src\s+([0-9.]+)/', $output, $matches)) {
        return filter_var($matches[1], FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)
            ? $matches[1]
            : null;
    }

    return null;
}

function safe_file_get_contents($url, $timeout = 3)
{
    $context = stream_context_create([
        'http' => ['timeout' => $timeout]
    ]);

    set_error_handler(function() {
        throw new Exception("Connection failed");
    });

    try {
        $data = file_get_contents($url, false, $context);
        restore_error_handler();
        return $data;
    } catch (Exception $e) {
        restore_error_handler();
        return false;
    }
}

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

    //Scan all IP addresses in same range and send message..
    for ($n = 2; $n<6; $n++)
    {
            $szTryIp = substr($szIpv4, 0, $nLastDot).".".$n;
            $url = "http://".$szTryIp."/gatekeeper/partnerscan.php";

            //$szReply = file_get_contents($url);
            $szReply = safe_file_get_contents($url);

            $current_timestamp = time();

            $context = stream_context_create([
                    'http' => [
                    'timeout' => 2,   // seconds
                    ]
                ]);

            $szReply = @file_get_contents($url, false, $context);

            if ($szReply === false) {
                $szReply = "Request failed or timed out";
            }

            $szMsg .= "$current_timestamp - trying: $szTryIp - $szReply<br>";
    }

    defaultErrorHandling(); //Turn error handling back to normal...

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
