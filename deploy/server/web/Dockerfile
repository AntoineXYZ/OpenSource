FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        apache2 ca-certificates curl php-cgi libqt5gui5 libqt5network5 \
    && rm -rf /var/lib/apt/lists/* \
    && a2enmod cgid actions env \
    && a2disconf serve-cgi-bin \
    && a2dissite 000-default \
    && install -d /opt/openshores/www /opt/openshores/cgi-bin /opt/openshores/bin \
    && ln -s /usr/bin/php-cgi /opt/openshores/cgi-bin/php \
    && install -d -o 1000 -g 1000 /var/lib/openshores/reports

COPY openshores.conf /etc/apache2/sites-available/openshores.conf
COPY index.php /opt/openshores/www/index.php
COPY playerson.php /opt/openshores/www/playerson.php
COPY register.html /opt/openshores/www/register.html
COPY emailchange.html passchange.html /opt/openshores/www/
COPY reset.html /opt/openshores/www/reset.html
COPY health.cgi /opt/openshores/cgi-bin/health.cgi
COPY entrypoint.sh /usr/local/bin/openshores-web

RUN chmod 0755 /usr/local/bin/openshores-web /opt/openshores/cgi-bin/health.cgi \
    && a2ensite openshores \
    && php-cgi -l /opt/openshores/www/index.php \
    && php-cgi -l /opt/openshores/www/playerson.php \
    && apache2ctl -t

EXPOSE 80
STOPSIGNAL SIGWINCH
ENTRYPOINT ["/usr/local/bin/openshores-web"]
