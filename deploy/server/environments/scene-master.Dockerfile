ARG SERVER_IMAGE=openshores-server:local-debug
FROM ${SERVER_IMAGE}

ARG DEPLOY_ENVIRONMENT
COPY ${DEPLOY_ENVIRONMENT}.ini /etc/openshores/scene-master.ini

COPY shutdown.conf /var/lib/openshores/shutdown.conf