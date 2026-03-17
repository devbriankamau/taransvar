<script>
var szUpdateRoutine = "assistance";	
</script>
<?php

function assistance()
{

	// output data of each row  
	print '<h2>Assistance Requests:</h2><table id="assistanceTbl">
		<tr><th colspan=\"2\">Id</td><th colspan=\"2\">Created</td><td>IP</td><td>Port</td></tr>';
		print "</table>";
}

print '<a href="index.php?f=addassreq">Request assistance from partnert (against DDos attack)</a>';

?>
