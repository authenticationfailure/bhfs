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

The system that wrote the files cannot decrypt them (assuming the private key is not stored on the same system). This means that in the event of a compromise of the system, files written before the compromise would not be accessible to the attacker.

BHFS now ships with the companion WHFS (White Hole Filesystem) to easily mount the encrypted files for reading on a trusted system, where the private key is stored.

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

## Important limitations and known issues

Not all applications are suitable for use with BHFS. 
Once the data is written to a file, the system can't read it back. For this reason files behave like write-only streams and the following limitations apply:
* Applications can only create new files and write to them by appending data. Attempts to append data to existing files will result in a new file being created which overwrites the original one;
* Seek operations are not permitted, data is always appended;
* Read operations always return the encrypted version of the data (this behavior might change in future versions and return permission denied for read operations).

BHFS is suitable for:
* Saving archives (e.g: ``tar zcvf ...``);
* Saving videos while recording;
* Storing files using copy and move operations;

BHFS is not suitable for:
* Storing cache files;
* Applications that read/write to files in sparse chunks (e.g. bittorrent).

Please note that file names are not encrypted, only the contents are.

WHFS is suitable for:
* Accessing files using the copy and move operations to other locations;
* Extracting archives directly from the filesystem (e.g: ``tar zxvf ...``);
* Depending on the player, play video files directly from the filesystem;
* Reorganising data using operations such as delete, mkdir, move, etc... 

WHFS is not suitable for:
* Applications that access files using seek operations
* Some video players and seek operations when playing files

Currently, the file sizes reported by WHFS are the sizes of the encrypted versions of the files, which can cause issues with some applications, such as using ``tar zcvf ...``.  Compressing files directly from WHFS using tar causes data corruption because tar appends extra zero bytes at the end of the file. When this happens the following error is displayed:

```
tar: <filename>: File shrank by NNN bytes; padding with zeros
```

## Requirements and installation

See the [INSTALL.md](INSTALL.md) file.

## Usage

BHFS and WHFS are intended to be used on two different types of systems:

 * BHFS is executed on the system(s) that produce the files. Let's call a system of this type 'Ben'.
 * WHFS is executed on trusted system(s) that need to access the decrypted files. Let's call a system of this type 'Whitney'.

For BHFS to be effective in limiting the impact of a compromise of Ben, the private key **must not** be stored on or accessible from Ben.
In order to decrypt the files, Whitney must have access to the private key corresponding to the public key used by Ben.

### Configure GnuPG for BHFS on Ben

This section assumes you already have a GPG key pair (for example generated on Whitney) and you have exported the public key for use with BHFS on Ben.

Import the public key (public_key.asc) into the GnuPG public key ring on Ben. The user importing the key has to be the same user that will mount the filesystem:

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

### Mount the BHFS filesystem on Ben

Now, on Ben mount the filesystem using the following command:

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

### Mount the WHFS filesystem on Whitney

On Whitney, ensure that the private key for bhfs@example.com is accessible. The key can be password protected when using gpg together with gpg-agent; however this is a more complex use case than using an unprotected key.

To mount the WHFS filesystem on Whitney use the following command:

```
whfs /mnt/rootdir/ /mnt/mountdir/
```

* /mnt/rootdir/ is the **full path** where the encrypted files are stored;
* /mnt/mountdir/ is the **full path** where the filesystem exposing the decrypted files will be mounted.

Although changes to the contents of individual files is not permitted, WHFS is not a read only filesystem as it conviniently supports some operations such as:
 
* File deletion
* Creation of directories and links
* File move operations 

If needed, the filesystem can be mounted as read only with the option ``-o ro``:

```
whfs -o ro /mnt/rootdir/ /mnt/mountdir/
```

## Troubleshooting

If something does not work for BHFS:
* Make sure the gpg public key is available to the user issuing the mount command. This can be done with the following command:
```gpg --list-public-keys```
* Make sure the gpg public key is trusted. This can be done by checking that the output of the following command displays "sig 3" for the corresponding key:
```gpg --list-sigs```
* Make sure you are using absolute paths for the rootDir and mountDir when issuing the mount command.

If something does not work for WHFS:
* Make sure the gpg private key is available to the user issuing the mount command. This can be done with the following command:
```gpg --list-secret-keys```
* Make sure the gpg private key matches the public key used for encryption.
* If the gpg key is password protected, make sure you are using gpg-agent and that the key is unlocked ([configure a suitable password caching timeout](https://superuser.com/questions/624343/keep-gnupg-credentials-cached-for-entire-user-session)). Your system (e.g. Ubuntu), may as well prompt you for the password via the GUI upon file access. 
* Make sure you are using absolute paths for the rootDir and mountDir when issuing the mount command.

## License

This program is distributed under the terms of the GNU GPL v2.0.
See the file [COPYING](COPYING).