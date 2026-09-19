<?php
/**
 * GET api/readings.php
 *
 * Parameter:
 *   device     nama alat
 *   from, to   tanggal lokal YYYY-MM-DD
 *   problems   1 = hanya baris yang sensornya gagal
 *   page       nomor halaman, mulai 1
 *   page_size  jumlah baris, maksimum 500
 */

require_once __DIR__ . '/filters.php';

header('Content-Type: application/json; charset=utf-8');

const PAGE_SIZE_MAX = 500;

try {
    [$where, $params] = build_filters();

    $pdo = db();

    $countSql = 'SELECT COUNT(*) FROM readings ' . $where;
    $stmt = $pdo->prepare($countSql);
    $stmt->execute($params);
    $total = (int) $stmt->fetchColumn();

    $pageSize = (int) ($_GET['page_size'] ?? 50);
    $pageSize = max(1, min($pageSize, PAGE_SIZE_MAX));

    $pages = max(1, (int) ceil($total / $pageSize));
    $page  = max(1, min((int) ($_GET['page'] ?? 1), $pages));
    $offset = ($page - 1) * $pageSize;

    // LIMIT/OFFSET tidak bisa jadi parameter terikat di MySQL,
    // jadi nilainya sudah dipaksa jadi integer di atas.
    $sql = 'SELECT id, device, recorded_at, temperature, water_level, distance,
                   percent, temp_ok, level_ok, rssi
            FROM readings ' . $where . '
            ORDER BY recorded_at DESC, id DESC
            LIMIT ' . $pageSize . ' OFFSET ' . $offset;

    $stmt = $pdo->prepare($sql);
    $stmt->execute($params);

    $results = [];

    foreach ($stmt as $row) {
        $local = to_local($row['recorded_at']);

        $results[] = [
            'id'          => (int) $row['id'],
            'device'      => $row['device'],
            'date'        => $local->format('Y-m-d'),
            'time'        => $local->format('H:i:s'),
            'iso'         => $local->format(DateTimeInterface::ATOM),
            'temperature' => $row['temperature'] === null ? null : (float) $row['temperature'],
            'water_level' => $row['water_level'] === null ? null : (float) $row['water_level'],
            'percent'     => $row['percent'] === null ? null : (float) $row['percent'],
            'distance'    => $row['distance'] === null ? null : (float) $row['distance'],
            'temp_ok'     => (bool) $row['temp_ok'],
            'level_ok'    => (bool) $row['level_ok'],
            'rssi'        => $row['rssi'] === null ? null : (int) $row['rssi'],
            'note'        => note_for($row),
        ];
    }

    echo json_encode([
        'count'     => $total,
        'pages'     => $pages,
        'page'      => $page,
        'page_size' => $pageSize,
        'results'   => $results,
    ], JSON_UNESCAPED_UNICODE);

} catch (Throwable $e) {
    http_response_code(500);
    error_log('readings.php: ' . $e->getMessage());
    echo json_encode(['error' => 'Tidak bisa mengambil data dari database.']);
}
