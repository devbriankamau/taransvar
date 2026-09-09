<?php
ini_set('display_errors', '0');
error_reporting(E_ALL);

include '../dbfunc.php';

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

$configFile = '/etc/tarasecfw.conf';
$gatewayName = gethostname() ?: 'TaraSec gateway';
$values = [];

if (is_readable($configFile)) {
    foreach (file($configFile, FILE_IGNORE_NEW_LINES) ?: [] as $line) {
        if (preg_match('/^\s*(DEMO_NODE|DEMO_NODES|DEMO_NODE_NAMES|HOTSPOT_ALLOWED_NETBIRD_NODES|HOTSPOT_ALLOWED_NETBIRD_TCP_PORTS)\s*=\s*["\']?([^"\']*)["\']?\s*$/', $line, $match)) {
            $values[$match[1]] = trim($match[2]);
        }
    }
}

$addresses = array_values(array_filter(array_map('trim', explode(
    ',',
    $values['DEMO_NODES'] ?? $values['HOTSPOT_ALLOWED_NETBIRD_NODES'] ?? ''
))));
$names = array_map('trim', explode(',', $values['DEMO_NODE_NAMES'] ?? ''));
$ports = [];
foreach (explode(',', $values['HOTSPOT_ALLOWED_NETBIRD_TCP_PORTS'] ?? '80,443') as $port) {
    $value = filter_var(trim($port), FILTER_VALIDATE_INT, [
        'options' => ['min_range' => 1, 'max_range' => 65535]
    ]);
    if ($value !== false) {
        $ports[] = (int)$value;
    }
}

$nodes = [];
foreach ($addresses as $index => $address) {
    if (filter_var($address, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false) {
        continue;
    }
    $nodes[] = [
        'address' => $address,
        'name' => $names[$index] ?? '',
        'ports' => array_values(array_unique($ports))
    ];
}

$demoSetups = [];
try {
    $conn = getConnection();
    $result = $conn->query("SELECT s.demoSshSetupId,s.name,INET_NTOA(s.nodeAIp) node_a,s.nodeAPort,INET_NTOA(n.ip) node_b,n.port nodeBPort,s.challengeTtlSeconds FROM demoSshSetup s JOIN demoSshNodeB n ON n.demoSshNodeBId=s.demoSshNodeBId WHERE s.active=b'1' AND n.active=b'1' ORDER BY s.name");
    while ($row = $result->fetch_assoc()) {
        $demoSetups[] = [
            'id' => (int)$row['demoSshSetupId'],
            'name' => (string)$row['name'],
            'node_a' => (string)$row['node_a'],
            'node_a_port' => (int)$row['nodeAPort'],
            'node_b' => (string)$row['node_b'],
            'node_b_port' => (int)$row['nodeBPort'],
            'expires_in' => (int)$row['challengeTtlSeconds']
        ];
    }
    $conn->close();
} catch (Throwable $e) {
    // Keep older/pre-migration gateways useful for the existing demo UI.
    error_log('Unable to load demoSshSetup: ' . $e->getMessage());
}

echo json_encode([
    'ok' => true,
    'gateway' => $gatewayName,
    'nodes' => $nodes,
    'demo_ssh_setups' => $demoSetups,
    'demo_node' => in_array(strtolower($values['DEMO_NODE'] ?? '0'), ['1', 'yes', 'true', 'on'], true),
    'configured' => is_readable($configFile),
    'server_time' => gmdate('c')
], JSON_UNESCAPED_SLASHES);
