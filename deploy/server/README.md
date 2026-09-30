# Docker server deployment

The deployment runs every server process in its own container. The login, mail, and scene roles reuse one game-server image; PostgreSQL and Apache each use their own image and container.

This deployment targets one Docker host. Scene workers scale across containers on that host. Multi-host scheduling is not included.

## Topology

```text
web (Apache + mod_cgid)
postgres
login-master
login-worker
mail-master
mail-worker
scene-master
scene-manager
scene-worker-1 ... scene-worker-N
```

Containers use Docker service names for private traffic. Native clients use the configured public host and published ports. PostgreSQL is never published.

Server compilation reuses the repository's existing named Docker volumes (`hazeron-linux-<config>-cache`). The build exports `build/linux-<config>/bin` and `lib`; the deployment then packages those artifacts into the runtime image. There is no second CMake cache inside the image build.

The image build uses a unique temporary context under `build/server/` and removes it when the script exits, including on failure. Set `HAZERON_KEEP_STAGING=1` to retain the context for inspection; its path is printed when the script exits.

## Local development

Start one Debug scene worker:

```bash
make server-up
```

Common commands:

```bash
make server-status
make server-logs
make server-logs SERVICE=scene-worker-1
make server-down
```

`server-down` preserves the PostgreSQL and web report volumes. Delete and recreate the local universe and web reports only when intended:

```bash
make server-reset
# Non-interactive: make server-reset CONFIRM=yes
```

Scale the scene-worker pool:

```bash
make server-up SCENE_WORKERS=3
```

Local defaults are in `environments/local.env.example`. Copy it to the ignored `environments/local.env` only when overrides are needed:

```bash
cp deploy/server/environments/local.env.example \
   deploy/server/environments/local.env
```

The local database creates this development account:

```text
account:  local
password: local
```

Change the credentials in `local.env` before the first initialization, or run `make server-reset` after changing them.

### Apache web service

The `web` service installs Apache 2 with the `event` MPM and `mod_cgid` in a
separate Ubuntu image. Its PHP homepage executes `AuServerStatus` with `-l`,
`-m`, and `-g` on each page load and embeds the resulting HTML fragments.
The scene status uses the management channel, which recognizes the database states.
The tool connects to the masters by service name on the private Docker network.
Apache runs PHP through `mod_actions` and `mod_cgid`, keeping the `event` MPM;
`AuServerStatus` remains a command-line program. `/register` serves the static
`register.html` form, which submits to `/cgi-bin/AuAcctNew`. The CGI handler
creates an account through the deployment's `login-worker` and returns its
existing success or error page. Apache passes `AU_LOGIN_SERVER`,
`AU_ACCOUNT_REGISTRATION=1`, and `AU_WEBMASTER_PASSWORD` (from
`SERVER_LOGIN_PASSWORD`) to this handler. The password must match the Webmaster
account in the login database, as initialized by the PostgreSQL setup script.
Outside Docker, the tool also accepts `AU_LOGIN_PORT` (default 16757);
registration uses `AuAcctNew.ini` unless `AU_ACCOUNT_REGISTRATION` is supplied.

`/reset` currently displays an unavailable notice. `AuAcctSendPassword` is the
forgotten-password tool; `AuAcctPassword` changes a password using the current
password. The reset tool is deliberately not exposed yet: the login server
changes the password before invoking `mail`, and email delivery is not configured
in the server image.

`/emailchange` and `/passchange` serve static forms that POST to `AuAcctEmail`
and `AuAcctPassword`, respectively. Both verify the account's current password
against the deployment's `login-worker`; they do not need Webmaster credentials
or email delivery. The handlers return their existing result pages. The legacy
change protocol does not acknowledge database updates, so verify changes by
logging in with the new password or inspecting the account record.

For this testing setup, build the CGI tools first, then build and start Apache
without rebuilding or restarting the game servers:

```bash
make linux-cgi CONFIG=debug
make server-web-up
make server-logs SERVICE=web
```

Open `http://localhost/`. `/index.html` redirects to the PHP homepage.
The page refreshes its results when reloaded; it does not poll in the background.
Each status command has a 12-second timeout, after which the page shows an error
for that service. Command failures also go to the Apache error log.

The `AuServerStatus`, `AuAcctNew`, `AuAcctEmail`, and `AuAcctPassword` executables are mounted read-only from
`build/linux-<SERVER_BUILD_CONFIG>/bin/`; the Qt runtime libraries
are installed in the web image. Match `CONFIG` to the deployment's
`SERVER_BUILD_CONFIG` (`debug` locally and `release` in production by default).
After rebuilding the executable, rerun the web-up target. It recreates the web
container so Docker remounts the new file if the linker replaced it.

The normal `make server-up` also includes Apache, but does not compile these tools.
For production, use `make server-prod-web-up` or the full `make server-prod-up`.
Build `make linux-cgi CONFIG=release` first for a production source checkout.
The production archive's `./start.sh` builds the bundled web image along with
the PostgreSQL image; CI includes all four Release executables in that archive.

