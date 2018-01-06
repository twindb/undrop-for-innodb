class profile::master {

  include profile::base


  file { '/etc/my.cnf':
    ensure  => present,
    owner   => 'mysql',
    source  => 'puppet:///modules/profile/my-master.cnf',
    notify  => Service['mysql'],
    require => Package['Percona-Server-server-56']
  }

  file { "/home/${profile::base::user}/mysql_grants.sql":
    ensure => present,
    owner  => $profile::base::user,
    mode   => "0400",
    source => 'puppet:///modules/profile/mysql_grants.sql',
  }

  exec { 'Create MySQL users':
    path    => '/usr/bin:/usr/sbin',
    user    => $profile::base::user,
    command => "mysql -u root < /home/${$profile::base::user}/mysql_grants.sql",
    require => [
      Service['mysql'],
      File["/home/${profile::base::user}/mysql_grants.sql"]
    ],
    before  => File["/home/${profile::base::user}/.my.cnf"],
    unless  => 'mysql -e "SHOW GRANTS FOR dba@localhost"'
  }

  service { 'mysql':
    ensure  => running,
    enable  => true,
    require => Package['Percona-Server-server-56'],
  }
}
