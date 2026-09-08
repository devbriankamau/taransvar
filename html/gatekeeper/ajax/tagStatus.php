<?php

/*
 * Live Gatekeeper tag status.
 *
 * Keep assessment logic in taraLib.php::getTagData(). The previous version
 * repeated the infection, hack-report and traffic queries here, with different
 * freshness and precedence rules from the app.
 */
function tagStatus()
{
	$data = getTagData();

	/*
	 * The AJAX request itself is the traffic being assessed. tarakernel sends
	 * queued traffic within one second, so briefly wait for an observation from
	 * the current second instead of displaying the preceding poll's state.
	 */
	$deadline = microtime(true) + 1.5;
	while ((int)($data["trafficSecondsSince"] ?? -1) !== 0 &&
		microtime(true) < $deadline) {
		usleep(100000);
		$data = getTagData();
	}

	$senderIp = htmlspecialchars(getSenderIp(), ENT_QUOTES, 'UTF-8');
	$severity = (int)($data["severity"] ?? 0);
	$trafficAge = (int)($data["trafficSecondsSince"] ?? -1);
	$hackAge = (int)($data["hackReportSecondsSince"] ?? -1);
	$infectionSeverity = (int)($data["infectionSeverity"] ?? -1);
	$infectionDisabled = (int)($data["infectionDisabled"] ?? 0);

	if ($trafficAge >= 0 && $trafficAge < 45) {
		$source = "current traffic";
		$age = $trafficAge;
	} elseif ($hackAge >= 0) {
		$source = "hack report";
		$age = $hackAge;
	} elseif ($infectionSeverity >= 0) {
		$source = "local infection record";
		$age = -1;
	} else {
		$source = "no threat evidence";
		$age = -1;
	}

	if ($severity > 1) {
		$status = '<font color="red">YOU ARE TAGGED</font><br>Severity: '.$severity;
	} elseif ($severity === 1) {
		$status = '<font color="green">You are clean</font><br>(restricted tag: SSH may be unavailable)';
	} else {
		$status = '<font color="green">You are clean</font>';
	}

	if ($infectionDisabled) {
		$status .= '<br>Local infection record is disabled.';
	}

	$status .= '<br>Evidence: '.htmlspecialchars($source, ENT_QUOTES, 'UTF-8');
	if ($age >= 0) {
		$status .= ' · '.$age.' sec ago';
	}
	$status .= '<br>IP: '.$senderIp;

	CXmlCommand::setInnerHTML("tagStatus", "", $status);
}

?>
