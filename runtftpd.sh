#!/bin/bash
su -c '/usr/sbin/in.tftpd -l -s /ixivix/ixivix/ -u nobody $*'
echo "TFTP running"

