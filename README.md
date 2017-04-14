# STB UPGRADE

> upgrade STB via USB or Serial port or Network

# How to generate the target file

## target_file_image :


file_header | partition_section_header * num | payloads

## check_sum
simple_hash() handle the datas and write it to partition_section_header.So the target machine run it and check the value to protect datas.


# USB

## auto mount

use `mdev -s` in Linux and write "/sbin/stbhotplug" to "/proc/sys/kernel/hotplug" to listen USB events

## promise
the dev_name (like mtd0...) shouldn't be error.  
the name of target file is fixed under the USB root dir
