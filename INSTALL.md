# Prerequisites

* Linux or Mac OS X
* FUSE >= 2.6 (Tested with FUSE 2.9.3 and OSXFUSE 3.6.3)
* GnuPG (Tested with version 2.2.0 and 1.4.18)
* Autotools, Autoconf, Pkg-Config
* GCC or LLVM

## On Debian/Ubuntu

On Debian/Ubuntu install the packages required for compilation with:

```
# apt install build-essential fuse libfuse-dev autotools-dev autoconf pkg-config
```

# Installation from sources

To compile BHFS run the following commands:

```
$ ./autogen.sh
$ ./configure
$ make
```

Then install the BHFS binary as root with the following command: 

```
# make install
```

# Uninstallation

Uninstall with:

```
# make uninstall
```