<?php
ini_set('display_errors', '0');
error_reporting(E_ALL);

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

echo json_encode([
    'ok' => true,
    'gateway' => $gatewayName,
    'nodes' => $nodes,
    'demo_node' => in_array(strtolower($values['DEMO_NODE'] ?? '0'), ['1', 'yes', 'true', 'on'], true),
    'configured' => is_readable($configFile),
    'server_time' => gmdate('c')
], JSON_UNESCAPED_SLASHES);
