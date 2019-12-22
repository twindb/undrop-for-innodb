#!/usr/bin/env bash
set -ex

cd /undrop-for-innodb/omnibus-undrop-for-innodb

bundle install --binstubs

bin/omnibus build undrop-for-innodb
