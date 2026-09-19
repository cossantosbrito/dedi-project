#include <WiFi.h>
#include <PubSubClient.h>

#define USE_TLS 1

#if USE_TLS
  #include <WiFiClientSecure.h>
#endif

#include <Wire.h>
#include <LiquidCrystal_I2C.h>   // LCD air (4 pin, I2C)
#include <LiquidCrystal.h>       // LCD suhu (16 pin, paralel)
#include <OneWire.h>
#include <DallasTemperature.h>


// =====================================================
// KONFIGURASI - UBAH BAGIAN INI SAJA
// =====================================================

// --- WiFi ---
const char* WIFI_SSID = "cossantos";
const char* WIFI_PASS = "brito6699";

// --- MQTT broker ---
// HiveMQ Cloud: WAJIB TLS (port 8883) + username/password.
// Username/password dibuat di Console cluster -> Access Management -> Credentials.
// Kalau USE_TLS diganti 0, pakai host/port polos misalnya broker.emqx.io:1883.
const char* MQTT_HOST = "73ffe5ace0de41cfaac68f90275a934d.s1.eu.hivemq.cloud";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "dedi2026";                // tidak boleh kosong di HiveMQ
const char* MQTT_PASS = "dedi@2026#";

// --- Identitas alat ---
// Kalau pakai broker publik, ganti jadi sesuatu yang unik,
// misalnya "coldroom-untl-a7", supaya tidak bentrok dengan orang lain.
const char* DEVICE_ID = "coldroom-daya-marketing";
const char* TOPIC_BASE = "coldroom";     // topic = coldroom/<DEVICE_ID>/...

// --- Pengaturan tangki & interval ---
float tankHeight = 21.33;                 // tinggi tangki (cm), bisa diubah via MQTT
unsigned long publishInterval = 2000;    // jarak antar siklus baca+kirim (ms)


// =====================================================
// DS18B20
// =====================================================

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


// =====================================================
// HC-SR04
// =====================================================

#define TRIG_PIN 5
#define ECHO_PIN 18

const int SAMPLE_COUNT = 5;   // jumlah pembacaan per siklus


// =====================================================
// LCD 1: TINGGI AIR (I2C 4 pin)
// =====================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define LCD_AIR_ADDR 0x27   // ganti sesuai hasil I2C scanner

LiquidCrystal_I2C lcdAir(LCD_AIR_ADDR, 16, 2);


// =====================================================
// LCD 2: SUHU (16 pin paralel, mode 4-bit)
// =====================================================

#define LCD_RS 19
#define LCD_E  23
#define LCD_D4 25
#define LCD_D5 26
#define LCD_D6 27
#define LCD_D7 33

LiquidCrystal lcdSuhu(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);


// =====================================================
// OBJEK JARINGAN
// =====================================================

#if USE_TLS
  WiFiClientSecure net;
#else
  WiFiClient net;
#endif

PubSubClient mqtt(net);

char topicTelemetry[80];
char topicStatus[80];
char topicCommand[80];

unsigned long lastCycle       = 0;
unsigned long lastMqttAttempt = 0;
bool          forcePublish    = false;


// =====================================================
// FUNGSI BANTU: tulis satu baris penuh 16 karakter
// (template: bisa untuk LCD I2C maupun LCD 16 pin)
// =====================================================

template <typename T>
void printLine(T &lcd, int row, String text) {

  while (text.length() < 16) {
    text += " ";
  }

  lcd.setCursor(0, row);
  lcd.print(text.substring(0, 16));
}


// Penanda kecil di pojok kanan LCD:
//   '*' = WiFi + MQTT OK
//   'w' = WiFi OK, MQTT putus
//   'x' = WiFi putus
char linkMark() {

  if (WiFi.status() != WL_CONNECTED) return 'x';
  if (!mqtt.connected())             return 'w';

  return '*';
}


// Baris judul + penanda koneksi di kolom ke-16
template <typename T>
void printHeader(T &lcd, String title) {

  while (title.length() < 15) {
    title += " ";
  }

  title = title.substring(0, 15) + String(linkMark());

  lcd.setCursor(0, 0);
  lcd.print(title);
}


// =====================================================
// HC-SR04: satu kali pembacaan (mikrodetik)
// =====================================================

long readEchoDuration() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  return pulseIn(ECHO_PIN, HIGH, 30000);   // timeout 30 ms
}


// =====================================================
// HC-SR04: baca beberapa kali, rata-rata yang valid
// =====================================================

float readDistanceCm(int &validCount, long &lastDuration) {

  float total = 0;
  validCount = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {

    long duration = readEchoDuration();
    lastDuration = duration;

    if (duration > 0) {
      total += duration * 0.0343 / 2.0;
      validCount++;
    }

    // mqtt.loop() dipanggil di sela-sela sampling supaya
    // koneksi tidak dianggap mati saat sensor sedang dibaca
    mqtt.loop();
    delay(60);
  }

  if (validCount == 0) {
    return tankHeight;
  }

  return total / validCount;
}