The avatar count links to `/playerson.php`, which runs `AuServerStatus -p` to
query the scene master for a fresh JSON list. PHP renders the avatar names and
AFK markers; failures display an unavailable message rather than an empty list.
`/playerson.html` redirects to this page. No player report file or history is
written, and the homepage no longer makes the additional player-list query.

The status output retains its legacy timezone labels. The scene query still
attempts legacy galactic-chat report writes; those paths are not provisioned by
this setup. The persistent reports volume is not yet connected to these pages.

`WEB_PORT` defaults to `80`. `WEB_BIND_ADDRESS` defaults to `127.0.0.1` locally
and `0.0.0.0` in production, independently of the game's `BIND_ADDRESS`.
For example, a host already running a reverse proxy can set:

```dotenv
WEB_BIND_ADDRESS=127.0.0.1
WEB_PORT=8080
```

Set these in the environment's `.env` file, then rerun its web-up target.
For simultaneous local and production stacks on the same host, assign distinct
web ports or bind addresses. Apache currently serves HTTP; a host reverse proxy
can provide HTTPS.

Docker checks health by requesting `/cgi-bin/health.cgi` from inside the
container and checking its response. This verifies CGI execution as well as
HTTP availability. The health endpoint accepts only container-local requests;
the homepage remains accessible through the published web port.

Apache configuration, static site files, and CGI executables live in the image.
Generated reports have their own named volume,
`${COMPOSE_PROJECT_NAME}-web-reports`, mounted at
`/var/lib/openshores/reports` and served under `/reports/`. Apache mounts it
read-only; a future report generator can mount the same volume read-write.
New volumes are initialized with owner UID/GID `1000:1000`, matching the game
image's `openshores` user. Directory listings and script execution are disabled
for reports, and PHP/script files are denied. Legacy PHP report output will need
adaptation when report generation is integrated.

The report volume survives image rebuilds, container replacement, and normal
`server-down`/`server-prod-down`. `server-reset` and Compose `down --volumes`
delete it along with the database. Back up reports separately from PostgreSQL.
Apache access/error logs go to Docker's bounded logs rather than this volume.

### Connect a local client

Linux:

```bash
(cd build/linux-debug/bin && ./AuSceneClient -alocalhost -log)
```

Windows:

```powershell
Push-Location .\build\windows-debug\bin
.\AuSceneClient.exe -alocalhost -log
Pop-Location
```

`-a` points the login, mail, and scene masters at `localhost`. Their fixed ports are `16757`, `16758`, and `16759`. Redirected workers advertise `127.0.0.1` and their configured published ports.

VS Code includes local Debug client launches for Linux and Windows. Launch targets do not start or restart the stack; run `make server-up` first. The Linux-only `Linux Server: Attach scene-worker-1` launch attaches GDB to the first scene worker. The local Debug worker receives ptrace permissions; production workers do not.

### Windows clients with Docker Engine inside WSL 2

When Docker Engine runs directly inside WSL 2 in NAT mode, Windows `localhost`
forwarding handles TCP but does not forward the game's UDP controls. Login and
scene loading can work while attacking and switching held items fail. Movement
can still appear to work because the client also processes it locally.

Check the networking mode with `wslinfo --networking-mode`. For NAT mode, run
`hostname -I` inside the WSL distribution hosting Docker and note its WSL IPv4
address (usually the first address, not a Docker bridge address). Set both values
in `deploy/server/environments/local.env` to that address, preserving any other
overrides. For example:

```dotenv
BIND_ADDRESS=172.18.91.213
PUBLIC_HOST=172.18.91.213
```

Run `make server-up` to apply the settings, then reconnect the Windows client
using that address in the connection target field or with `-a172.18.91.213`.
Use the actual WSL address in place of the example. Set the same address in the
VS Code launch arguments when debugging. WSL's address can change after a WSL
restart; update both settings and restart the stack when it does.

This publishes the ports on the WSL interface and advertises an address Windows
can reach over both TCP and UDP. Changing only the client connection target is
insufficient because the servers redirect the client to `PUBLIC_HOST`.

See Microsoft's [WSL localhost implementation notes](https://github.com/microsoft/WSL/blob/master/doc/docs/technical-documentation/localhost.md)
for the TCP forwarding behavior in NAT mode.

## Single-host production

### Host configuration

Copy the production template and set the public host and every secret:

```bash
cp deploy/server/environments/production.env.example \
   deploy/server/environments/production.env
$EDITOR deploy/server/environments/production.env
```

The renderer rejects template hostnames and unset secrets. Validate without starting services:

```bash
make server-prod-config
```

Start and operate production:

```bash
make server-prod-up
make server-prod-status
make server-prod-logs
make server-prod-down
```

