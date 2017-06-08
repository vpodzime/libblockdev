### CI status

<img alt="CI status" src="https://fedorapeople.org/groups/storage_apis/statuses/libblockdev-master.svg" width="100%" height="200ex" />


### Introduction

libblockdev is a C library supporting GObject introspection for manipulation of
block devices. It has a plugin-based architecture where each technology (like
LVM, Btrfs, MD RAID, Swap,...) is implemented in a separate plugin, possibly
with multiple implementations (e.g. using LVM CLI or the new LVM DBus API).

For more information about the library's architecture see the specs.rst
file. For more infomation about the expected functionality and features see the
features.rst file. For more information about the schedule see the roadmap.rst
file.

For information about development and contributing guidelines see the
README.DEVEL.rst file.

For more information about the API see the generated documentation at
http://rhinstaller.github.io/libblockdev/.