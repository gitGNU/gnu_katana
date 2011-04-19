#!/bin/sh
./run_tests.py -s real_tests/apache httpd '/-f' "`pwd`/real_tests/apache/httpd.conf" /-X
