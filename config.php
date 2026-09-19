<?php
/**
 * Konfigurasi. Nilai diambil dari environment kalau ada, kalau tidak pakai
 * nilai di bawah. Jangan simpan sandi asli di berkas yang ikut ke Git.
 */

// --------------------------------------------------------------------
// Zona waktu

const APP_TIMEZONE = 'Asia/Dili';   // UTC+9

date_default_timezone_set(APP_TIMEZONE);

// --------------------------------------------------------------------
// Database MySQL

const DB_HOST = '127.0.0.1';
const DB_PORT = '3306';
const DB_NAME = 'coldroom';
const DB_USER = 'coldroom';
const DB_PASS = 'sandi_yang_kuat';

// --------------------------------------------------------------------
// Broker MQTT (dipakai oleh mqtt_ingest.php)

const MQTT_HOST = '73ffe5ace0de41cfaac68f90275a934d.s1.eu.hivemq.cloud';
const MQTT_PORT = 8883;
const MQTT_USER = 'dedi2026';
const MQTT_PASS = 'ganti_sandi_hivemq';
const MQTT_TLS  = true;

// '+' cocok dengan semua device id, jadi alat kedua langsung ikut tersimpan
const MQTT_TOPIC = 'coldroom/+/telemetry';

/**
 * ESP32 mengirim tiap 2 detik. Menyimpan semuanya berarti ~43.000 baris per
 * hari per alat. Angka ini membatasi penyimpanan jadi satu baris per sekian
 * detik, kecuali saat status sensor berubah — itu selalu disimpan.
 */
const MQTT_MIN_INTERVAL = 60;

// --------------------------------------------------------------------
// Ambil dari environment kalau tersedia (lebih aman untuk produksi)

function conf(string $envKey, $fallback)
{
    $value = getenv($envKey);
    return ($value === false || $value === '') ? $fallback : $value;
}
