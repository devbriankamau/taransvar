<?php
//To be include by both www (func/listInfections.php) and ajax (ajax.php) function

function getActivateInfectionsLinks($row)
{
	switch ($row["active"])
	{
		case "1":
			$szAction = "deactivate";
			$szExtraAction = '';
			break;
		case "0":
			$szAction = "activate";
			$szExtraAction = '<a href="index.php?f=delInfection&action=delete&id='.$row["infectionId"].'">[delete]</a>';
			break;
        default:
            $szAction = $szExtraAction = "ERROR (unknown active)";
            break;
	}

	return '<a href="index.php?f=delInfection&action='.$szAction.'&id='.$row["infectionId"].'">['.$szAction.']</a>'.$szExtraAction.'</td>';
}

 
?>