'use strict';

/**
 * Berlangganan topic telemetry ESP32 dan simpan ke MySQL.
 *
 * ESP32 mengirim tiap beberapa detik. Menyimpan semuanya akan membuat tabel
 * sangat besar, jadi disaring lewat shouldSave(): paling cepat satu baris
 * per MQTT_MIN_INTERVAL detik per alat, kecuali saat status sensor
 * (temp_ok/level_ok) berubah — itu selalu disimpan supaya kegagalan tidak
 * terlewat di antara dua interval.
 */

require('dotenv').config();
const mqtt = require('mqtt');
const pool = require('./db');

const MIN_INTERVAL_MS = Number(process.env.MQTT_MIN_INTERVAL || 60) * 1000;
const lastSaved = new Map(); // device -> timestamp (ms)
const lastFlags = new Map(); // device -> "tempOk,levelOk"

function shouldSave(device, tempOk, levelOk) {
  const flagKey = tempOk + ',' + levelOk;
  const now = Date.now();

  if (lastFlags.get(device) !== flagKey) {
    lastFlags.set(device, flagKey);
    lastSaved.set(device, now);
    return true;
  }

  const last = lastSaved.get(device);
  if (last === undefined || now - last >= MIN_INTERVAL_MS) {
    lastSaved.set(device, now);
    return true;
  }

  return false;
}

async function handleMessage(topic, payload) {
  let data;
  try {
    data = JSON.parse(payload.toString());
  } catch (e) {
    console.log('Payload sala (la\'os JSON):', payload.toString().slice(0, 100));
    return;
  }

  const parts = topic.split('/');
  const device = data.device || parts[parts.length - 2] || 'la-koinesidu';
  const tempOk = !!data.temp_ok;
  const levelOk = !!data.level_ok;

  if (!shouldSave(device, tempOk, levelOk)) return;

  const sql = `INSERT INTO readings
    (device, recorded_at, temperature, water_level, distance, percent, tank_height, temp_ok, level_ok, rssi, uptime)
    VALUES (?, UTC_TIMESTAMP(), ?, ?, ?, ?, ?, ?, ?, ?, ?)`;

  const params = [
    device,
    tempOk && data.temp !== undefined ? Number(data.temp) : null,
    levelOk && data.level !== undefined ? Number(data.level) : null,
    data.distance !== undefined ? Number(data.distance) : null,
    levelOk && data.percent !== undefined ? Number(data.percent) : null,
    data.tank !== undefined ? Number(data.tank) : null,
    tempOk ? 1 : 0,
    levelOk ? 1 : 0,
    data.rssi !== undefined ? Number(data.rssi) : null,
    data.uptime !== undefined ? Number(data.uptime) : null,
  ];

  try {
    await pool.execute(sql, params);
    console.log(
      new Date().toLocaleTimeString(), 'tersimpan', device,
      'temp=' + (tempOk ? data.temp : 'falha'),
      'nivel=' + (levelOk ? data.level : 'falha')
    );
  } catch (e) {
    console.error('Falha atu rai iha database:', e.message);
  }
}

function startIngest() {
  const host = process.env.MQTT_HOST;
  const port = Number(process.env.MQTT_PORT || 8883);
  const topic = process.env.MQTT_TOPIC || 'coldroom/+/telemetry';
  const url = 'mqtts://' + host + ':' + port;

  const client = mqtt.connect(url, {
    username: process.env.MQTT_USER,
    password: process.env.MQTT_PASS,
    clientId: 'coldroom-ingest-' + Math.random().toString(16).slice(2, 10),
    reconnectPeriod: 5000,
    connectTimeout: 10000,
  });

  client.on('connect', function () {
    console.log('Ingest: konektadu ba', host);
    client.subscribe(topic, { qos: 1 }, function (err) {
      if (err) console.error('Ingest: falha subskrisaun:', err.message);
      else console.log('Ingest: subskreve', topic);
    });
  });

  client.on('message', function (topic, payload) {
    handleMessage(topic, payload).catch(function (e) { console.error(e); });
  });

  client.on('error', function (e) { console.error('Ingest erru:', e.message); });
  client.on('reconnect', function () { console.log('Ingest: hakonektu fila fali...'); });

  return client;
}

module.exports = { startIngest, shouldSave, handleMessage };