Scale during deployment:

```bash
make server-prod-up SCENE_WORKERS=8
```

The public firewall must permit:

- the configured web TCP port (`80` by default) when serving HTTP directly;
- TCP `16757`, `16758`, and `16759` for the masters;
- the configured login-worker, mail-worker, and scene-manager TCP ports;
- each scene worker's TCP/UDP scene port and TCP chat port.

All public game roles currently advertise the same `PUBLIC_HOST` with unique ports. `server-prod-down` preserves the production PostgreSQL and web report volumes. There is deliberately no production reset target.

The original protocol does not provide modern transport security. Public deployment still requires host hardening, firewall policy, monitoring, backups, and an explicit assessment of the legacy authentication protocol.

### GitHub Actions image publishing

Pushes to `main` publish `ghcr.io/<username>/openshores-server` with `latest` and
commit SHA tags, plus a deployment artifact named `server-production-compose-<commit SHA>`.

Set these repository Actions secrets:

- `GHCR_USERNAME`: registry username.
- `GHCR_TOKEN`: token with permission to publish the image.
- `SERVER_PRODUCTION_ENV`: production `.env` contents; use
  [`production.env.example`](environments/production.env.example) as a template.

Download the artifact from the workflow run. On a host with Docker Engine and
Docker Compose installed, extract its tar into a dedicated directory and start:

```bash
tar -xf server-production-compose.tar
./start.sh
```

For private images, run `docker login ghcr.io` first. Run `./stop.sh` to stop the
stack while preserving the database and web reports. **The artifact contains production credentials.**

## Configuration model

Important settings:

| Setting | Purpose |
| --- | --- |
| `SERVER_BUILD_CONFIG` | `debug`, `release`, or `relwithdebinfo` |
| `SCENE_WORKERS` | Number of generated scene-worker containers |
| `ATOM_MANAGERS` | Atom-manager bundles per scene worker |
| `BIND_ADDRESS` | Host interface used for published game ports |
| `WEB_BIND_ADDRESS` | Host interface used for HTTP; independent of game binding |
| `WEB_PORT` | Published HTTP port, default `80` |
| `INTERNAL_AUTH_TOKEN` | Shared secret for login worker registration and scene server avatar events; required in production and generated locally when omitted |
| `PUBLIC_HOST` | Host/IP placed in client redirect packets |
| `*_PORT` | Published role and worker ports |
| `POSTGRES_VOLUME` | Persistent database volume name |
| `AU_DB_PASSWORD` | Runtime database-user password |
| `SERVER_LOGIN_PASSWORD` | Password used by scene server accounts |

For production, generate `INTERNAL_AUTH_TOKEN` with `python3 -c 'import secrets; print(secrets.token_hex(32))'` and keep it in the private environment file.
The login master, login worker, and every scene server must receive the same token as `AU_INTERNAL_TOKEN`.

Generated Compose environment and worker services live under `build/server/local/` or `build/server/production/`.

Application defaults remain compatible with the original deployment. Docker supplies private login, mail, and database endpoints through environment variables; workers separately report the configured public host and port to clients.

### Scene-master INI file

The scene master reads an editable host file mounted read-only at
`/etc/openshores/scene-master.ini`:

| Stack | Host file | Initial setting |
| --- | --- | --- |
| Local | `deploy/server/environments/local.ini` | `galaxyGen=1galaxy` |
| Production | `deploy/server/environments/production.ini` | `galaxyGen=all` |

For a fresh universe containing only Shores of Hazeron, set:

```ini
[Settings]
galaxyGen=1galaxy
```

Use `galaxyGen=all` to generate every galaxy. Existing galaxies are retained;
changing the setting does not reduce an existing universe to one galaxy.
Managers and workers load the galaxies from the database and do not need this INI.

Compose passes `=/etc/openshores/scene-master` to `AuSceneServer`; the application
appends `.ini`. The mount requires the host file to exist. The INI is read at
startup, and changes take effect after recreating the scene-master container.
The first deployment of this feature also requires an updated server image;
run `make server-up` locally or `make server-prod-up` for production to build and
apply the changes. Subsequent INI-only edits do not require an image rebuild.

These INI files are maintained directly, not generated by `configure.py`.
Keep deployment credentials and network settings in the existing `.env` files:
INI settings override command-line arguments when both specify the same key.
The production deployment archive includes `deploy/server/environments/production.ini`,
which can be edited after extraction and before running `./start.sh`.

## Logs and data

`make server-logs` and `make server-prod-logs` use Docker Compose logs. Each service uses bounded Docker local logs: three files of up to 20 MiB.

PostgreSQL state lives in the named volume selected by `POSTGRES_VOLUME`. Back up that volume before upgrades or production topology changes.

Generated web reports live in `${COMPOSE_PROJECT_NAME}-web-reports`. Both data
volumes are preserved by normal shutdown commands.
