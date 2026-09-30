<?php
header('Content-Type: text/html; charset=UTF-8');
header('Cache-Control: no-store');
set_time_limit(45);

function serverStatus(string $label, string $flag): void
{
    $program = '/opt/openshores/bin/AuServerStatus';
    $output = [];
    $exitCode = 1;

    if (is_executable($program)) {
        // Only the fixed flags below are accepted; no request data enters this command.
        exec('/usr/bin/timeout --kill-after=1s 12s ' . escapeshellarg($program)
            . ' ' . escapeshellarg($flag), $output, $exitCode);
    }

    if ($exitCode === 0 && count($output) > 0) {
        // AuServerStatus produces the HTML fragment, not HTTP headers.
        echo implode("\n", $output);
    } else {
        echo '<b>' . htmlspecialchars($label, ENT_QUOTES, 'UTF-8') . '</b><br>';
        echo ($exitCode === 124 || $exitCode === 137)
            ? 'Status request timed out.'
            : 'Status unavailable.';
        error_log('AuServerStatus ' . $flag . ' failed with exit code ' . $exitCode);
    }
}

function getVersion(string $flag)
    {
        $program = '/opt/openshores/bin/AuServerStatus';
        $output = [];
        $exitCode = 1;

        if (is_executable($program)) {
            // Only the fixed flags below are accepted; no request data enters this command.
            exec('/usr/bin/timeout --kill-after=1s 12s ' . escapeshellarg($program)
                . ' ' . escapeshellarg($flag), $output, $exitCode);
        }

        if ($exitCode === 0 && count($output) > 0) {
            // AuServerStatus produces the HTML fragment, not HTTP headers.
            echo implode("\n", $output);
        } else {
            echo "Version info failed to load!";
        }
    }
?>
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            background-color: #0B0B3B;
            color: white;
            text-shadow: 2px 2px black;
        }
        #statusdiv {
            border: 1px solid grey;
            max-width: 240px;
            padding: 5px;
        }
        #links {
            padding: 5px;
            border: 1px solid grey;
            max-width: 240px;
            margin-top: 10px;
        }
        a:visited {
            color: #6078ac;
        }
    </style>
    <title>OpenShores</title>
</head>
<body>
    <main>
        <h1>OpenShores</h1>
        <h2>Server status</h2>
        <p>Reload this page to check again.</p>
        <div id="statusdiv">
            <section><?php serverStatus('Login Servers', '-l'); ?></section>
            <hr>
            <section><?php serverStatus('Mail Servers', '-m'); ?></section>
            <hr>
            <section><?php serverStatus('Scene Servers', '-g'); ?></section>
        </div>
        <div id="links">
            <a href="/register">Register account</a><br>
            <a href="/reset">Reset password</a><br>
            <a href="/emailchange">Change E-mail address</a><br>
            <a href="/passchange">Change password</a><br>
        </div>
        <p><?php getVersion('-v'); ?></p>
    </main>
</body>
</html>
