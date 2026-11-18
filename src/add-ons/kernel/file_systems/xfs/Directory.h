/*
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_


#include "Inode.h"


// Enum values to check which directory we are reading
enum DirectoryType {
	XFS_DIR_SHORT,
	XFS_DIR_BLOCK,
	XFS_DIR_LEAF,
	XFS_DIR_NODE,
	XFS_DIR_INVALID,
};


#define XFS_DIR2_DATA_ALIGN_LOG 3
#define XFS_DIR2_SPACE_SIZE \
	(1ULL << (32 + XFS_DIR2_DATA_ALIGN_LOG))

// Directory address space divided into sections, spaces seperated by 32GB
#define XFS_DIR2_DATA_SPACE 0
#define XFS_DIR2_DATA_OFFSET (XFS_DIR2_DATA_SPACE * XFS_DIR2_SPACE_SIZE)
#define XFS_DIR2_LEAF_SPACE 1
#define XFS_DIR2_LEAF_OFFSET (XFS_DIR2_LEAF_SPACE * XFS_DIR2_SPACE_SIZE)
#define XFS_DIR2_FREE_SPACE 2
#define XFS_DIR2_FREE_OFFSET (XFS_DIR2_FREE_SPACE * XFS_DIR2_SPACE_SIZE)


/*
 * This class should act as a layer between any kind of directory
 * and the kernel interface
 */
class DirectoryIterator {
public:
			virtual						~DirectoryIterator()							=	0;

			virtual	status_t			GetNext(char* name, size_t* length,
											xfs_ino_t* ino)								=	0;

			virtual	status_t			Lookup(const char* name, size_t length,
											xfs_ino_t* id)								=	0;

			virtual status_t			Rewind()										=	0;

			static DirectoryType		GetDirectoryType(Inode* inode);

			static DirectoryIterator*	Init(Inode* inode);
};


#endif
