SERVER_CONFIG ?=
SCENE_WORKERS ?=
SERVER_LOG_TAIL ?= 200
SERVER_PRODUCTION_ENV ?= deploy/server/environments/production.env

LOCAL_SERVER_DIR := build/server/local
LOCAL_SERVER_ENV := $(LOCAL_SERVER_DIR)/compose.env
LOCAL_SERVER_WORKERS := $(LOCAL_SERVER_DIR)/workers.compose.json
PRODUCTION_SERVER_DIR := build/server/production
PRODUCTION_SERVER_ENV := $(PRODUCTION_SERVER_DIR)/compose.env
PRODUCTION_SERVER_WORKERS := $(PRODUCTION_SERVER_DIR)/workers.compose.json
LOCAL_COMPOSE = BUILDX_NO_DEFAULT_ATTESTATIONS=1 docker compose \
	--env-file "$(LOCAL_SERVER_ENV)" \
	-f deploy/server/compose.yaml -f "$(LOCAL_SERVER_WORKERS)"
PRODUCTION_COMPOSE = BUILDX_NO_DEFAULT_ATTESTATIONS=1 docker compose \
	--env-file "$(PRODUCTION_SERVER_ENV)" \
	-f deploy/server/compose.yaml -f "$(PRODUCTION_SERVER_WORKERS)"

.PHONY: server-config server-ensure-config server-image server-up server-down \
        server-web-up server-logs server-status server-reset \
        server-prod-config server-prod-ensure-config server-prod-image \
        server-prod-up server-prod-web-up server-prod-down server-prod-logs server-prod-status

server-config:
	python3 deploy/server/configure.py --environment local \
		$(if $(strip $(SERVER_CONFIG)),--config "$(SERVER_CONFIG)") \
		$(if $(strip $(SCENE_WORKERS)),--scene-workers "$(SCENE_WORKERS)")

server-ensure-config:
	@if [ ! -f "$(LOCAL_SERVER_ENV)" ] || [ ! -f "$(LOCAL_SERVER_WORKERS)" ]; then \
		python3 deploy/server/configure.py --environment local; \
	fi

server-image: server-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	image=$$(awk -F= '$$1 == "SERVER_IMAGE" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	target=$$(awk -F= '$$1 == "SERVER_IMAGE_TARGET" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	bash deploy/server/image/build.sh "$$config" "$$image" "$$target" "$(LINUX_IMAGE)"

server-up: server-image
	$(LOCAL_COMPOSE) up --build --detach --remove-orphans --wait

server-web-up: server-config
	$(LOCAL_COMPOSE) up --build --detach --wait --no-deps --force-recreate web

server-down: server-ensure-config
	$(LOCAL_COMPOSE) down --remove-orphans

server-logs: server-ensure-config
	$(LOCAL_COMPOSE) logs --follow --tail="$(SERVER_LOG_TAIL)" $(SERVICE)

server-status: server-ensure-config
	$(LOCAL_COMPOSE) ps

server-reset: server-image
	@if [ "$(CONFIRM)" != "yes" ]; then \
		printf 'Delete the local universe database and web reports, then restart? [y/N] '; \
		read answer; \
		case "$$answer" in y|Y|yes|YES) ;; *) echo 'Cancelled.'; exit 1;; esac; \
	fi
	$(LOCAL_COMPOSE) down --volumes --remove-orphans
	$(LOCAL_COMPOSE) up --build --detach --remove-orphans --wait

server-prod-config:
	python3 deploy/server/configure.py --environment production \
		--env-file "$(SERVER_PRODUCTION_ENV)" \
		$(if $(strip $(SERVER_CONFIG)),--config "$(SERVER_CONFIG)") \
		$(if $(strip $(SCENE_WORKERS)),--scene-workers "$(SCENE_WORKERS)")

server-prod-ensure-config:
	@if [ ! -f "$(PRODUCTION_SERVER_ENV)" ] || [ ! -f "$(PRODUCTION_SERVER_WORKERS)" ]; then \
		python3 deploy/server/configure.py --environment production \
			--env-file "$(SERVER_PRODUCTION_ENV)"; \
	fi

server-prod-image: server-prod-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	image=$$(awk -F= '$$1 == "SERVER_IMAGE" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	target=$$(awk -F= '$$1 == "SERVER_IMAGE_TARGET" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	bash deploy/server/image/build.sh "$$config" "$$image" "$$target" "$(LINUX_IMAGE)"

server-prod-up: server-prod-image
	$(PRODUCTION_COMPOSE) up --build --detach --remove-orphans --wait

server-prod-web-up: server-prod-config
	$(PRODUCTION_COMPOSE) up --build --detach --wait --no-deps --force-recreate web

server-prod-down: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) down --remove-orphans

server-prod-logs: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) logs --follow --tail="$(SERVER_LOG_TAIL)" $(SERVICE)

server-prod-status: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) ps
