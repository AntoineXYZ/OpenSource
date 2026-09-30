#!/usr/bin/env python3
"""Render environment-specific Compose settings and scene-worker services."""

from __future__ import annotations

import argparse
import ipaddress
import json
import os
import re
import secrets
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEPLOY_DIR = ROOT / "deploy" / "server"

COMMON_DEFAULTS = {
    "SERVER_BUILD_CONFIG": "debug",
    "SCENE_WORKERS": "1",
    "ATOM_MANAGERS": "1",
    "BIND_ADDRESS": "127.0.0.1",
    "PUBLIC_HOST": "127.0.0.1",
    "WEB_BIND_ADDRESS": "127.0.0.1",
    "WEB_PORT": "80",
    "LOGIN_WORKER_PORT": "16767",
    "MAIL_WORKER_PORT": "16768",
    "SCENE_MANAGER_PORT": "16769",
    "SCENE_WORKER_PORT_BASE": "16800",
    "COMPOSE_PROJECT_NAME": "openshores-local",
    "POSTGRES_VOLUME": "openshores-local-postgres",
    "POSTGRES_PASSWORD": "openshores-local-postgres",
    "AU_DB_OWNER_PASSWORD": "Light",
    "AU_DB_PASSWORD": "Gold",
    "SERVER_LOGIN_PASSWORD": "Au*Server",
    "INTERNAL_AUTH_TOKEN": "",
    "CREATE_LOCAL_PLAYER": "true",
    "LOCAL_PLAYER_NAME": "local",
    "LOCAL_PLAYER_PASSWORD": "local",
}

PRODUCTION_DEFAULTS = {
    "SERVER_BUILD_CONFIG": "release",
    "BIND_ADDRESS": "0.0.0.0",
    "PUBLIC_HOST": "",
    "WEB_BIND_ADDRESS": "0.0.0.0",
    "COMPOSE_PROJECT_NAME": "openshores-production",
    "POSTGRES_VOLUME": "openshores-production-postgres",
    "POSTGRES_PASSWORD": "",
    "AU_DB_OWNER_PASSWORD": "",
    "AU_DB_PASSWORD": "",
    "SERVER_LOGIN_PASSWORD": "",
    "INTERNAL_AUTH_TOKEN": "",
    "CREATE_LOCAL_PLAYER": "false",
    "LOCAL_PLAYER_NAME": "",
    "LOCAL_PLAYER_PASSWORD": "",
}


def read_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise SystemExit(f"{path}:{number}: expected NAME=value")
        name, value = line.split("=", 1)
        name = name.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name):
            raise SystemExit(f"{path}:{number}: invalid setting name {name!r}")
        values[name] = value
    return values


def integer(settings: dict[str, str], name: str, minimum: int, maximum: int) -> int:
    try:
        value = int(settings[name])
    except ValueError as error:
        raise SystemExit(f"{name} must be an integer") from error
    if value < minimum or value > maximum:
        raise SystemExit(f"{name} must be between {minimum} and {maximum}")
    return value


def boolean(settings: dict[str, str], name: str) -> bool:
    value = settings[name].lower()
    if value not in {"true", "false"}:
        raise SystemExit(f"{name} must be true or false")
    return value == "true"


