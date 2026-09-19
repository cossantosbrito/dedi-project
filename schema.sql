-- Jalankan sekali:
--   mysql -u root -p < schema.sql

CREATE DATABASE IF NOT EXISTS coldroom
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE coldroom;

CREATE TABLE IF NOT EXISTS readings (
  id           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,

  device       VARCHAR(64)  NOT NULL,
  recorded_at  DATETIME     NOT NULL COMMENT 'disimpan dalam UTC',

  temperature  DOUBLE       NULL COMMENT 'derajat C, NULL kalau DS18B20 gagal',
  water_level  DOUBLE       NULL COMMENT 'cm, NULL kalau HC-SR04 gagal',
  distance     DOUBLE       NULL COMMENT 'cm, jarak mentah sensor',
  percent      DOUBLE       NULL,
  tank_height  DOUBLE       NULL,

  temp_ok      TINYINT(1)   NOT NULL DEFAULT 1,
  level_ok     TINYINT(1)   NOT NULL DEFAULT 1,

  rssi         INT          NULL,
  uptime       BIGINT       NULL COMMENT 'detik sejak ESP32 menyala',

  PRIMARY KEY (id),
  KEY idx_device_time (device, recorded_at),
  KEY idx_time (recorded_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Pengguna database khusus, jangan pakai root dari aplikasi:
--
--   CREATE USER 'coldroom'@'localhost' IDENTIFIED BY 'sandi_yang_kuat';
--   GRANT SELECT, INSERT, DELETE ON coldroom.* TO 'coldroom'@'localhost';
--   FLUSH PRIVILEGES;
