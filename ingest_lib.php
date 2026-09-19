<?php
/**
 * Logika penyimpanan, dipakai oleh dua jalur masuk:
 *   - mqtt_ingest.php   (daemon PHP dengan php-mqtt/client)
 *   - ingest_stdin.php  (disalurkan dari mosquitto_sub, tanpa Composer)
 */

require_once __DIR__ . '/db.php';

$GLOBALS['lastSaved'] = [];   // device => waktu terakhir tersimpan
$GLOBALS['lastFlags'] = [];   // device => [temp_ok, level_ok]

function say(string $msg): void
{
    echo date('Y-m-d H:i:s') . '  ' . $msg . PHP_EOL;
}

/**
 * Apakah pesan ini perlu disimpan?
 *
 * ESP32 mengirim tiap 2 detik. Menyimpan semuanya berarti ~43.000 baris per
 * hari per alat, dan tabel jadi berat. Jadi simpan berkala saja — tapi selalu
 * simpan saat status sensor berubah, supaya kegagalan tidak terlewat di
 * antara dua interval.
 */
function shouldSave(string $device, bool $tempOk, bool $levelOk): bool
{
    $interval = (int) conf('MQTT_MIN_INTERVAL', MQTT_MIN_INTERVAL);
    $flags    = [$tempOk, $levelOk];
    $now      = time();

    if (($GLOBALS['lastFlags'][$device] ?? null) !== $flags) {
        $GLOBALS['lastFlags'][$device] = $flags;
        $GLOBALS['lastSaved'][$device] = $now;
        return true;
    }

    if (!isset($GLOBALS['lastSaved'][$device])
        || $now - $GLOBALS['lastSaved'][$device] >= $interval) {
        $GLOBALS['lastSaved'][$device] = $now;
        return true;
    }

    return false;
}

/** Olah satu pesan MQTT dan simpan kalau lolos saringan di atas. */
function handleMessage(string $topic, string $payload, bool $verbose = true): bool
{
    $data = json_decode($payload, true);

    if (!is_array($data)) {
        if ($verbose) {
            say('Payload bukan JSON: ' . substr($payload, 0, 100));
        }
        return false;
    }

    $parts  = explode('/', $topic);
    $device = $data['device'] ?? ($parts[count($parts) - 2] ?? 'tidak-dikenal');

    $tempOk  = !empty($data['temp_ok']);
    $levelOk = !empty($data['level_ok']);

    if (!shouldSave($device, $tempOk, $levelOk)) {
        return false;
    }

    $sql = 'INSERT INTO readings
            (device, recorded_at, temperature, water_level, distance, percent,
             tank_height, temp_ok, level_ok, rssi, uptime)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)';

    try {
        db()->prepare($sql)->execute([
            $device,
            gmdate('Y-m-d H:i:s'),                                   // disimpan UTC
            $tempOk  && isset($data['temp'])  ? (float) $data['temp']  : null,
            $levelOk && isset($data['level']) ? (float) $data['level'] : null,
            isset($data['distance']) ? (float) $data['distance'] : null,
            $levelOk && isset($data['percent']) ? (float) $data['percent'] : null,
            isset($data['tank']) ? (float) $data['tank'] : null,
            $tempOk  ? 1 : 0,
            $levelOk ? 1 : 0,
            isset($data['rssi'])   ? (int) $data['rssi']   : null,
            isset($data['uptime']) ? (int) $data['uptime'] : null,
        ]);
    } catch (PDOException $e) {
        say('Gagal menyimpan: ' . $e->getMessage());
        return false;
    }

    if ($verbose) {
        say(sprintf(
            'Tersimpan %s  suhu=%s  tinggi=%s',
            $device,
            $tempOk  ? number_format((float) $data['temp'], 2)  : 'gagal',
            $levelOk ? number_format((float) $data['level'], 2) : 'gagal'
        ));
    }

    return true;
}
