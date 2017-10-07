# BHFS - Black Hole Filesytem

A "naive" write-only filesystem based on FUSE and GnuPG.

## Table of Contents

1. [Description](#description)
1. [Disclaimer](#disclaimer)
1. [Why naive?](#why-naive)
1. [Use cases](#use-cases)
1. [Important limitations](#important-limitations)
1. [Requirements and installation](#requirements-and-installation)
1. [Usage](#usage)
   1. [Configure GnuPG](#configure-gnupg)
   1. [Mount the filesystem](#mount-the-filesystem)
1. [Troubleshooting](#troubleshooting)
1. [License](#license)

## Description

BHFS is a filesystem which encrypts files on-the-fly using asymmetric cryptography. Files are encrypted one-way using a public key. Once written, files can be read only by the owner of the corresponding private key.

The system that wrote the files cannot decrypt them. This means that in the event of a compromise of the system, files written before the compromise would not be accessible to the attacker.

BHFS is based on [FUSE](https://github.com/libfuse/libfuse) and behind the scenes uses [GnuPG](https://www.gnupg.org/) to provide encryption.

## Disclaimer

This is a proof of concept and the code is nowhere near production ready. 
Use at your own risk!

Side effects may include loss of data, security vulnerabilities and incompatibility with future versions.

## Why naive?
Because of some not-so-elegant design choices and shortcuts I took to quickly get to a working and usable software. There's room for improvement.

## Use cases

I developed it for a couple of home projects but the applications could be wider. 
The advantage of using a filesystem is that it's easy to retrofit an existing solution in a transparent way.

### Security camera
For example I'm using it on a basic [Raspberry Pi](https://www.raspberrypi.org/) security camera. Videos are encrypted as they are written to local storage and then uploaded to cloud storage. Should the camera be compromised, either physically (stolen) or via the network, previously recorded videos would remain private.

### Log files
Log files may end up containing sensitive information. They can be protected by writing them on a BHFS filesystem.

### Upload portal
This is a slightly more complex use case.
Imagine an internet-facing portal for the upload (via web or sftp) of sensitive files.
For example this could be documents requested by the HR department for pre-employments checks. The uploaded files could be stored in a BHFS filesystem. Only backend systems or HR staff that require access to the data would have the private key to decrypt the documents. This would mitigate the impact of a vulnerability in the upload portal.

## Important limitations

Not all applications are suitable for use with BHFS. 
Once the data is written to a file, the system can't read it back. For this reason files behave like write-only streams and the following limitations apply:
* Applications can only create new files and write to them by appending data. Attempts to append data to existing files will result in a new file being created which overwrites the original one;
* Seek operations are not permitted, data is always appended;
* Read operations always return the encrypted version of the data (this behavior might change in future versions and return permission denied for read operations).

BHFS is suitable for:
* Saving archives (tar zcvf ...);
* Saving videos while recording;
* Storing files using copy and move operations;

BHFS is not suitable for:
* Storing cache files;
* Applications that read/write to files in sparse chunks (e.g. bittorrent).

Please note that file names are not encrypted, only the contents are.

## Requirements and installation

See the [INSTALL.md](INSTALL.md) file.

## Usage

### Configure GnuPG

This section assumes you already have a GPG key pair and you have exported the public key for use with BHFS on a different system.  

Import the public key (public_key.asc) into the GnuPG public key ring on the system where you want to use BHFS. The user needs to be the same user that will mount the filesystem:

```
$ gpg --import public_key.asc
gpg: /var/lib/motion/.gnupg/trustdb.gpg: trustdb created
gpg: key AAAAAAAA: public key "My Test Key <bhfs@example.com>" imported
gpg: Total number processed: 1
gpg:               imported: 1  (RSA: 1)
```

Mark the key as trusted (replace AAAAAAAA with the identifier from the previous step):

```
$ gpg --edit-key AAAAAAAA trust quit
[...]
Please decide how far you trust this user to correctly verify other users' keys
(by looking at passports, checking fingerprints from different sources, etc.)

  1 = I don't know or won't say
  2 = I do NOT trust
  3 = I trust marginally
  4 = I trust fully
  5 = I trust ultimately
  m = back to the main menu

Your decision? 5
Do you really want to set this key to ultimate trust? (y/N) y 
[...]
```

[Optional] Disable compression for better performance. On slow systems, such as the Raspberry Pi, this considerably increases performance. 
This can be done by adding the following line to ``~/.gnupg/gpg.conf``. 

```
compress-level 0
```

### Mount the filesystem

Mount the filesystem using the following command:

```
bhfs -r bhfs@example.com /mnt/rootdir/ /mnt/mountdir/
```

Where:
* bhfs@example.com is the recipient, i.e. the address of the public key imported earlier on. The key must be accessible by the user issuing the mount command;
* /mnt/rootdir/ is the **full path** where the encrypted files will be stored;
* /mnt/mountdir/ is the **full path** where the filesystem will be mounted.

Start writing to /tmp/mountdir/.

Files in /mnt/rootdir can be decrypted using the recipient's private key using the following command:

```
gpg --decrypt -o decrypted_filename encrypted_filename
```

## Troubleshooting

If something does not work:
* Make sure the gpg public key is available to the user issuing the mount command. This can be done with the following command:
```gpg --list-public-keys```
* Make sure the gpg public key is trusted. This can be done by checking that the output of the following command displays "sig 3" for the corresponding key:
```gpg --list-sigs```
* Make sure you are using absolute paths for the rootDir and mountDir when issuing the mount command.

## License

This program is distributed under the terms of the GNU GPL v2.0.
See the file [COPYING](COPYING).