// =====================================================
// WIFI
// =====================================================

void wifiConnect() {

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);            // lebih stabil untuk alat yang selalu online
  WiFi.setHostname(DEVICE_ID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Menyambung WiFi ke ");
  Serial.print(WIFI_SSID);

  printLine(lcdAir, 0, "WiFi connect...");
  printLine(lcdAir, 1, String(WIFI_SSID));

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());

    printLine(lcdAir, 0, "WiFi OK");
    printLine(lcdAir, 1, WiFi.localIP().toString());

  } else {

    Serial.println("WiFi GAGAL (akan dicoba lagi di loop)");

    printLine(lcdAir, 0, "WiFi GAGAL");
    printLine(lcdAir, 1, "coba lagi...");
  }

  delay(1500);
}


// Cek WiFi di setiap loop, sambung ulang kalau putus
void wifiEnsure() {

  static unsigned long lastRetry = 0;

  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastRetry < 10000) return;

  lastRetry = millis();

  Serial.println("WiFi putus, menyambung ulang...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}


// =====================================================
// MQTT
// =====================================================

// Pesan masuk dari topic perintah: coldroom/<id>/cmd
//   tank=120        -> ubah tinggi tangki jadi 120 cm
//   interval=5000   -> ubah interval kirim jadi 5 detik
//   publish         -> kirim data sekarang juga
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String msg;

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  msg.trim();

  Serial.print("Perintah masuk: ");
  Serial.println(msg);

  if (msg.startsWith("tank=")) {

    float v = msg.substring(5).toFloat();

    if (v > 2 && v < 1000) {
      tankHeight = v;
      Serial.print("Tinggi tangki diubah jadi ");
      Serial.println(tankHeight);
    }

  } else if (msg.startsWith("interval=")) {

    long v = msg.substring(9).toInt();

    if (v >= 1000 && v <= 60000) {
      publishInterval = v;
      Serial.print("Interval diubah jadi ");
      Serial.println(publishInterval);
    }

  } else if (msg == "publish") {

    forcePublish = true;
  }
}


// Sambung / sambung ulang MQTT tanpa memblokir loop
void mqttEnsure() {

  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;

  if (millis() - lastMqttAttempt < 5000) return;

  lastMqttAttempt = millis();

  // client id harus unik di broker
  String clientId = String(DEVICE_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("Menyambung MQTT ke ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" ... ");

  bool ok;

  // Last Will: kalau ESP32 mati mendadak, broker mengirim "offline"
  // ke topic status secara otomatis (retained).
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                      topicStatus, 1, true, "offline");
  } else {
    ok = mqtt.connect(clientId.c_str(), NULL, NULL,
                      topicStatus, 1, true, "offline");
  }

  if (ok) {

    Serial.println("OK");

    mqtt.publish(topicStatus, "online", true);   // retained
    mqtt.subscribe(topicCommand);

    Serial.print("Publish ke  : ");
    Serial.println(topicTelemetry);
    Serial.print("Subscribe   : ");
    Serial.println(topicCommand);

  } else {

    Serial.print("GAGAL, state=");
    Serial.print(mqtt.state());
    Serial.print(", heap bebas ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" byte (coba lagi 5 detik lagi)");

#if USE_TLS
    if (mqtt.state() == -4) {
      Serial.println("  -> timeout saat TLS. Cek host, port 8883, dan jam/heap.");
    }
    if (strlen(MQTT_USER) == 0) {
      Serial.println("  -> MQTT_USER masih kosong. HiveMQ Cloud menolak koneksi anonim.");
    }
#else
    if (mqtt.state() == -4) {
      Serial.println("  -> broker tidak membalas. Kalau port 8883, USE_TLS harus 1.");
    }
#endif
  }
}


// Kirim satu payload JSON
void publishTelemetry(float temperature, bool tempOk,
                      float distance, bool echoOk,
                      float waterLevel, int validCount) {

  if (!mqtt.connected()) return;

  char payload[320];

  snprintf(payload, sizeof(payload),
    "{\"device\":\"%s\","
    "\"temp\":%.2f,\"temp_ok\":%d,"
    "\"distance\":%.2f,\"level\":%.2f,\"level_ok\":%d,"
    "\"valid\":%d,\"samples\":%d,"
    "\"tank\":%.1f,\"percent\":%.1f,"
    "\"rssi\":%d,\"ip\":\"%s\",\"uptime\":%lu}",
    DEVICE_ID,
    tempOk ? temperature : 0.0, tempOk ? 1 : 0,
    distance, waterLevel, echoOk ? 1 : 0,
    validCount, SAMPLE_COUNT,
    tankHeight, tankHeight > 0 ? (waterLevel / tankHeight * 100.0) : 0.0,
    WiFi.RSSI(), WiFi.localIP().toString().c_str(),
    millis() / 1000);

  bool sent = mqtt.publish(topicTelemetry, payload);

  Serial.print("MQTT        : ");
  Serial.println(sent ? "terkirim" : "GAGAL kirim");
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  // Serial Monitor
  Serial.begin(115200);

  // I2C ESP32 (untuk LCD air)
  Wire.begin(SDA_PIN, SCL_PIN);

  // DS18B20
  sensors.begin();

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // LCD 1 - Tinggi Air (I2C)
  lcdAir.init();
  lcdAir.backlight();
  printLine(lcdAir, 0, "LCD AIR");
  printLine(lcdAir, 1, "I2C 0x" + String(LCD_AIR_ADDR, HEX));

  // LCD 2 - Suhu (16 pin)
  lcdSuhu.begin(16, 2);
  printLine(lcdSuhu, 0, "LCD SUHU");
  printLine(lcdSuhu, 1, "16 pin paralel");

  delay(2000);

  lcdAir.clear();
  lcdSuhu.clear();

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 COLD ROOM MONITOR + MQTT");
  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);
  Serial.print("Jumlah DS18B20 terdeteksi: ");
  Serial.println(sensors.getDeviceCount());
  Serial.println("==============================");

  // Susun nama topic
  snprintf(topicTelemetry, sizeof(topicTelemetry), "%s/%s/telemetry", TOPIC_BASE, DEVICE_ID);
  snprintf(topicStatus,    sizeof(topicStatus),    "%s/%s/status",    TOPIC_BASE, DEVICE_ID);
  snprintf(topicCommand,   sizeof(topicCommand),   "%s/%s/cmd",       TOPIC_BASE, DEVICE_ID);

  // WiFi + MQTT
  wifiConnect();

#if USE_TLS
  // setInsecure() = TLS aktif, tapi sertifikat broker tidak diperiksa.
  // Trafik tetap terenkripsi. Untuk memeriksa sertifikat, lihat catatan di README.
  net.setInsecure();
  net.setTimeout(15);
#endif

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(20);        // handshake TLS butuh waktu lebih lama
  mqtt.setBufferSize(512);          // default 256 byte, payload kita bisa lebih

  mqttEnsure();

  lcdAir.clear();
}


