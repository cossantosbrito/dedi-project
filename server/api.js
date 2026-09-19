'use strict';

const express = require('express');
const pool = require('./db');

const router = express.Router();
const PAGE_SIZE_MAX = 500;
const EXPORT_ROW_MAX = 50000;

function buildFilters(query) {
  const where = [];
  const params = [];

  if (query.device) {
    where.push('device = ?');
    params.push(query.device);
  }
  if (query.from) {
    where.push('recorded_at >= ?');
    params.push(query.from + ' 00:00:00');
  }
  if (query.to) {
    where.push('recorded_at <= ?');
    params.push(query.to + ' 23:59:59');
  }
  if (query.problems === '1') {
    where.push('(temp_ok = 0 OR level_ok = 0)');
  }

  const sql = where.length ? ('WHERE ' + where.join(' AND ')) : '';
  return [sql, params];
}

function noteFor(row) {
  if (!row.temp_ok && !row.level_ok) return 'Sensór rua (temperatura no distánsia) falha';
  if (!row.temp_ok) return 'Sensór temperatura falha';
  if (!row.level_ok) return 'Sensór distánsia falha';
  return '';
}

function csvField(value) {
  const s = value === null || value === undefined ? '' : String(value);
  return /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
}

router.get('/readings', async function (req, res) {
  try {
    const [where, params] = buildFilters(req.query);

    const [countRows] = await pool.query(
      'SELECT COUNT(*) AS total FROM readings ' + where, params
    );
    const total = countRows[0].total;

    let pageSize = parseInt(req.query.page_size, 10) || 50;
    pageSize = Math.max(1, Math.min(pageSize, PAGE_SIZE_MAX));
    const pages = Math.max(1, Math.ceil(total / pageSize));
    let page = parseInt(req.query.page, 10) || 1;
    page = Math.max(1, Math.min(page, pages));
    const offset = (page - 1) * pageSize;

    const [rows] = await pool.query(
      'SELECT id, device, recorded_at, temperature, water_level, distance, percent, temp_ok, level_ok, rssi ' +
      'FROM readings ' + where + ' ' +
      'ORDER BY recorded_at DESC, id DESC ' +
      'LIMIT ' + pageSize + ' OFFSET ' + offset,
      params
    );

    res.json({
      count: total,
      pages: pages,
      page: page,
      page_size: pageSize,
      results: rows.map(function (r) {
        return {
          id: r.id,
          device: r.device,
          recorded_at: r.recorded_at,
          temperature: r.temperature,
          water_level: r.water_level,
          distance: r.distance,
          percent: r.percent,
          temp_ok: !!r.temp_ok,
          level_ok: !!r.level_ok,
          rssi: r.rssi,
          note: noteFor(r),
        };
      }),
    });
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: 'La bele hetan dadus husi database.' });
  }
});

router.get('/export.csv', async function (req, res) {
  try {
    const [where, params] = buildFilters(req.query);

    const [rows] = await pool.query(
      'SELECT id, device, recorded_at, temperature, water_level, distance, percent, tank_height, temp_ok, level_ok, rssi, uptime ' +
      'FROM readings ' + where + ' ' +
      'ORDER BY recorded_at DESC, id DESC ' +
      'LIMIT ' + EXPORT_ROW_MAX,
      params
    );

    res.setHeader('Content-Type', 'text/csv; charset=utf-8');
    res.setHeader('Content-Disposition', 'attachment; filename="coldroom-history.csv"');

    const header = [
      'id', 'device', 'recorded_at_utc', 'temperature_c', 'water_level_cm',
      'distance_cm', 'percent', 'tank_height_cm', 'temp_ok', 'level_ok',
      'rssi_dbm', 'uptime_s', 'note',
    ];
    res.write(header.join(',') + '\n');

    for (const r of rows) {
      const line = [
        r.id, r.device, r.recorded_at,
        r.temperature, r.water_level, r.distance,
        r.percent, r.tank_height,
        r.temp_ok, r.level_ok, r.rssi, r.uptime,
        noteFor(r),
      ].map(csvField).join(',');
      res.write(line + '\n');
    }

    res.end();
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: 'La bele exporta dadus.' });
  }
});

module.exports = router;
