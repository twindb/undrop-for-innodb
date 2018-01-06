class profile::base {
  $user = 'vagrant'

  user { $user:
    ensure => present
  }

  file { "/home/${user}":
    ensure => directory,
    owner  => $user,
    mode   => "0750"
  }

  file { "/home/${profile::base::user}/.bashrc":
    ensure => present,
    owner  => $profile::base::user,
    mode   => "0644",
    source => 'puppet:///modules/profile/bashrc',
  }

  file { '/root/.ssh':
    ensure => directory,
    owner => 'root',
    mode => '700'
  }

  file { '/root/.ssh/authorized_keys':
    ensure => present,
    owner => 'root',
    mode => '600',
    source => 'puppet:///modules/profile/id_rsa.pub'
  }

  file { '/root/.ssh/id_rsa':
    ensure => present,
    owner => 'root',
    mode => '600',
    source => 'puppet:///modules/profile/id_rsa'
  }

  file { "/home/${profile::base::user}/.my.cnf":
    ensure => present,
    owner  => $profile::base::user,
    mode   => "0600",
    content => "[client]
user=root
password=qwerty
"
  }

  file { "/root/.my.cnf":
    ensure => present,
    owner  => 'root',
    mode   => "0600",
    content => "[client]
user=root
password=qwerty
"
  }

  $packages = [ 'vim-enhanced', 'nmap-ncat',
    'Percona-Server-client-56', 'Percona-Server-server-56',
    'Percona-Server-devel-56', 'Percona-Server-shared-56', 'percona-toolkit',
    'gcc', 'flex', 'bison']

  package { $packages:
    ensure  => installed,
    require => [
      Yumrepo['Percona'],
      Package['epel-release']
    ]
  }

  yumrepo { 'Percona':
    baseurl  => 'http://repo.percona.com/centos/$releasever/os/$basearch/',
    enabled  => 1,
    gpgcheck => 0,
    descr    => 'Percona',
    retries  => 3
  }

  package { 'epel-release':
    ensure => installed
  }
}
