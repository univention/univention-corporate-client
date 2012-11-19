#!/bin/sh
set -e
aclocal
autoconf
automake -ac
