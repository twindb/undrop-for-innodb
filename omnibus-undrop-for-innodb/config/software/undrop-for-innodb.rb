#
# Copyright 2019 TwinDB LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# These options are required for all software definitions
name 'undrop-for-innodb'

license 'Apache-2.0'
license_file 'LICENSE'
skip_transitive_dependency_licensing true


source path: '/undrop-for-innodb'
relative_path 'undrop-for-innodb'

build do
  # Setup a default environment from Omnibus - you should use this Omnibus
  # helper everywhere. It will become the default in the future.
  env = with_standard_compiler_flags(with_embedded_path)

  # Manipulate any configure flags you wish:
  #   For some reason zlib needs this flag on solaris
  # env["CFLAGS"] << " -DNO_VIZ" if solaris?

  # "command" is part of the build DSL. There are a number of handy options
  # available, such as "copy", "sync", "ruby", etc. For a complete list, please
  # consult the Omnibus gem documentation.
  #
  # "install_dir" is exposed and refers to the top-level projects +install_dir+
  # command "./configure" \
  #         " --prefix=#{install_dir}/embedded", env: env

  # You can have multiple steps - they are executed in the order in which they
  # are read.
  #
  # "workers" is a DSL method that returns the most suitable number of
  # builders for the currently running system.
  command 'make', env: env
  command "make BINDIR=#{install_dir}/embedded/bin install", env: env
end
