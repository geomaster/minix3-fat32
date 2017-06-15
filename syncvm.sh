#!/usr/bin/env bash
# Syncs the source here with the source inside the MINIX VM via SSH, runs `make
# hdboot` in `/usr/src/minix/servers/ls`, which creates the boot image, and
# reboots the machine if all went well. Great for fast iteration.
rsync -zvar -e ssh usr/src/minix/servers/fat32/ minix:/usr/src/minix/servers/fat32
ssh minix 'cd /usr/src/minix/servers/fat32 && make hdboot'
