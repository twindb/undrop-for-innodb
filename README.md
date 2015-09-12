# TwinDB data recovery toolkit
## Supported Failures

[![Join the chat at https://gitter.im/twindb/undrop-for-innodb](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/twindb/undrop-for-innodb?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
TwinDB Data Recovery Toolkit is a set of tools that operate with MySQL files at low level and allow to recover InnoDB databases after different failure scenarios.

The toolkit is also known as **UnDrop for InnoDB**, which is more accurate name because the toolkit works with InnoDB tables.

The tool recovers data when backups are not available. It supports recovery from following failures:

- A table or database was dropped.
- InnoDB table space corruption.
- Hard disk failure.
- File system corruption.
- Records were deleted from a table.
- A table was truncated.
- InnoDB files were accidentally deleted.
- A table was dropped and created empty one.
## Installation
The source code of the toolkit is hosted on LaunchPad. The tool has been developed on Linux, it’s known to work on CentOS 4,5,6,7, Debian, Ubuntu and Amazon Linux. Only 64 bit systems are supported.

To best way to get the source code is to clone it from GitHub.
```
git clone https://github.com/twindb/undrop-for-innodb.git
```

### Prerequisites

The toolkit needs `make`, `gcc`, `flex` and `bison` to compile.

### Compilation

To build the toolkit run make in the source code root:
```
# make
```
## Usage
There is a series of blog posts that describe in great details recovery from different failures.

 * [Recover Table Structure From InnoDB Dictionary](https://twindb.com/recover-table-structure-from-innodb-dictionary/) – how to generate CREATE TABLE statement if you have ibdata1 file.
 * [Take image from corrupted hard drive](https://twindb.com/take-image-from-corrupted-hard-drive/) – what you should do if a hard disk is dying.
 * [Recover Corrupt MySQL Database](https://twindb.com/recover-corrupt-mysql-database/) – how to recover database from corrupt InnoDB tablespace. The same approach can be taken to recover from corrupt file system.
 * [Recover after DROP TABLE. Case 2](https://twindb.com/recover-after-drop-table-innodb_file_per_table-on/) – how to recover InnoDB table if it was dropped and innodb_file_per_table was ON (a separate .ibd file per table).
 * [Recover after DROP TABLE. Case 1](https://twindb.com/recover-innodb-table-after-drop-table-innodb/) – how to recover InnoDB table if it was dropped and innodb_file_per_table was OFF (all tables are in ibadat1 file).
 * [Recover InnoDB dictionary](https://twindb.com/how-to-recover-innodb-dictionary/) – how to recover and read InnoDB dictionary tables.
 * [UnDROP tool for InnoDB](https://twindb.com/undrop-tool-for-innodb/) – describes tools of the toolkit, their usage, command line options.
 * [InnoDB dictionary](https://twindb.com/innodb-dictionary/) – describes InnoDB dictionary, its tables and format.

## Commercial Support and Data Recovery Service

You can recover your database using the toolkit and detailed instructions from the blog posts. 
We can help you to do it much faster.

[Aleksandr Kuzminsky](https://www.linkedin.com/in/akuzminsky) is an author of TwinDB data recovery toolkit and its forks. 
Since 2008 he dealt with many hundreds recovery cases.

Check [MySQL Data Recovery Service](https://twindb.com/mysql-data-recovery/) to learn more about prices, prerequisites and contact details.
