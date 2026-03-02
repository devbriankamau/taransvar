<?php

function listrouters()
{
	$conn=getConnection();
	$szSQL = "select name, inet_ntoa(ip) as ip, partnerStatusReceived from partnerRouter R join partner P on P.partnerId = R.partnerId";
	//print "<br>$szSQL<br>";
	$conn->query($szSQL) or die(mysql_error());
	$result = $conn->query($szSQL);

        if ($result->num_rows > 0) 
        {
        	// output data of each row  
        	print "<h2>All registered routers:</h2><table>";
	        $nCount=0;
	        while($row = $result->fetch_assoc()) 
	        {
	        	if (!$nCount)
	        		print "<tr><td>Partner</td><td>IP</td><td>Status received</td></tr>";

	            print "<tr><td>".$row["name"]."</td><td>".$row["ip"]."</td><td>".$row["partnerStatusReceived"]."</td></tr>";
	            $nCount++;
	        }
	        print "</table>";
        }
}

?>
