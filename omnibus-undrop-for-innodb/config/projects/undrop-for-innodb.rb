#
# Copyright 2019 TwinDB LLC
#
# All Rights Reserved.
#

name "undrop-for-innodb"
maintainer 'TwinDB Development Team <dev@twindb.com>'
homepage 'https://github.com/twindb'

# Defaults to C:/undrop-for-innodb on Windows
# and /opt/undrop-for-innodb on all other platforms
install_dir "#{default_root}/#{name}"

build_version '2.0.0'
build_iteration 1

# Creates required build directories
dependency "preparation"

# undrop-for-innodb dependencies/components
dependency 'undrop-for-innodb'


exclude "**/.git"
exclude "**/bundler/git"
