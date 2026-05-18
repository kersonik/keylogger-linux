<?php
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $data = file_get_contents('php://input');
    $timestamp = date("Y-m-d H:i:s");
    $ip = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    $x_id = $_SERVER['HTTP_X_ID'] ?? 'unknown';
    $username = $x_id;
    $last_dash = strrpos($x_id, '-');
    if ($last_dash !== false) $username = substr($x_id, $last_dash + 1);
    $filename = "{$username}@{$ip}-{$timestamp}";
    $logDir = __DIR__ . '/logs';
    $logFile = $logDir . '/' . $filename;
    if (!is_dir($logDir}) @mkdir($logDir, 0755, true);
    if (file_put_contents($logFile, $data, FILE_APPEND) !== false) echo "OK";
    else { header('Content-Type: text/plain'); echo "ERR"; }
}
?>
