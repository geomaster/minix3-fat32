# MINIX FAT32 service

This is a fork of the MINIX 3.3.0 source with an implementation for `fat32`, a
service that allows other processes ask about info for FAT32 partitions and read
their contents. This is **not** a filesystem driver, and you cannot mount FAT32
partitions with it. A userspace tool for communicating with the service is
provided.

## Repo contents

Inside this repo is a snapshot of the `/usr/src/minix` tree with the FAT32
service added inside `/usr/src/minix/servers/ls` and added to all the necessary
other files.

## Compiling

Ensure that the source for your Minix system is the same as the one this fork is
based on. You can check the initial commit for differences. Then, copy the
contents of the usr/src/minix directory to your own `/usr/src/minix/directory`. To
build everything, `cd` to `/usr/src/releasetools` and run `make hdboot`. This will
build your boot images in `/boot`. Reboot the system and run `ps ax | grep fat32` to
ensure `fat32` is running (its pid should be `13`).

## Usage

TODO.

## License

The MINIX code contained in this repo is copyrighted by The MINIX project and
release under the MINIX license. I claim no ownership of it.

My own code on top of the MINIX source code is also released under the same MINIX
license (clone of the BSD license).