def worker_account(index: int) -> str:
    # initlogin.sql contains five server accounts per unit through u12.
    unit = 2 + (index - 1) // 5
    slot = (index - 1) % 5
    return f"u{unit}s{slot}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", choices=("local", "production"), required=True)
    parser.add_argument("--env-file", type=Path)
    parser.add_argument("--scene-workers", type=int)
    parser.add_argument("--config", choices=("debug", "release", "relwithdebinfo"))
    args = parser.parse_args()

    environment = args.environment
    settings = dict(COMMON_DEFAULTS)
    if environment == "production":
        settings.update(PRODUCTION_DEFAULTS)

    default_env_path = DEPLOY_DIR / "environments" / f"{environment}.env"
    env_path = args.env_file or default_env_path
    if environment == "production" and not env_path.exists():
        raise SystemExit(
            f"missing production configuration: copy "
            f"{DEPLOY_DIR / 'production.env.example'} to {default_env_path}"
        )
    settings.update(read_env(env_path))
    for name in COMMON_DEFAULTS:
        if name in os.environ:
            settings[name] = os.environ[name]
    if args.scene_workers is not None:
        settings["SCENE_WORKERS"] = str(args.scene_workers)
    if args.config is not None:
        settings["SERVER_BUILD_CONFIG"] = args.config

    # Keep the local token stable across rebuilds without checking it into Git.
    if environment == "local" and not settings["INTERNAL_AUTH_TOKEN"]:
        previous = read_env(ROOT / "build" / "server" / "local" / "compose.env")
        settings["INTERNAL_AUTH_TOKEN"] = previous.get("INTERNAL_AUTH_TOKEN") or secrets.token_hex(32)
    if settings["INTERNAL_AUTH_TOKEN"] and not re.fullmatch(
        r"[A-Za-z0-9_-]{32,}", settings["INTERNAL_AUTH_TOKEN"]
    ):
        raise SystemExit("INTERNAL_AUTH_TOKEN must have at least 32 letters, digits, - or _")

    workers = integer(settings, "SCENE_WORKERS", 1, 50)
    atom_managers = integer(settings, "ATOM_MANAGERS", 1, 256)
    login_worker_port = integer(settings, "LOGIN_WORKER_PORT", 1024, 65535)
    mail_worker_port = integer(settings, "MAIL_WORKER_PORT", 1024, 65535)
    scene_manager_port = integer(settings, "SCENE_MANAGER_PORT", 1024, 65535)
    worker_port_base = integer(settings, "SCENE_WORKER_PORT_BASE", 1024, 65534)
    web_port = integer(settings, "WEB_PORT", 1, 65535)
    create_local_player = boolean(settings, "CREATE_LOCAL_PLAYER")

    config = settings["SERVER_BUILD_CONFIG"].lower()
    if config not in {"debug", "release", "relwithdebinfo"}:
        raise SystemExit("SERVER_BUILD_CONFIG must be debug, release, or relwithdebinfo")

    try:
        ipaddress.ip_address(settings["BIND_ADDRESS"])
    except ValueError as error:
        raise SystemExit("BIND_ADDRESS must be an IP address") from error

    try:
        web_bind = ipaddress.ip_address(settings["WEB_BIND_ADDRESS"])
    except ValueError as error:
        raise SystemExit("WEB_BIND_ADDRESS must be an IP address") from error

    public_host = settings["PUBLIC_HOST"].strip()
    if not public_host or not re.fullmatch(r"[A-Za-z0-9.-]+", public_host):
        raise SystemExit("PUBLIC_HOST must be a DNS name or IP address without a port")

    final_worker_port = worker_port_base + (workers - 1) * 2 + 1
    if final_worker_port > 65535:
        raise SystemExit("scene worker port range exceeds 65535")
    fixed_ports = {16757, 16758, 16759, login_worker_port, mail_worker_port, scene_manager_port}
    worker_ports = set(range(worker_port_base, final_worker_port + 1))
    if len(fixed_ports) != 6 or fixed_ports & worker_ports:
        raise SystemExit("configured public ports overlap")

    game_bind = ipaddress.ip_address(settings["BIND_ADDRESS"])
    if web_port in fixed_ports | worker_ports and (
        web_bind == game_bind or web_bind.is_unspecified or game_bind.is_unspecified
    ):
        raise SystemExit("WEB_PORT overlaps a game port on the selected bind addresses")

    if create_local_player and (
        not settings["LOCAL_PLAYER_NAME"] or not settings["LOCAL_PLAYER_PASSWORD"]
    ):
        raise SystemExit("local player name and password are required when CREATE_LOCAL_PLAYER=true")

    if environment == "production":
        required = (
            "POSTGRES_PASSWORD",
            "AU_DB_OWNER_PASSWORD",
            "AU_DB_PASSWORD",
            "SERVER_LOGIN_PASSWORD",
            "INTERNAL_AUTH_TOKEN",
        )
        missing = [name for name in required if not settings[name] or settings[name] == "CHANGE_ME"]
        if missing:
            raise SystemExit("production secrets are unset: " + ", ".join(missing))
        if public_host.endswith("example.com"):
            raise SystemExit("replace the example PUBLIC_HOST before production deployment")

    debug_enabled = environment == "local" and config == "debug"
    settings.update(
        {
            "DEPLOY_ENVIRONMENT": environment,
            "SERVER_BUILD_CONFIG": config,
            "SERVER_IMAGE_TARGET": "server-debug" if debug_enabled else "server-runtime",
            "SERVER_IMAGE": f"openshores-server:{environment}-{config}",
            "POSTGRES_IMAGE": f"openshores-postgres:{environment}",
            "WEB_IMAGE": f"openshores-web:{environment}",
            "WEB_PORT": str(web_port),
            "SCENE_WORKERS": str(workers),
            "ATOM_MANAGERS": str(atom_managers),
            "CREATE_LOCAL_PLAYER": "true" if create_local_player else "false",
        }
    )

    server_environment = {
        "AU_LOGIN_SERVER": "login-worker",
        "AU_MAIL_SERVER": "mail-worker",
        "AU_LOGIN_DB_HOST": "postgres",
        "AU_MAIL_DB_HOST": "postgres",
        "AU_UNIVERSE_DB_HOST": "postgres",
        "AU_DB_PASSWORD": settings["AU_DB_PASSWORD"],
        "AU_INTERNAL_TOKEN": settings["INTERNAL_AUTH_TOKEN"],
        "AU_PUBLIC_ENDPOINTS": "1",
    }
    services: dict[str, object] = {}
    for index in range(1, workers + 1):
        scene_port = worker_port_base + (index - 1) * 2
        chat_port = scene_port + 1
        worker: dict[str, object] = {
            "container_name": f"{settings['COMPOSE_PROJECT_NAME']}-scene-worker-{index}",
            "image": settings["SERVER_IMAGE"],
            "pull_policy": "never",
            "restart": "unless-stopped",
            "stop_grace_period": "60s",
            "working_dir": "/var/lib/openshores",
            "environment": server_environment,
            "command": [
                "/opt/openshores/bin/AuSceneServer",
                f"-n{public_host}",
                f"-p{scene_port}",
                f"-c{chat_port}",
                "-m16762",
                "-sscene-manager",
                f"-t{atom_managers}",
                f"-l{worker_account(index)}",
                f"-w{settings['SERVER_LOGIN_PASSWORD']}",
            ],
            "depends_on": {
                "postgres": {"condition": "service_healthy"},
                "login-worker": {"condition": "service_healthy"},
                "scene-manager": {"condition": "service_healthy"},
            },
            "ports": [
                f"{settings['BIND_ADDRESS']}:{scene_port}:{scene_port}/tcp",
                f"{settings['BIND_ADDRESS']}:{scene_port}:{scene_port}/udp",
                f"{settings['BIND_ADDRESS']}:{chat_port}:{chat_port}/tcp",
            ],
            "networks": ["server"],
            "healthcheck": {
                "test": [
                    "CMD",
                    "/opt/openshores/bin/health-port",
                    str(scene_port),
                    str(chat_port),
                ],
                "interval": "5s",
                "timeout": "3s",
                "retries": 30,
                "start_period": "10s",
            },
            "logging": {
                "driver": "local",
                "options": {"max-size": "20m", "max-file": "3"},
            },
        }
        if debug_enabled:
            worker["cap_add"] = ["SYS_PTRACE"]
            worker["security_opt"] = ["seccomp=unconfined"]
        services[f"scene-worker-{index}"] = worker

    output_dir = ROOT / "build" / "server" / environment
    output_dir.mkdir(parents=True, exist_ok=True)
    workers_path = output_dir / "workers.compose.json"
    workers_path.write_text(json.dumps({"services": services}, indent=2) + "\n", encoding="utf-8")

    generated_names = [
        "DEPLOY_ENVIRONMENT",
        "SERVER_IMAGE_TARGET",
        "SERVER_IMAGE",
        "POSTGRES_IMAGE",
        "WEB_IMAGE",
    ]
    env_order = list(COMMON_DEFAULTS) + generated_names
    env_lines = [f"{name}={settings[name]}" for name in env_order]
    (output_dir / "compose.env").write_text("\n".join(env_lines) + "\n", encoding="utf-8")

    print(f"{environment.capitalize()} server config: {workers} scene worker(s), {config}")
    print(f"Generated: {workers_path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
