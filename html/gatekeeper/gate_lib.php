<?php
//gate_lib.php
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

function savePartner($szNname, $szIp)
{
    //$szMsg .= "$current_timestamp - trying: $url - $szReply<br>";

    $conn = getConnection();

    $szSQL = "insert into partner (name) values (?)";     
    $stmt = $conn->prepare($szSQL);
    $stmt->bind_param("s", $cReply->name); 
    $stmt->execute();

    $newId = $conn->insert_id;

    $szSQL = "insert into partnerRouter (partnerId, ip, nettmask) values (?, inet_aton(?), inet_aton('255.255.255.255'))";     
    $stmt = $conn->prepare($szSQL);
    $stmt->bind_param("is", $newId, $szTryIp); 
    $stmt->execute();
}


?>