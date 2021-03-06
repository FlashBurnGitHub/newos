/*
** Copyright 2002-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NFS_FS_H
#define _NFS_FS_H

/* NFS v2 stuff */
/* From RFC 1094 */

enum nfs_stat {
	NFS_OK = 0,
	NFSERR_PERM=1,
	NFSERR_NOENT=2,
	NFSERR_IO=5,
	NFSERR_NXIO=6,
	NFSERR_ACCES=13,
	NFSERR_EXIST=17,
	NFSERR_NODEV=19,
	NFSERR_NOTDIR=20,
	NFSERR_ISDIR=21,
	NFSERR_FBIG=27,
	NFSERR_NOSPC=28,
	NFSERR_ROFS=30,
	NFSERR_NAMETOOLONG=63,
	NFSERR_NOTEMPTY=66,
	NFSERR_DQUOT=69,
	NFSERR_STALE=70,
	NFSERR_WFLUSH=99
};

enum nfs_ftype {
	NFNON = 0,
	NFREG = 1,
	NFDIR = 2,
	NFBLK = 3,
	NFCHR = 4,
	NFLNK = 5
};

enum nfs_proc {
	NFSPROC_NULL = 0,
	NFSPROC_GETATTR,
	NFSPROC_SETATTR,
	NFSPROC_ROOT,
	NFSPROC_LOOKUP,
	NFSPROC_READLINK,
	NFSPROC_READ,
	NFSPROC_WRITECACHE,
	NFSPROC_WRITE,
	NFSPROC_CREATE,
	NFSPROC_REMOVE,
	NFSPROC_RENAME,
	NFSPROC_LINK,
	NFSPROC_SYMLINK,
	NFSPROC_MKDIR,
	NFSPROC_RMDIR,
	NFSPROC_READDIR,
	NFSPROC_STATFS
};

#define NFSPROG 100003
#define NFSVERS 2

enum mount_proc {
	MOUNTPROC_NULL,
	MOUNTPROC_MNT,
	MOUNTPROC_DUMP,
	MOUNTPROC_UMNT,
	MOUNTPROC_UMNTTALL,
	MOUNTPROC_EXPORT
};

#define MOUNTPROG 100005
#define MOUNTVERS 1

/*
* The maximum number of bytes of data in a READ or WRITE
* request.
*/
#define MAXDATA 8192

/* The maximum number of bytes in a pathname argument. */
#define MAXPATHLEN 1024

/* The maximum number of bytes in a file name argument. */
#define MAXNAMLEN 255

/* The size in bytes of the opaque "cookie" passed by READDIR. */
#define COOKIESIZE 4

/* The size in bytes of the opaque file handle. */
#define FHSIZE 32

/* The maximum number of bytes in a pathname argument. */
#define MNTPATHLEN 1024

/* The maximum number of bytes in a name argument. */
#define MNTNAMLEN 255

/* nfs structures
 * NOTE: They don't necessarilly match what is on the wire precisely.
 * Routines exist to pack/unpack data
 */

typedef struct {
	const char *dirpath;
} nfs_mountargs;
#define NFS_MOUNTPATH_MAXLEN (ROUNDUP(MNTPATHLEN, 4) + 4)
#define NFS_MOUNTARGS_MAXLEN NFS_MOUNTPATH_MAXLEN

size_t nfs_pack_mountargs(uint8 *buf, const nfs_mountargs *args);

typedef int nfs_status;

typedef struct {
	nfs_status status;
	unsigned int tsize;
	unsigned int bsize;
	unsigned int blocks;
	unsigned int bfree;
	unsigned int bavail;
} nfs_statfsres;

typedef unsigned char nfs_fhandle[FHSIZE];

typedef struct {
	const char *name;
} nfs_filename;
#define NFS_FILENAME_MAXLEN (ROUNDUP(MAXNAMLEN, 4) + 4)

size_t nfs_pack_filename(uint8 *buf, const nfs_filename *name);

typedef struct {
	unsigned int seconds;
	unsigned int useconds;
} nfs_timeval;

typedef struct {
	int ftype;
	unsigned int mode;
	unsigned int nlink;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	unsigned int blocksize;
	unsigned int rdev;
	unsigned int blocks;
	unsigned int fsid;
	unsigned int fileid;
	nfs_timeval  atime;
	nfs_timeval  mtime;
	nfs_timeval  ctime;
} nfs_fattr;

typedef struct {
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	nfs_timeval  atime;
	nfs_timeval  mtime;
} nfs_sattr;

typedef struct {
	nfs_fhandle *file;
	unsigned int offset;
	unsigned int count;
	unsigned int totalcount;
} nfs_readargs;
#define NFS_READARGS_MAXLEN (FHSIZE + 3 * 4)
size_t nfs_pack_readargs(uint8 *buf, const nfs_readargs *args);

typedef struct {
	nfs_status	status;
	nfs_fattr 	*attributes;
	unsigned int len;
	unsigned char *data;
} nfs_readres;
#define NFS_READRES_MAXLEN (4 + sizeof(nfs_fattr) + 4)
void nfs_unpack_readres(uint8 *buf, nfs_readres *res);

typedef struct {
	nfs_fhandle file;
	unsigned int beginoffset;
	unsigned int offset;
	unsigned int totalcount;
	unsigned char data[0];
} nfs_writeargs;

typedef struct {
	nfs_status status;
	nfs_fattr *attributes;
} nfs_attrstat;
#define NFS_ATTRSTAT_MAXLEN (4 + sizeof(nfs_fattr))
void nfs_unpack_attrstat(uint8 *buf, nfs_attrstat *res);

typedef struct {
	nfs_fhandle *dir;
	nfs_filename name;
} nfs_diropargs;
#define NFS_DIROPARGS_MAXLEN (FHSIZE + NFS_FILENAME_MAXLEN)
size_t nfs_pack_diropargs(uint8 *buf, const nfs_diropargs *args);

typedef struct {
	int status;
	nfs_fhandle *file;
	nfs_fattr *attributes;
} nfs_diropres;
#define NFS_DIROPRES_MAXLEN (4 + FHSIZE + sizeof(nfs_fattr))
void nfs_unpack_diropres(uint8 *buf, nfs_diropres *res);

typedef struct {
	nfs_fhandle *dir;
	unsigned int cookie;
	unsigned int count;
} nfs_readdirargs;
#define NFS_READDIRARGS_MAXLEN (FHSIZE + 4 + 4)
size_t nfs_pack_readdirargs(uint8 *buf, const nfs_readdirargs *args);

typedef struct {
	nfs_status status;
	unsigned int data[0];
} nfs_readdirres;

typedef struct {
	nfs_diropargs where;
	nfs_sattr attributes;
} nfs_createargs;
#define NFS_CREATEARGS_MAXLEN (NFS_DIROPARGS_MAXLEN + sizeof(nfs_sattr))
size_t nfs_pack_createopargs(uint8 *buf, const nfs_createargs *args);

typedef struct {
	nfs_diropargs from;
	nfs_diropargs to;
} nfs_renameargs;

typedef struct {
	nfs_fhandle from;
	nfs_diropargs to;
} nfs_linkargs;

#endif
