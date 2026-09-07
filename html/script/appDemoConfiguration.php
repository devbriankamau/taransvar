<?php
ini_set('display_errors', '0');
error_reporting(E_ALL);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

$policyFile = '/var/lib/tarasec/demo-nodes.tsv';
$gatewayName = gethostname() ?: 'TaraSec gateway';
$nodes = [];

if (is_readable($policyFile)) {
    $lines = file($policyFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines ?: [] as $line) {
        $parts = explode("\t", $line, 3);
        $address = trim((string)($parts[0] ?? ''));
        if (filter_var($address, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false) {
            continue;
        }
        $ports = [];
        foreach (explode(',', (string)($parts[1] ?? '80,443')) as $port) {
            $value = filter_var(trim($port), FILTER_VALIDATE_INT, [
                'options' => ['min_range' => 1, 'max_range' => 65535]
            ]);
            if ($value !== false) {
                $ports[] = (int)$value;
            }
        }
        $nodes[] = [
            'address' => $address,
            'name' => trim((string)($parts[2] ?? '')),
            'ports' => array_values(array_unique($ports))
        ];
    }
}

echo json_encode([
    'ok' => true,
    'gateway' => $gatewayName,
    'nodes' => $nodes,
    'configured' => is_readable($policyFile),
    'server_time' => gmdate('c')
], JSON_UNESCAPED_SLASHES);
