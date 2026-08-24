<?php

declare(strict_types=1);

// End-to-end companion benchmark for jbboehr/php-mustache develop.

const TARGETS = [
    'small' => 1024,
    'medium' => 32768,
    'large' => 262144,
];

const MATERIAL_WIN_PERCENT = 20.0;

function buildWorkload(string $name, int $targetBytes, bool $nestedPartials): array
{
    $root = <<<'MUSTACHE'
<!doctype html>
<html>
<body>
{{#products}}
  {{> layout}}
{{/products}}
</body>
</html>

MUSTACHE;
    $layout = <<<'MUSTACHE'
<main aria-label="{{title}}">
  {{> card}}
</main>

MUSTACHE;
    $badge = <<<'MUSTACHE'
{{#featured}}<strong class="badge">Featured</strong>{{/featured}}

MUSTACHE;
    $unit = <<<'MUSTACHE'
{{! production-shaped product card }}
<section class="product product--{{category.slug}}">
  <h2>{{title}}</h2>
  {{#available}}
    <p>{{description}}</p>
    <span data-id="{{id}}">{{price}}</span>
  {{/available}}
  {{^available}}
    <span class="unavailable">Unavailable</span>
  {{/available}}
  {{> badge}}
</section>

MUSTACHE;

    if (!$nestedPartials) {
        $flatUnit = str_replace(
            "  {{> badge}}\n",
            "  {{#featured}}<strong class=\"badge\">Featured</strong>{{/featured}}\n",
            $unit,
        );
        $prefix = "<!doctype html>\n<html>\n<body>\n{{#products}}\n";
        $suffix = "{{/products}}\n</body>\n</html>\n";
        $body = '';
        while (strlen($prefix) + strlen($body) + strlen($suffix) < $targetBytes) {
            $body .= $flatUnit;
        }

        return [
            'name' => $name,
            'target_source_bytes' => $targetBytes,
            'root' => $prefix . $body . $suffix,
            'partials' => [],
        ];
    }

    $card = '';
    while (strlen($root) + strlen($layout) + strlen($badge) + strlen($card) < $targetBytes) {
        $card .= $unit;
    }

    return [
        'name' => $name,
        'target_source_bytes' => $targetBytes,
        'root' => $root,
        'partials' => [
            'layout' => $layout,
            'card' => $card,
            'badge' => $badge,
        ],
    ];
}

function graphBytes(string $root, array $partials): int
{
    return strlen($root) + array_sum(array_map('strlen', $partials));
}

function parseGraph(Mustache $mustache, string $root, array $partials): array
{
    $parsedPartials = [];
    foreach ($partials as $name => $source) {
        $parsedPartials[$name] = $mustache->parse($source);
    }

    return [
        'root' => $mustache->parse($root),
        'partials' => $parsedPartials,
    ];
}

function encodeGraph(array $graph): array
{
    $encodedPartials = [];
    foreach ($graph['partials'] as $name => $ast) {
        $encodedPartials[$name] = (string) $ast;
    }

    return [
        'root' => (string) $graph['root'],
        'partials' => $encodedPartials,
    ];
}

function decodeGraph(array $encoded): array
{
    $partials = [];
    foreach ($encoded['partials'] as $name => $bytes) {
        $partials[$name] = new MustacheAST($bytes);
    }

    return [
        'root' => new MustacheAST($encoded['root']),
        'partials' => $partials,
    ];
}

function sourceObjectGraph(string $root, array $partials): array
{
    $objects = [];
    foreach ($partials as $name => $source) {
        $objects[$name] = new MustacheTemplate($source);
    }

    return [
        'root' => new MustacheTemplate($root),
        'partials' => $objects,
    ];
}

function benchmarkData(): array
{
    return [
        'products' => [[
            'id' => 'product-123',
            'title' => 'A rock-hard template engine',
            'description' => 'Safe ownership, bounded parsing, and compatible rendering.',
            'price' => '$19.50',
            'available' => true,
            'featured' => true,
            'category' => ['slug' => 'libraries'],
        ]],
    ];
}

function consume(mixed $value): int
{
    if (is_int($value)) {
        return $value;
    }
    if (is_bool($value)) {
        return $value ? 1 : 0;
    }
    if (is_string($value)) {
        return strlen($value);
    }
    if (is_array($value)) {
        return count($value);
    }
    if (is_object($value)) {
        return spl_object_id($value);
    }
    return 0;
}

function percentile(array $sorted, float $fraction): float
{
    $index = max(0, (int) ceil(count($sorted) * $fraction) - 1);
    return $sorted[$index];
}

function measure(callable $operation, int $warmups, int $samples): array
{
    $sink = 0;
    for ($i = 0; $i < $warmups; ++$i) {
        $sink ^= consume($operation());
    }

    gc_collect_cycles();
    if (function_exists('memory_reset_peak_usage')) {
        memory_reset_peak_usage();
    }
    $memoryBefore = memory_get_usage(false);
    $sink ^= consume($operation());
    $phpPeakDelta = max(0, memory_get_peak_usage(false) - $memoryBefore);

    $durations = [];
    gc_collect_cycles();
    $gcWasEnabled = gc_enabled();
    if ($gcWasEnabled) {
        gc_disable();
    }
    try {
        for ($i = 0; $i < $samples; ++$i) {
            $start = hrtime(true);
            $value = $operation();
            $elapsed = hrtime(true) - $start;
            $sink ^= consume($value);
            unset($value);
            $durations[] = (float) $elapsed;
        }
    } finally {
        if ($gcWasEnabled) {
            gc_enable();
        }
    }

    sort($durations, SORT_NUMERIC);
    $median = percentile($durations, 0.50);
    $p95 = percentile($durations, 0.95);

    return [
        'median_us' => $median / 1000.0,
        'p95_us' => $p95 / 1000.0,
        'min_us' => $durations[0] / 1000.0,
        'max_us' => $durations[count($durations) - 1] / 1000.0,
        'throughput_per_second' => 1_000_000_000.0 / $median,
        'php_peak_delta_bytes' => $phpPeakDelta,
        'sink' => $sink,
    ];
}

function extensionPath(string $name): string
{
    $setting = 'BENCH_' . strtoupper($name) . '_EXTENSION';
    $path = getenv($setting);
    if (!is_string($path) || $path === '' || !is_file($path)) {
        throw new RuntimeException("{$setting} must name the loaded {$name} module for cold-worker samples");
    }
    return $path;
}

function coldWorkerCommand(bool $hasApcu): array
{
    $command = [PHP_BINARY, '-n'];
    if ($hasApcu) {
        $command[] = '-d';
        $command[] = 'extension=' . extensionPath('apcu');
        $command[] = '-d';
        $command[] = 'apc.enable_cli=1';
    }
    $command[] = '-d';
    $command[] = 'extension=' . extensionPath('mustache');
    $command[] = __FILE__;
    $command[] = '--cold-sample';
    return $command;
}

function writeColdFixture(string $sourcePayload, string $astPayload, array $data, string $expectedOutput): string
{
    $path = tempnam(sys_get_temp_dir(), 'mustache-cache-bench-');
    if (!is_string($path)) {
        throw new RuntimeException('could not allocate a cold-worker fixture');
    }

    $fixture = serialize([
        'source_payload' => $sourcePayload,
        'ast_payload' => $astPayload,
        'data' => $data,
        'expected_output_hash' => hash('sha256', $expectedOutput),
        'expected_output_bytes' => strlen($expectedOutput),
    ]);
    if (file_put_contents($path, $fixture) !== strlen($fixture)) {
        @unlink($path);
        throw new RuntimeException('could not write a complete cold-worker fixture');
    }
    return $path;
}

function coldWorkerSample(string $fixturePath, string $case): void
{
    $contents = file_get_contents($fixturePath);
    if (!is_string($contents)) {
        throw new RuntimeException('could not read the cold-worker fixture');
    }
    $fixture = unserialize($contents, ['allowed_classes' => false]);
    if (!is_array($fixture)) {
        throw new RuntimeException('invalid cold-worker fixture');
    }

    $sourceCases = ['cold_php_cache_hit_source', 'cold_apcu_cache_hit_source'];
    $astCases = ['cold_php_cache_hit_ast', 'cold_apcu_cache_hit_ast'];
    if (in_array($case, $sourceCases, true)) {
        $payload = $fixture['source_payload'];
    } elseif (in_array($case, $astCases, true)) {
        $payload = $fixture['ast_payload'];
    } else {
        throw new RuntimeException("unknown cold-worker case {$case}");
    }
    if (!is_string($payload)) {
        throw new RuntimeException('invalid cold-worker cache payload');
    }

    $useApcu = strncmp($case, 'cold_apcu_', strlen('cold_apcu_')) === 0;
    $cacheKey = 'mustache-bench-cold-' . getmypid();
    if ($useApcu) {
        if (!extension_loaded('apcu') || !apcu_store($cacheKey, $payload)) {
            throw new RuntimeException('could not prime APCu in the cold worker');
        }
    }

    $mustache = new Mustache();
    gc_collect_cycles();
    gc_disable();
    if (function_exists('memory_reset_peak_usage')) {
        memory_reset_peak_usage();
    }
    $memoryBefore = memory_get_usage(false);
    $start = hrtime(true);
    if ($useApcu) {
        $payload = apcu_fetch($cacheKey);
        if (!is_string($payload)) {
            throw new RuntimeException('APCu did not return the cold-worker payload');
        }
    }
    $graph = unserialize($payload, ['allowed_classes' => true]);
    $output = $mustache->render($graph['root'], $fixture['data'], $graph['partials']);
    $elapsed = hrtime(true) - $start;
    $phpPeakDelta = max(0, memory_get_peak_usage(false) - $memoryBefore);

    if (strlen($output) !== $fixture['expected_output_bytes']
        || hash('sha256', $output) !== $fixture['expected_output_hash']) {
        throw new RuntimeException("cold-worker rendering differs for {$case}");
    }
    if ($useApcu) {
        apcu_delete($cacheKey);
    }

    echo json_encode([
        'elapsed_ns' => $elapsed,
        'php_peak_delta_bytes' => $phpPeakDelta,
        'rendered_bytes' => strlen($output),
    ], JSON_THROW_ON_ERROR);
}

function runColdWorkerSample(array $command, string $fixturePath, string $case): array
{
    $sampleCommand = [...$command, $fixturePath, $case];
    $descriptors = [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ];
    $process = proc_open($sampleCommand, $descriptors, $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException('could not start a cold-worker sample');
    }

    fclose($pipes[0]);
    $stdout = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[2]);
    $status = proc_close($process);
    if ($status !== 0 || !is_string($stdout)) {
        throw new RuntimeException("cold-worker sample failed ({$status}): " . trim((string) $stderr));
    }

    $result = json_decode($stdout, true, flags: JSON_THROW_ON_ERROR);
    if (!is_array($result) || !is_int($result['elapsed_ns']) || !is_int($result['php_peak_delta_bytes'])
        || !is_int($result['rendered_bytes'])) {
        throw new RuntimeException('cold-worker sample returned invalid measurements');
    }
    return $result;
}

function measureColdWorkers(array $command, string $fixturePath, string $case, int $samples): array
{
    $durations = [];
    $peakDeltas = [];
    $sink = 0;
    for ($i = 0; $i < $samples; ++$i) {
        $sample = runColdWorkerSample($command, $fixturePath, $case);
        $durations[] = (float) $sample['elapsed_ns'];
        $peakDeltas[] = $sample['php_peak_delta_bytes'];
        $sink ^= $sample['rendered_bytes'];
    }

    sort($durations, SORT_NUMERIC);
    sort($peakDeltas, SORT_NUMERIC);
    $median = percentile($durations, 0.50);
    $p95 = percentile($durations, 0.95);
    return [
        'median_us' => $median / 1000.0,
        'p95_us' => $p95 / 1000.0,
        'min_us' => $durations[0] / 1000.0,
        'max_us' => $durations[count($durations) - 1] / 1000.0,
        'throughput_per_second' => 1_000_000_000.0 / $median,
        'php_peak_delta_bytes' => (int) percentile($peakDeltas, 0.50),
        'sink' => $sink,
    ];
}

function printResult(string $workload, string $case, array $result): void
{
    printf(
        "%-13s %-25s %11.3f %11.3f %11.0f %12d\n",
        $workload,
        $case,
        $result['median_us'],
        $result['p95_us'],
        $result['throughput_per_second'],
        $result['php_peak_delta_bytes'],
    );
}

if (!extension_loaded('mustache')) {
    fwrite(STDERR, "mustache extension is not loaded\n");
    exit(2);
}

if (($argv[1] ?? '') === '--cold-sample') {
    try {
        coldWorkerSample((string) ($argv[2] ?? ''), (string) ($argv[3] ?? ''));
        exit(0);
    } catch (Throwable $error) {
        fwrite(STDERR, $error->getMessage() . "\n");
        exit(2);
    }
}

$samples = max(21, (int) (getenv('BENCH_SAMPLES') ?: 101));
$warmups = max(3, (int) (getenv('BENCH_WARMUPS') ?: 10));
$coldSamplesSetting = getenv('BENCH_COLD_SAMPLES');
$coldSamples = $coldSamplesSetting === false ? 31 : max(0, (int) $coldSamplesSetting);
$apcuCliSetting = strtolower((string) ini_get('apc.enable_cli'));
$hasApcu = extension_loaded('apcu') && in_array($apcuCliSetting, ['1', 'on', 'true', 'yes'], true);
$mustache = new Mustache();
$data = benchmarkData();
$coldCommand = $coldSamples > 0 ? coldWorkerCommand($hasApcu) : [];

$metadata = [
    'generated_at_utc' => gmdate(DATE_ATOM),
    'php_version' => PHP_VERSION,
    'php_sapi' => PHP_SAPI,
    'php_binary' => PHP_BINARY,
    'mustache_extension_version' => phpversion('mustache'),
    'libmustache_revision' => getenv('BENCH_LIBMUSTACHE_REVISION') ?: null,
    'php_mustache_revision' => getenv('BENCH_PHP_MUSTACHE_REVISION') ?: null,
    'samples' => $samples,
    'warmups' => $warmups,
    'cold_samples' => $coldSamples,
    'apcu' => $hasApcu ? phpversion('apcu') : null,
    'material_win_percent' => MATERIAL_WIN_PERCENT,
    'gc_during_timed_samples' => 'disabled',
    'cold_worker_model' => 'fresh PHP CLI process; startup and fixture loading excluded; APCu primed before first timed fetch',
];
$allResults = [];

printf("PHP %s, mustache %s, APCu %s, %d samples after %d warmups, %d cold workers\n", PHP_VERSION,
    (string) phpversion('mustache'), $hasApcu ? (string) phpversion('apcu') : 'disabled', $samples, $warmups,
    $coldSamples);
printf("%-13s %-25s %11s %11s %11s %12s\n", 'workload', 'case', 'median us', 'p95 us', 'ops/s', 'PHP peak B');

$workloads = [];
foreach (TARGETS as $size => $targetBytes) {
    $workloads[] = buildWorkload("{$size}-flat", $targetBytes, false);
    $workloads[] = buildWorkload("{$size}-graph", $targetBytes, true);
}

foreach ($workloads as $workload) {
    $name = $workload['name'];
    $sourceGraph = sourceObjectGraph($workload['root'], $workload['partials']);
    $astGraph = parseGraph($mustache, $workload['root'], $workload['partials']);
    $encodedGraph = encodeGraph($astGraph);
    $decodedGraph = decodeGraph($encodedGraph);

    $sourceOutput = $mustache->render($sourceGraph['root'], $data, $sourceGraph['partials']);
    $astOutput = $mustache->render($decodedGraph['root'], $data, $decodedGraph['partials']);
    if ($sourceOutput !== $astOutput || $sourceOutput === '') {
        throw new RuntimeException("source and AST rendering differ for {$name}");
    }
    if (encodeGraph($decodedGraph) !== $encodedGraph) {
        throw new RuntimeException("AST bytes do not round-trip for {$name}");
    }

    $sourcePhpPayload = serialize($sourceGraph);
    $astPhpPayload = serialize($astGraph);
    $sourceRoundTrip = unserialize($sourcePhpPayload, ['allowed_classes' => true]);
    $astRoundTrip = unserialize($astPhpPayload, ['allowed_classes' => true]);
    if ($mustache->render($sourceRoundTrip['root'], $data, $sourceRoundTrip['partials']) !== $sourceOutput
        || $mustache->render($astRoundTrip['root'], $data, $astRoundTrip['partials']) !== $sourceOutput) {
        throw new RuntimeException("PHP cache payload changes rendering for {$name}");
    }

    $sourceKey = "mustache-bench-source-{$name}";
    $astKey = "mustache-bench-ast-{$name}";
    if ($hasApcu) {
        apcu_store($sourceKey, $sourcePhpPayload);
        apcu_store($astKey, $astPhpPayload);
        if (apcu_fetch($sourceKey) !== $sourcePhpPayload || apcu_fetch($astKey) !== $astPhpPayload) {
            throw new RuntimeException("APCu payload round-trip failed for {$name}");
        }
    }

    $coldFixturePath = writeColdFixture($sourcePhpPayload, $astPhpPayload, $data, $sourceOutput);

    $cases = [
        'parse_source_graph' => static function () use ($mustache, $workload): int {
            $graph = parseGraph($mustache, $workload['root'], $workload['partials']);
            return count($graph['partials']) + spl_object_id($graph['root']);
        },
        'decode_ast_graph' => static function () use ($encodedGraph): int {
            $graph = decodeGraph($encodedGraph);
            return count($graph['partials']) + spl_object_id($graph['root']);
        },
        'render_source_graph' => static function () use ($mustache, $sourceGraph, $data): int {
            return strlen($mustache->render($sourceGraph['root'], $data, $sourceGraph['partials']));
        },
        'compile_render_source_graph' => static function () use ($mustache, $workload, $data): int {
            $graph = sourceObjectGraph($workload['root'], $workload['partials']);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        },
        'decode_render_ast_graph' => static function () use ($mustache, $encodedGraph, $data): int {
            $graph = decodeGraph($encodedGraph);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        },
        'reuse_ast_render' => static function () use ($mustache, $astGraph, $data): int {
            return strlen($mustache->render($astGraph['root'], $data, $astGraph['partials']));
        },
        'php_cache_hit_source' => static function () use ($mustache, $sourcePhpPayload, $data): int {
            $graph = unserialize($sourcePhpPayload, ['allowed_classes' => true]);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        },
        'php_cache_hit_ast' => static function () use ($mustache, $astPhpPayload, $data): int {
            $graph = unserialize($astPhpPayload, ['allowed_classes' => true]);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        },
        'serialize_source_graph' => static fn (): string => serialize($sourceGraph),
        'serialize_ast_graph' => static fn (): string => serialize($astGraph),
    ];

    if ($hasApcu) {
        $cases['apcu_cache_hit_source'] = static function () use ($mustache, $sourceKey, $data): int {
            $graph = unserialize(apcu_fetch($sourceKey), ['allowed_classes' => true]);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        };
        $cases['apcu_cache_hit_ast'] = static function () use ($mustache, $astKey, $data): int {
            $graph = unserialize(apcu_fetch($astKey), ['allowed_classes' => true]);
            return strlen($mustache->render($graph['root'], $data, $graph['partials']));
        };
        $cases['apcu_store_source'] = static fn (): bool => apcu_store($sourceKey, serialize($sourceGraph));
        $cases['apcu_store_ast'] = static fn (): bool => apcu_store($astKey, serialize($astGraph));
    }

    $workloadResults = [
        'target_source_bytes' => $workload['target_source_bytes'],
        'source_bytes' => graphBytes($workload['root'], $workload['partials']),
        'ast_bytes' => graphBytes($encodedGraph['root'], $encodedGraph['partials']),
        'php_source_payload_bytes' => strlen($sourcePhpPayload),
        'php_ast_payload_bytes' => strlen($astPhpPayload),
        'rendered_bytes' => strlen($sourceOutput),
        'cases' => [],
    ];

    try {
        foreach ($cases as $case => $operation) {
            $result = measure($operation, $warmups, $samples);
            $workloadResults['cases'][$case] = $result;
            printResult($name, $case, $result);
        }

        if ($hasApcu
            && (apcu_fetch($sourceKey) !== serialize($sourceGraph)
                || apcu_fetch($astKey) !== serialize($astGraph))) {
            throw new RuntimeException("APCu serialized store changed its payload for {$name}");
        }

        if ($coldSamples > 0) {
            $coldCases = ['cold_php_cache_hit_source', 'cold_php_cache_hit_ast'];
            if ($hasApcu) {
                $coldCases[] = 'cold_apcu_cache_hit_source';
                $coldCases[] = 'cold_apcu_cache_hit_ast';
            }
            foreach ($coldCases as $case) {
                $result = measureColdWorkers($coldCommand, $coldFixturePath, $case, $coldSamples);
                $workloadResults['cases'][$case] = $result;
                printResult($name, $case, $result);
            }
        }
    } finally {
        @unlink($coldFixturePath);
    }
    $allResults[$name] = $workloadResults;

    if ($hasApcu) {
        apcu_delete($sourceKey);
        apcu_delete($astKey);
    }
}

$document = [
    'metadata' => $metadata,
    'workloads' => $allResults,
];
$jsonPath = getenv('BENCH_JSON');
if (is_string($jsonPath) && $jsonPath !== '') {
    file_put_contents($jsonPath, json_encode($document, JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR) . "\n");
}
