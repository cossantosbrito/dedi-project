<?php
/**
 * Worker MQTT -> MySQL.
 *
 * Jalankan di terminal dulu untuk mengujinya:
 *     php mqtt_ingest.php
 *
 * Di server, pasang sebagai layanan systemd (lihat SERVER.md).
 * Proses ini berjalan terus-menerus dan tidak boleh dipanggil lewat web.
 */

if (PHP_SAPI !== 'cli') {
    http_response_code(403);
    exit("Skrip ini hanya untuk baris perintah.\n");
}

require_once __DIR__ . '/vendor/autoload.php';
require_once __DIR__ . '/ingest_lib.php';

use PhpMqtt\Client\ConnectionSettings;
use PhpMqtt\Client\MqttClient;

$running = true;

// ---------------------------------------------------------------------
// Loop utama, dengan sambung ulang kalau broker putus

if (function_exists('pcntl_async_signals')) {
    pcntl_async_signals(true);
}

$host  = conf('MQTT_HOST', MQTT_HOST);
$port  = (int) conf('MQTT_PORT', MQTT_PORT);
$topic = conf('MQTT_TOPIC', MQTT_TOPIC);

say("Worker mulai. Broker {$host}:{$port}, topic {$topic}");

while ($running) {

    $clientId = 'php-ingest-' . substr(md5((string) getmypid() . microtime()), 0, 8);
    $mqtt     = new MqttClient($host, $port, $clientId, MqttClient::MQTT_3_1_1);

    if (function_exists('pcntl_signal')) {
        $stop = function () use ($mqtt, &$running) {
            $running = false;
            $mqtt->interrupt();
        };
        pcntl_signal(SIGINT, $stop);
        pcntl_signal(SIGTERM, $stop);
    }

    $settings = (new ConnectionSettings())
        ->setUsername(conf('MQTT_USER', MQTT_USER) ?: null)
        ->setPassword(conf('MQTT_PASS', MQTT_PASS) ?: null)
        ->setConnectTimeout(10)
        ->setKeepAliveInterval(45)
        ->setUseTls((bool) MQTT_TLS)
        ->setTlsVerifyPeer(true);

    try {
        $mqtt->connect($settings, true);
        say('Terhubung ke broker.');

        $mqtt->subscribe($topic, function (string $t, string $message) {
            handleMessage($t, $message);
        }, 1);

        say('Berlangganan ' . $topic
            . '. Menyimpan paling cepat setiap '
            . conf('MQTT_MIN_INTERVAL', MQTT_MIN_INTERVAL) . ' detik per alat.');

        $mqtt->loop(true);          // memblokir sampai interrupt() atau error
        $mqtt->disconnect();

    } catch (Throwable $e) {
        say('Koneksi bermasalah: ' . $e->getMessage());

        if ($running) {
            say('Mencoba lagi 10 detik lagi...');
            sleep(10);
        }
    }
}

say('Worker berhenti.');