// =====================================================
// SATU SIKLUS: BACA - TAMPIL - KIRIM
// =====================================================

void runCycle() {

  // ===================================================
  // BACA DS18B20
  // ===================================================

  sensors.requestTemperatures();

  float temperature = sensors.getTempCByIndex(0);

  // -127 = sensor tidak terhubung, 85 = pembacaan belum valid
  bool tempOk = (temperature != DEVICE_DISCONNECTED_C) &&
                (temperature != 85.0);


  // ===================================================
  // BACA HC-SR04
  // ===================================================

  int  validCount = 0;
  long lastDuration = 0;

  float distance = readDistanceCm(validCount, lastDuration);
  bool echoOk = (validCount > 0);


  // ===================================================
  // HITUNG TINGGI AIR
  // ===================================================

  float waterLevel = tankHeight - distance;

  if (waterLevel < 0) waterLevel = 0;
  if (waterLevel > tankHeight) waterLevel = tankHeight;


  // ===================================================
  // LCD 1: TINGGI AIR
  // ===================================================

  printHeader(lcdAir, "Nivel bee:");

  if (echoOk) {
    printLine(lcdAir, 1, String(waterLevel, 1) + " cm");
  } else {
    printLine(lcdAir, 1, "HC-SR04 ERROR");
  }


  // ===================================================
  // LCD 2: SUHU
  // ===================================================

  printLine(lcdSuhu, 0, "Temperatura :");

  if (tempOk) {
    printLine(lcdSuhu, 1, String(temperature, 1) + " C");
  } else {
    printLine(lcdSuhu, 1, "DS18B20 ERROR");
  }


  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.println("------------------------------");

  Serial.print("Temperature : ");
  if (tempOk) {
    Serial.print(temperature, 2);
    Serial.println(" C");
  } else {
    Serial.print("ERROR (valor: ");
    Serial.print(temperature);
    Serial.println(")");
  }

  Serial.print("HC-SR04     : ");
  Serial.print(validCount);
  Serial.print("/");
  Serial.print(SAMPLE_COUNT);
  Serial.print(" valid, duration terakhir = ");
  Serial.print(lastDuration);
  Serial.println(" us");

  Serial.print("Distance    : ");
  if (echoOk) {
    Serial.print(distance, 2);
    Serial.println(" cm");
  } else {
    Serial.println("ERROR (tidak ada echo)");
  }

  Serial.print("Water Level : ");
  Serial.print(waterLevel, 2);
  Serial.println(" cm");

  Serial.print("WiFi        : ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(WiFi.localIP());
    Serial.print("  RSSI ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("terputus");
  }


  // ===================================================
  // KIRIM KE MQTT
  // ===================================================

  publishTelemetry(temperature, tempOk, distance, echoOk, waterLevel, validCount);

  Serial.println("------------------------------");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  wifiEnsure();
  mqttEnsure();
  mqtt.loop();          // wajib dipanggil sesering mungkin

  if (forcePublish || millis() - lastCycle >= publishInterval) {

    forcePublish = false;
    lastCycle = millis();

    runCycle();
  }
}
