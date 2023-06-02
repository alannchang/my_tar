#include <stdio.h>

#define BLOCKSIZE 512

typedef struct 
{                              /* byte offset */
  char name[100];               /*   0 */ // name of file
  char mode[8];                 /* 100 */ // file permission mode
  char uid[8];                  /* 108 */ // numeric user ID of file owner
  char gid[8];                  /* 116 */ // numeric group ID of file group
  char size[12];                /* 124 */ // size of file in bytes (octal)
  char mtime[12];               /* 136 */ // last modification time (octal)
  char chksum[8];               /* 148 */ // checksum of header block (octal)
  char typeflag;                /* 156 */ // type of file being archived
  char linkname[100];           /* 157 */ // if file is symbolic link, name of linked file
  char magic[6];                /* 257 */ // UStar indicator ("ustar" followed by null byte)
  char version[2];              /* 263 */ // UStar version ("00")
  char uname[32];               /* 265 */ // user name of file owner
  char gname[32];               /* 297 */ // group name of file owner
  char devmajor[8];             /* 329 */ // if device file, major device number
  char devminor[8];             /* 337 */ // if device file, minor device number
  char prefix[155];             /* 345 */ // prefix of file name if exceeds 100 bytes
  char pad[12];                 /* 500 */ // padding to bring total to 512
} tar_header;