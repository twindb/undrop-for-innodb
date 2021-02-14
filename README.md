# TwinDB data recovery toolkit
## Supported Failures

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

Undrop for InnoDB overview with installation instructions and demo.
[![Undrop for InnoDB overview](https://img.youtube.com/vi/-1LeLhGjAWM/0.jpg)](https://www.youtube.com/watch?v=-1LeLhGjAWM)

The source code of the toolkit is hosted on GitHub. The tool has been developed on Linux, it’s known to work on CentOS 4,5,6,7, Debian, Ubuntu and Amazon Linux. Only 64 bit systems are supported.

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
