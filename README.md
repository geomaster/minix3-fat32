# MINIX FAT32 service

This is a fork of the MINIX 3.3.0 source with an implementation for `fat32`, a
service that allows other processes ask about info for FAT32 partitions and read
their contents. This is **not** a filesystem driver, and you cannot mount FAT32
partitions with it. A userspace tool for communicating with the service is
provided.

## Repo contents

Inside this repo is a snapshot of the `/usr/src/minix` tree with the FAT32
service added inside `/usr/src/minix/servers/fat32` and added to all the necessary
other files.

## Compiling

Ensure that the source for your Minix system is the same as the one this fork is
based on. You can check the initial commit for differences. Then, copy the
contents of the `usr/src/minix` directory in the repo to your own
`/usr/src/minix/` directory. To build everything, `cd` to `/usr/src/releasetools`
and run `make hdboot`. This will build your boot images in `/boot`. Reboot the
system and run `ps ax | grep fat32` to ensure `fat32` is running (its pid should
be `13`).

## Usage

This server is able to lets you traverse a FAT32 partition and read files off of
it. It includes support for long filename entries and should hopefully be able to
read any valid FAT32 filesystem. A big unsupported feature are wide-character
filenames: the filenames are simply truncated to ASCII. The API is lower-level
than a filesystem driver, and there isn't a libc-level API. Instead, user
programs must make direct syscalls. Fortunately, I have provided a C++11 wrapper
around this API inside `fatori`, that you can use verbatim or copy.

### API

`fatori/fat32.cpp` contains the code for the wrapper, so you can take a look
there to see what it actually does. You can take a look at the public interface
inside `fatori/fat32.hpp`. The C++ API has the following classes:

* `maybe<T>`. Utility class that can either hold a value of type `T` or
  "nothing". This is returned by API calls that can return either some result or
  "no more results". If `is_some` is `false`, the `maybe` holds "nothing". If
  `is_some` is true, the `maybe` holds "something", which is stored inside
  `maybe.value`.
* `fs`. You construct an object of this type directly. The only parameter to the
  constructor is the path to a block device or file where the filesystem resides.
  (Be sure to give the block device of the partition, not of the whole drive, as
  `fat32` cannot read the partition headers).
* `dir`. Represents a FAT32 directory. You get an object of this type by calling
  `fs.open_root_dir()`, which gives you the root directory of the partition, or
  `dir.open_subdir()`, which opens a subdirectory of this directory. The
  directory allows you to read the next entry in it by calling
  `dir.next_entry()`, which returns information about the next file/directory in
  the given directory (if there's no more, it returns a `maybe<entry>` with
  `is_some` set to `false`). This advances the cursor. If the returned entry is a
  directory, you can immediately call `dir.open_subdir()`, which will return a
  `dir` object representing the directory that corresponds to the entry that was
  just read.
* `file`. If the last entry read from a directory was a file, calling
  `dir.open_file()` will return a `file` object for you to work with,
  corresponding to the file that was just read as an entry. You have only one
  function of interest: `file.read_block()` allocates a cluster-sized buffer and
  reads the next cluster of the file. It returns a `maybe<vector<uint8_t>>` with
  `is_some` set to `false` if you've reached the end of the file. Otherwise, the
  contents are returned.

The API is fully RAII and properly throws exceptions if any operation fails.

### fatori

`fatori` is a small user-space program which allows you to inspect the contents
of a FAT32 partition easily. Run `./fatori <fat32-block-device-or-file>`. You
should get a prompt. Following are the commands `fatori` accepts. All paths are
relative to the partition root.

* `ls /path/to/dir`. Shows the contents of a directory.
* `tree /path/to/dir`. Recursively shows the contents of a directory in a
  tree-like format.
* `cat /path/to/file`. Prints the contents of a given file.
* `stat /path/to/file-or-dir`. Shows available information about a given file or
  directory.
* `exit`.

The code uses the aforementioned C++ API and the source code lives at
`fatori/fatori.cpp`, so you can take a look at how the API is used.

## License

The MINIX code contained in this repo is copyrighted by The MINIX project and
release under the MINIX license. I claim no ownership of it.

My own code on top of the MINIX source code is also released under the same MINIX
license (clone of the BSD license).
