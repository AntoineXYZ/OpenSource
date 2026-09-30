# Docker server deployment

The deployment runs every server process in its own container. The login, mail, and scene roles reuse one game-server image; PostgreSQL uses its own image and container.

This deployment targets one Docker host. Scene workers scale across containers on that host. Multi-host scheduling is not included.

## Topology

```text
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

`server-down` preserves the PostgreSQL volume. Delete and recreate the local universe only when intended:

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

- TCP `16757`, `16758`, and `16759` for the masters;
- the configured login-worker, mail-worker, and scene-manager TCP ports;
- each scene worker's TCP/UDP scene port and TCP chat port.

All public roles currently advertise the same `PUBLIC_HOST` with unique ports. `server-prod-down` preserves the production PostgreSQL volume. There is deliberately no production reset target.

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
stack while preserving the database. **The artifact contains production credentials.**

## Configuration model

Important settings:

| Setting | Purpose |
| --- | --- |
| `SERVER_BUILD_CONFIG` | `debug`, `release`, or `relwithdebinfo` |
| `SCENE_WORKERS` | Number of generated scene-worker containers |
| `ATOM_MANAGERS` | Atom-manager bundles per scene worker |
| `BIND_ADDRESS` | Host interface used for published ports |
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

## Logs and data

`make server-logs` and `make server-prod-logs` use Docker Compose logs. Each service uses bounded Docker local logs: three files of up to 20 MiB.

PostgreSQL state lives in the named volume selected by `POSTGRES_VOLUME`. Back up that volume before upgrades or production topology changes.
