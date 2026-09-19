# ESP32 Cold Room Monitor — WiFi + MQTT

Sensor suhu (DS18B20) dan tinggi air (HC-SR04) dibaca ESP32, ditampilkan di dua LCD,
lalu dikirim ke broker MQTT. Dashboard web berlangganan topic yang sama lewat WebSocket.

```
ESP32 ──MQTT/TCP 1883──▶ Broker ◀──MQTT/WebSocket 8084── dashboard.html (browser)
```

## Isi

| File | Keterangan |
|---|---|
| `coldroom_mqtt.ino` | Sketch ESP32 (sensor + LCD + WiFi + MQTT) |
| `dashboard.html` | Dashboard web, satu file, tanpa server |

## 1. Library yang diperlukan

Arduino IDE → Tools → Manage Libraries, pasang:

- LiquidCrystal I2C — Frank de Brabander
- OneWire — Paul Stoffregen
- DallasTemperature — Miles Burton
- **PubSubClient — Nick O'Leary** (yang baru untuk MQTT)

`WiFi.h` dan `LiquidCrystal.h` sudah termasuk di board package ESP32.
Board: **ESP32 Dev Module**, Upload Speed 921600, Serial Monitor 115200.

## 2. Ubah bagian konfigurasi

Di bagian atas `coldroom_mqtt.ino`:

```cpp
const char* WIFI_SSID = "GANTI_NAMA_WIFI";
const char* WIFI_PASS = "GANTI_PASSWORD_WIFI";

const char* MQTT_HOST = "broker.emqx.io";   // broker publik, untuk testing
const int   MQTT_PORT = 1883;

const char* DEVICE_ID = "coldroom-01";      // buat unik, mis. "coldroom-untl-a7"
float tankHeight = 100.0;                   // tinggi tangki sebenarnya (cm)
```

ESP32 hanya bisa WiFi 2.4 GHz — kalau router dual band, pastikan SSID 2.4 GHz.

## 3. Topic MQTT

| Topic | Arah | Isi |
|---|---|---|
| `coldroom/<DEVICE_ID>/telemetry` | ESP32 → | JSON pembacaan sensor |
| `coldroom/<DEVICE_ID>/status` | ESP32 → | `online` / `offline` (retained, Last Will) |
| `coldroom/<DEVICE_ID>/cmd` | → ESP32 | `tank=120`, `interval=5000`, `publish` |

Contoh payload telemetry:

```json
{"device":"coldroom-01","temp":4.25,"temp_ok":1,
 "distance":35.50,"level":64.50,"level_ok":1,
 "valid":5,"samples":5,"tank":100.0,"percent":64.5,
 "rssi":-62,"ip":"192.168.1.50","uptime":184}
```

`temp_ok` dan `level_ok` bernilai 0 kalau sensor gagal, jadi dashboard bisa
membedakan "0 °C" yang benar dari sensor yang lepas.

## 4. Upload dan cek Serial Monitor

Yang diharapkan muncul:

```
Menyambung WiFi ke ....
WiFi OK. IP: 192.168.1.50
Menyambung MQTT ke broker.emqx.io:1883 ... OK
Publish ke  : coldroom/coldroom-01/telemetry
```

Kalau MQTT gagal, `state=` menunjukkan sebabnya:

| state | Arti |
|---|---|
| -4 | timeout, broker tidak membalas |
| -2 | koneksi TCP gagal (host/port salah, atau port 1883 diblok firewall) |
| 4 | username / password ditolak |
| 5 | client tidak diizinkan broker |

## 5. Uji dari komputer sebelum buka dashboard

```bash
mosquitto_sub -h broker.emqx.io -t 'coldroom/coldroom-01/#' -v
```

Atau pakai aplikasi MQTTX. Kalau di sini sudah kelihatan datanya, masalah
apa pun di dashboard berarti soal WebSocket, bukan soal ESP32.

## 6. Buka dashboard

Buka `dashboard.html` di browser (klik dua kali sudah cukup), lalu isi:

- Alamat broker: `wss://broker.emqx.io:8084/mqtt`
- Topic dasar: `coldroom/coldroom-01`

Klik **Hubungkan**. Pengaturan tersimpan di browser untuk kunjungan berikutnya.

Catatan port broker publik EMQX: TCP 1883, TLS 8883, WebSocket 8083, WebSocket TLS 8084,
path `/mqtt` wajib ditulis.

Kalau dashboard di-host lewat HTTPS, alamat broker **harus** `wss://` — browser
menolak `ws://` dari halaman HTTPS.

## 7. Untuk pemakaian sungguhan

Broker publik terbuka untuk siapa saja: orang lain bisa membaca data dan mengirim
perintah ke topic Anda. Untuk produksi, pasang Mosquitto sendiri:

```bash
sudo apt install mosquitto mosquitto-clients
sudo mosquitto_passwd -c /etc/mosquitto/passwd coldroom
```

`/etc/mosquitto/conf.d/coldroom.conf`:

```
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous false
password_file /etc/mosquitto/passwd
```

Lalu taruh Nginx di depan port 9001 untuk TLS (`wss://`), dan isi `MQTT_USER` /
`MQTT_PASS` di sketch. Untuk TLS di sisi ESP32, ganti `WiFiClient` jadi
`WiFiClientSecure` + `setCACert()` dan pakai port 8883.

## Catatan pin

Pin yang dipakai (4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 33) tidak bentrok dengan WiFi.
Tapi selama WiFi aktif, pin ADC2 (0, 2, 4, 12–15, 25–27) tidak bisa dipakai untuk
`analogRead()` — pemakaian digital seperti di proyek ini tetap aman. Kalau nanti
menambah sensor analog, pakai pin ADC1 (32–39).
