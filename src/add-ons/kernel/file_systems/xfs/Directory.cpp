/*
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BPlusTree.h"
#include "Directory.h"
#include "BlockDirectory.h"
#include "LeafDirectory.h"
#include "NodeDirectory.h"
#include "ShortDirectory.h"


DirectoryIterator::~DirectoryIterator()
{
}


DirectoryType
DirectoryIterator::GetDirectoryType(Inode* inode)
{
	// If inode format is local all entries exist inside inode itself hence local format
	if (inode->Format() == XFS_DINODE_FMT_LOCAL)
		return XFS_DIR_SHORT;

	/*
	 * We get the last logical file offset from extent tree and 
	 * compare it with last logical offset of all types of directories.
	 */
	xfs_fileoff_t lastOffset = inode->GetDataExtent()->GetLastOffset();
	xfs_fsblock_t dirBlockCount = 1 << (inode->GetVolume()->DirBlockLog());
	uint32 blockSize = inode->BlockSize();
	if (lastOffset == -1ULL)
		return XFS_DIR_INVALID;

	uint64 dataOffset = XFS_DIR2_DATA_OFFSET / blockSize;
	if (lastOffset == (dataOffset + dirBlockCount))
		return XFS_DIR_BLOCK;

	uint64 leafOffset = XFS_DIR2_LEAF_OFFSET / blockSize;
	if (lastOffset == (leafOffset + dirBlockCount))
		return XFS_DIR_LEAF; 

	return XFS_DIR_NODE;
}


DirectoryIterator*
DirectoryIterator::Init(Inode* inode)
{
	DirectoryType dirType = GetDirectoryType(inode);
	status_t status;
	switch(dirType) {
		case XFS_DIR_SHORT:
		{
			ShortDirectory* shortDir = new(std::nothrow) ShortDirectory(inode);
			return shortDir;
		}
		case XFS_DIR_BLOCK:
		{
			BlockDirectory* blockDir = new(std::nothrow) BlockDirectory(inode);
			if (blockDir == NULL)
				return NULL;
			status = blockDir->Init();
			if (status != B_OK) {
				delete blockDir;
				return NULL;
			}
			return blockDir;
		}
		case XFS_DIR_LEAF:
		{
			LeafDirectory* leafDir = new(std::nothrow) LeafDirectory(inode);
			if (leafDir == NULL)
				return NULL;
			status = leafDir->Init();
			if (status != B_OK) {
				delete leafDir;
				return NULL;
			}
			return leafDir;
		}
		case XFS_DIR_NODE:
		{
			NodeDirectory* nodeDir = new(std::nothrow) NodeDirectory(inode);
			if (nodeDir == NULL)
				return NULL;
			status = nodeDir->Init();
			if (status != B_OK) {
				delete nodeDir;
				return NULL;
			}
			return nodeDir;
		}
		default:
			return NULL;
	}
}