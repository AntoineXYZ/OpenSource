<?php
header('Content-Type: text/html; charset=UTF-8');
header('Cache-Control: no-store');
set_time_limit(20);

$output = [];
$exitCode = 1;
$avatars = null;
$program = '/opt/openshores/bin/AuServerStatus';
if (is_executable($program)) {
    exec('/usr/bin/timeout --kill-after=1s 12s ' . escapeshellarg($program)
        . ' -p', $output, $exitCode);
}
if ($exitCode === 0) {
    $decoded = json_decode(implode("\n", $output));
    if (is_array($decoded)) {
        $avatars = $decoded;
        foreach ($avatars as $avatar) {
            if (!is_object($avatar) || !isset($avatar->name, $avatar->afk)
                || !is_string($avatar->name) || !is_bool($avatar->afk)) {
                $avatars = null;
                break;
            }
        }
    }
}
if ($avatars === null) {
    http_response_code(503);
    error_log('Online avatar query failed; AuServerStatus exit code ' . $exitCode);
}
?>
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Avatars online - OpenShores</title>
    <style>
        body { background: #0B0B3B; color: white; font-family: sans-serif; }
        a { color: #a0b3f9; }
    </style>
</head>
<body>
    <main>
        <h1>Avatars online</h1>
        <?php if ($avatars === null): ?>
            <p>The online avatar list is unavailable. Please try again shortly.</p>
        <?php elseif (count($avatars) === 0): ?>
            <p>No avatars are currently online.</p>
        <?php else: ?>
            <p><?= count($avatars) ?> <?= count($avatars) === 1 ? 'avatar is' : 'avatars are' ?> currently online.</p>
            <ul>
                <?php foreach ($avatars as $avatar): ?>
                    <li><?= htmlspecialchars($avatar->name, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?><?= $avatar->afk ? ' (AFK)' : '' ?></li>
                <?php endforeach; ?>
            </ul>
        <?php endif; ?>
        <p>Reload this page to check again.</p>
        <p><a href="/">Home</a></p>
    </main>
</body>
</html>
