
<script>
var szUpdateRoutine = "log";	
</script>
<?php 

function listLog()
{//asdf
	$conn = getConnection();
	$szSQL = "select inet_ntoa(adminIp) as ip, dmesg, unix_timestamp(now())-unix_timestamp(dmesgUpdated) as secsAgo from setup";
	$result = $conn->query($szSQL);
	$row = 0;

	if ($result && $result->num_rows > 0) 
	{
		if ($row = $result->fetch_assoc()) 
		{
			if (!isset($row["secsAgo"]) || (int)$row["secsAgo"] > 20)
				print "<b><font color=\"red\">This content is supposed to be updated every 10 seconds but misc/crontasks.pl seems not to be set up properly</font></b>";
			else
				print "<b>NOTE! This contents was updated ".$row["secsAgo"]." seconds ago</b>. For updated content, ssh ".$row["ip"]." and run sudo dmesg -w</b>";

			$lines = explode("\n", ($row["dmesg"]?$row["dmesg"]:""));
			$lines = array_reverse($lines);
			$text_reversed = implode("\n", $lines);
			$replaced = str_replace("\n","<br>",$text_reversed);

			$ip = getSenderIp();
			$replaced = str_replace($ip,"<b><font color=\"red\">".$ip."</font></b>",$replaced);
			
			print '<table id="logTbl"><tr><td id="lg1"><p><div id="logHere" align="left">'.$replaced."</div></p></td></tr></table>";
		}
	}
	if (!$row)
		print "Setup not found!";

	/*$sql = "select lineId, batchId, timeSearch, theTime, theText from dmesgLine order by lineId desc limit 100";
	
	$result = $conn->query($sql);

	if ($result && $result->num_rows > 0) 
	{
		// output data of each row  
		print "<h2>Log:</h2><table><tr><td>Id</td><td>Time, Text</td></tr>";
		$nCount=0;
		while($row = $result->fetch_assoc()) 
		{
	    		print "<tr><td>".$row["lineId"]."</td><td>".$row["theTime"]."</td><td>".$row["theText"]."</td></tr>";
			$nCount++;
	  	}
		print "</table>";
	} 
	else 
	{
	  echo "No log entries<br>";
	}
	$conn->close();
	//print 'Supposed to list servers';
	print '<br><a href="index.php?f=addpartner">Add partner</a>';*/
}

?>
