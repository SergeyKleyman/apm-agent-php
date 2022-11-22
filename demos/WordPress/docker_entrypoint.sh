#!/usr/bin/env bash
set -xe

function ensureSyslogIsRunning () {
    if ps -ef | grep -v 'grep' | grep -q 'syslogd' ; then
        echo 'Syslog is already started.'
        return
    fi

    if which syslogd; then
        syslogd
    else
        if which rsyslogd; then
            rsyslogd
        else
            echo 'syslog is not installed'
            exit 1
        fi
    fi
}

ensureSyslogIsRunning

php-fpm
