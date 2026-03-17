<?php
error_reporting( E_ALL );
ini_set('display_errors', '1');
ini_set('display_startup_errors', 1); 

$szLib = "XmlCommand.class.php";
if (file_exists($szLib))
	require_once $szLib;	//So that can test call the php file directly for debugging
else
	require_once("../".$szLib);	


function assistance()
{


	$conn = getConnection();
	$sql = "SELECT requestId, created, ip, inet_ntoa(ip) as aIp, port, senderIp, inet_ntoa(senderIp) as aSenderIp, senderPort, category, regardingRequestId, comment, handlingComment, senttime, requestQuality, CAST(wantSpoofed AS UNSIGNED) as wantSpoofed, CAST(active as UNSIGNED) as active, CAST(fromOther AS UNSIGNED) as fromOther, CAST(handled AS UNSIGNED) as handled, purpose from assistanceRequest order by requestId desc limit 50";
	$result = $conn->query($sql);

	if ($result->num_rows > 0) 
	{
		// output data of each row  
		$nCount=0;
		while($row = $result->fetch_assoc()) 
		{
	        $cArr = array($row["created"],$row["aIp"],$row["port"],$row["category"],$row["comment"],$row["requestQuality"]); 
    	    $szRowId = "ar".$row["requestId"];
        	CXmlCommand::addTableRow("assistanceTbl", "top", $szRowId, $cArr, "", $szRowId);//$szHTML)
			$nCount++;
	  	}
	} 
	else 
	{
	  //echo "<br>";
	}
	$conn->close();

}


?>
