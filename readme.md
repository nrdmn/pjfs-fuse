# pjfs-fuse

## What is PJFS?

PJFS (Partitioning, Journaling File System) is a filesystem developed by Green Hills Software, Inc.
Features:
- Journaling
- Multiple volumes per file system
- Endianness support
- Support for different block sizes

[PJFS press release](https://www.ghs.com/news/20051114_pjfs.html)

## About this implementation

This is a very basic fuse implementation of PJFS.
Current limitations:
- Read-only
- No journal support
- Maximum file size 55296 bytes
- Fixed block size of 512 bytes
- Up to 4096 blocks per volume (2 MiB)
- Little endian support only
- No timestamps

## How PJFS works

### Virtual and physical blocks

PJFS has the concept of virtual block IDs.
It maps between actual memory offset (physical block ID) and volume-specific identifier per block.
Physical block IDs are relative to the whole PJFS, not to a volume.

### Items
All files and directories in a PJFS are items which share a common set of properties.

| Item type | Description |
| - | - |
| 1 | File |
| 2 | Directory |

### Item Key

An Item Key is the virtual block ID of the item's first block and its type.

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 4 | Virtual block ID |
| + 0x4 | 4 | Type |

### Range

A Range describes a range of numeric values including the first and excluding the last value.

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 4 | Begin of range (inclusive) |
| + 0x4 | 4 | End of range (exclusive) |

### Superblock

The Superblock contains information about the PJFS file system.
It is the first block of a PJFS file system.
It is 0x80 bytes large.

```
00000000  70 6a 66 73 5f 76 31 5f  6c 65 00 00 00 00 00 00  |pjfs_v1_le......|
00000010  00 08 08 04 40 40 40 00  01 00 00 00 80 00 00 00  |....@@@.........|
00000020  01 00 00 00 00 00 00 00  00 02 00 00 08 00 00 00  |................|
00000030  01 00 00 00 01 00 00 00  00 00 00 00 a8 01 00 00  |................|
00000040  a8 01 00 00 01 00 00 00  a8 01 00 00 a7 01 00 00  |................|
00000050  00 00 00 00 02 00 00 00  02 00 00 00 02 00 00 00  |................|
00000060  04 00 00 00 02 00 00 00  04 00 00 00 05 00 00 00  |................|
00000070  01 00 00 00 05 00 00 00  06 00 00 00 01 00 00 00  |................|
```

| Offset | Length | Description |
| - | - | - |
| 0x0 | 16 (?) | Magic string |
| 0x10 | 1 | Big endian |
| 0x11 | 1 | Size of disk size |
| 0x12 | 1 | Size of file size |
| 0x13 | 1 | Size of time |
| 0x14 | 1 | Size of directory header |
| 0x15 | 1 | Size of directory entry |
| 0x16 | 1 | Size of item |
| 0x17 | 1 | ? |
| 0x18 | 4 | Superblock version |
| 0x1c | 4 | Superblock size |
| 0x20 | 4 | FS version |
| 0x24 | 4 | FS flags |
| 0x28 | 4 | Block size |

### Volume table header

Directly followed by the superblock follows the volume table header, which is followed by table entries.

```
00000080  00 00 00 00 01 00 00 00  40 00 00 00 00 00 00 00  |........@.......|
00000090  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

| Offset | Length | Description |
| - | - | - |
| 0x80 | 4 | CRC32 from 0x84 over all volume table entries, or zero |
| 0x84 | 4 | Number of volumes |
| 0x88 | 4 | Size of one volume table entry (?) |

### Volume table entries

Every volume table entry is presumably 0x40 bytes large.

```
000000a0  76 6f 6c 30 00 00 00 00  00 00 00 00 00 00 00 00  |vol0............|
000000b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000c0  01 00 00 00 a8 01 00 00  a7 01 00 00 00 00 00 00  |................|
000000d0  5f 00 00 00 5f 00 00 00  5f 00 00 00 04 01 00 00  |_..._..._.......|
```

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 32 (?) | Volume name |
| + 0x20 | 8 | Range of the volume, in physical blocks |
| + 0x28 | 16 | ? |
| + 0x38 | 8 | "Slash dir key", the key of the root directory |

### A volume's first block

The first block of a volume contains a list of pointers to block allocation tables.
Every entry consists of the physical block ID of a block allocation table block and presumably flags.
A physical block ID of 0xffffffff marks the end of the list.

```
00000200  02 00 00 00 00 00 00 00  04 00 00 00 00 00 00 00  |................|
00000210  08 00 00 00 00 00 00 00  26 00 00 00 00 00 00 00  |........&.......|
00000220  48 00 00 00 00 00 00 00  07 00 00 00 00 00 00 00  |H...............|
00000230  81 01 00 00 11 00 00 00  ff ff ff ff 00 00 00 00  |................|
00000240  ff ff ff ff 00 00 00 00  ff ff ff ff 00 00 00 00  |................|
00000250  ff ff ff ff 00 00 00 00  ff ff ff ff 00 00 00 00  |................|
00000260  ff ff ff ff 00 00 00 00  ff ff ff ff 00 00 00 00  |................|
00000270  ff ff ff ff 00 00 00 00  ff ff ff ff 00 00 00 00  |................|
...
```

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 4 | Physical block ID |
| + 0x4 | 4 | Flags (?) |

### Block allocation table

A block allocation table contains a list of pointers to blocks.
Every entry consists of the physical block ID of the block being pointed to and flags.
A physical block ID of 0xffffffff marks the end of the list.
To look up a virtual block with ID n, take the nth entry of all block allocation table blocks.

The following example shows the second block allocation table block.
To look up the block with virtual ID 95 (0x5f in hex), you have to skip the first block allocation table block as every such block only contains up to 64 entries.
Then, in the second block allocation table block (shown below), look at the 95-64=31th entry at offset 0x8f8.
The block has the physical block ID 0x6e and has flags 0x104, i.e. you'll find it at offset 0x6e*512=56320.

```
00000800  33 01 00 00 02 00 00 00  34 01 00 00 02 00 00 00  |3.......4.......|
00000810  4c 01 00 00 02 00 00 00  5a 01 00 00 02 00 00 00  |L.......Z.......|
00000820  60 01 00 00 02 00 00 00  64 01 00 00 02 00 00 00  |`.......d.......|
00000830  6b 01 00 00 02 00 00 00  6d 01 00 00 02 00 00 00  |k.......m.......|
00000840  6f 01 00 00 02 00 00 00  52 00 00 00 02 00 00 00  |o.......R.......|
00000850  53 00 00 00 02 00 00 00  54 00 00 00 02 00 00 00  |S.......T.......|
00000860  55 00 00 00 02 00 00 00  56 00 00 00 02 00 00 00  |U.......V.......|
00000870  57 00 00 00 02 00 00 00  58 00 00 00 02 00 00 00  |W.......X.......|
00000880  59 00 00 00 02 00 00 00  5a 00 00 00 02 00 00 00  |Y.......Z.......|
00000890  5b 00 00 00 02 00 00 00  5c 00 00 00 02 00 00 00  |[.......\.......|
000008a0  5d 00 00 00 02 00 00 00  5e 00 00 00 02 00 00 00  |].......^.......|
000008b0  5f 00 00 00 02 00 00 00  60 00 00 00 02 00 00 00  |_.......`.......|
000008c0  61 00 00 00 02 00 00 00  62 00 00 00 02 00 00 00  |a.......b.......|
000008d0  63 00 00 00 02 00 00 00  64 00 00 00 02 00 00 00  |c.......d.......|
000008e0  65 00 00 00 02 00 00 00  66 00 00 00 02 00 00 00  |e.......f.......|
000008f0  67 00 00 00 02 00 00 00  6e 00 00 00 04 01 00 00  |g.......n.......|
00000900  87 01 00 00 04 01 00 00  5c 01 00 00 04 01 00 00  |........\.......|
00000910  3b 00 00 00 04 01 00 00  69 00 00 00 04 01 00 00  |;.......i.......|
00000920  0d 00 00 00 04 01 00 00  6b 00 00 00 04 01 00 00  |........k.......|
00000930  69 01 00 00 04 01 00 00  10 00 00 00 04 01 00 00  |i...............|
00000940  3e 00 00 00 06 01 00 00  41 00 00 00 06 01 00 00  |>.......A.......|
00000950  32 00 00 00 06 01 00 00  29 00 00 00 06 01 00 00  |2.......).......|
00000960  11 00 00 00 06 01 00 00  30 00 00 00 06 01 00 00  |........0.......|
00000970  45 00 00 00 06 01 00 00  2f 00 00 00 06 01 00 00  |E......./.......|
00000980  2e 00 00 00 06 01 00 00  13 00 00 00 06 01 00 00  |................|
00000990  47 01 00 00 06 01 00 00  15 00 00 00 06 01 00 00  |G...............|
000009a0  18 00 00 00 06 01 00 00  19 00 00 00 06 01 00 00  |................|
000009b0  1a 00 00 00 06 01 00 00  16 00 00 00 06 01 00 00  |................|
000009c0  1b 00 00 00 06 01 00 00  1d 00 00 00 06 01 00 00  |................|
000009d0  1e 00 00 00 06 01 00 00  1c 00 00 00 06 01 00 00  |................|
000009e0  1f 00 00 00 06 01 00 00  21 00 00 00 06 01 00 00  |........!.......|
000009f0  22 00 00 00 06 01 00 00  23 00 00 00 06 01 00 00  |".......#.......|
```

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 4 | Physical block ID |
| + 0x4 | 4 | Flags |

### Item header

An item may span over multiple blocks.
If offset + 0x4c is 0xffffffff, the item's contents directly follow at + 0x50.
If it is not, a list of virtual block IDs starting at + 0x4c point to the blocks containing the contents of the item.
The rest of the block is then filled with 0xff.

```
00007000  00 01 00 00 7e 01 00 00  04 01 00 00 62 00 00 00  |....~.......b...|
00007010  04 01 00 00 01 00 00 00  46 00 00 00 00 00 00 00  |................|
00007020  00 00 00 00 00 00 00 00  ff ff ff ff ff ff ff 7f  |................|
00007030  7e 02 00 00 7e 02 00 00  00 00 00 00 00 00 00 00  |~...~...........|
00007040  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  |................|
00007050  23 69 6e 63 6c 75 64 65  20 3c 73 74 64 69 6f 2e  |#include <stdio.|
00007060  68 3e 0a 0a 69 6e 74 20  6d 61 69 6e 28 29 0a 7b  |h>..int main().{|
00007070  0a 09 70 75 74 73 28 22  48 65 6c 6c 6f 2c 20 77  |..puts("Hello, w|
00007080  6f 72 6c 64 21 22 29 3b  0a 09 72 65 74 75 72 6e  |orld!");..return|
00007090  20 30 3b 0a 7d 0a 00 00  00 00 00 00 00 00 00 00  | 0;.}...........|
000070a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000070b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000070c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000070d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000070e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000070f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007100  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007110  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007120  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007130  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007140  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007150  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007160  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007170  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007180  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007190  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000071f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 1 | ? |
| + 0x1 | 1 | Type |
| + 0x2 | 2 | ? |
| + 0x4 | 8 | Item key of this item |
| + 0xc | 8 | Item key of the parent item |
| + 0x14 | 4 | ? |
| + 0x18 | 8 | Size |
| + 0x20 | 8 | ? |
| + 0x28 | 8 | ? |
| + 0x2c | 4 | ? |
| + 0x30 | 4 | ? |
| + 0x34 | 4 | ? |
| + 0x38 | 4 | ? |
| + 0x3c | 4 | ? |

### Directories

A directory is an item with type 2 and a special structure of contents.
The directory's first 0x40 bytes describe the number of directory entries.
The parent directory of a volume's root directory is the directory itself, i.e. the item's parent's key is the item's key.

```
00007600  00 02 00 00 62 00 00 00  04 01 00 00 5f 00 00 00  |....b......._...|
00007610  04 01 00 00 01 00 00 00  40 01 00 00 00 00 00 00  |........@.......|
00007620  00 00 00 00 00 00 00 00  ff ff ff ff ff ff ff 7f  |................|
00007630  4f 00 00 00 7e 02 00 00  00 00 00 00 00 00 00 00  |O...~...........|
00007640  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  |................|
00007650  04 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007660  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007670  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007680  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007690  63 00 00 00 04 01 00 00  02 00 00 00 00 00 00 00  |c...............|
000076a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000076b0  66 6f 6f 00 00 00 00 00  00 00 00 00 00 00 00 00  |foo.............|
000076c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000076d0  64 00 00 00 04 01 00 00  02 00 00 00 00 00 00 00  |d...............|
000076e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000076f0  62 61 72 00 00 00 00 00  00 00 00 00 00 00 00 00  |bar.............|
00007700  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007710  65 00 00 00 04 01 00 00  02 00 00 00 00 00 00 00  |e...............|
00007720  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007730  62 61 7a 00 00 00 00 00  00 00 00 00 00 00 00 00  |baz.............|
00007740  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007750  7e 01 00 00 04 01 00 00  01 00 00 00 00 00 00 00  |~...............|
00007760  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007770  71 75 75 78 00 00 00 00  00 00 00 00 00 00 00 00  |quux............|
00007780  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00007790  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000077f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

### Directory entry

| Offset | Length | Description |
| - | - | - |
| + 0x0 | 8 | Item key |
| + 0x8 | 1 | Type |
| + 0x9 | 23 | ? |
| + 0x20 | 32 | Name |
