# Coldroom server (MQTT -> MySQL + API export)

Proses Node.js kecil yang:

1. Berlangganan broker MQTT yang sama dengan dashboard (topic `coldroom/+/telemetry`)
   dan menyimpan tiap pembacaan ke MySQL.
2. Menyediakan API HTTP (`/api/readings`, `/api/export.csv`) yang dipakai tombol
   **"Exporta CSV"** di `index.html`.

MySQL sudah harus terpasang dan servicenya jalan (di komputer ini: service `MySQL80`).

## 1. Buat database + tabel

Jalankan lewat MySQL Workbench atau `mysql` CLI, login sebagai `root`:

```sql
SOURCE D:/dedi project/schema.sql;
```

(atau copy-paste isi `schema.sql` yang ada di root folder proyek ini.)

## 2. Buat user aplikasi (jangan pakai root dari sini)

Masih sebagai `root`, jalankan (ganti passwordnya):

```sql
CREATE USER 'coldroom'@'localhost' IDENTIFIED BY 'ganti-dengan-password-kuat';
GRANT SELECT, INSERT ON coldroom.* TO 'coldroom'@'localhost';
FLUSH PRIVILEGES;
```

## 3. Isi konfigurasi

```bash
cd server
copy .env.example .env
```

Buka `.env` dan isi:

- `DB_USER` / `DB_PASS` — user yang baru dibuat di langkah 2.
- `MQTT_USER` / `MQTT_PASS` — kredensial broker HiveMQ Cloud yang sama dipakai
  ESP32 dan dashboard (**bukan** nilai yang tampil di field "Username" dashboard,
  itu cuma label tampilan).

## 4. Install dependency & jalankan

```bash
cd server
npm install
npm start
```

Kalau berhasil akan muncul:

```
API dashboard iha http://localhost:8090/api
Ingest: konektadu ba 73ffe5ace0de41cfaac68f90275a934d.s1.eu.hivemq.cloud
Ingest: subskreve coldroom/+/telemetry
```

Biarkan proses ini tetap berjalan (mis. di jendela terminal terpisah, atau
dipasang sebagai Windows service / Task Scheduler kalau mau jalan terus-menerus).

## 5. Export dari dashboard

Dengan server ini jalan, tombol **"Exporta CSV"** di panel Istória pada
`index.html` akan mengunduh seluruh riwayat (atau yang difilter) sebagai file CSV.

Endpoint mendukung parameter query opsional:

- `?device=coldroom-01`
- `?from=2026-09-01&to=2026-09-19`
- `?problems=1` (hanya baris yang sensornya gagal)

Contoh: `http://localhost:8090/api/export.csv?from=2026-09-01&to=2026-09-19`
