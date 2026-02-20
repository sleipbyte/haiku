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


DirectoryIterator*
DirectoryIterator::Init(Inode* inode)
{
	if (inode->Format() == XFS_DINODE_FMT_LOCAL) {
		TRACE("Iterator:Init: LOCAL");
		ShortDirectory* shortDir = new(std::nothrow) ShortDirectory(inode);
		return shortDir;
	}

	if (inode->Format() == XFS_DINODE_FMT_EXTENTS) {
		TRACE("Iterator:Init: EXTENTS");
		status_t status;

		// Check if it is extent based directory
		BlockDirectory* blockDir = new(std::nothrow) BlockDirectory(inode);
		if (blockDir == NULL)
			return NULL;

		if (blockDir->IsBlockType()) {
			status = blockDir->Init();
			if (status == B_OK)
				return blockDir;
		}

		delete blockDir;

		// Check if it is leaf based directory
		LeafDirectory* leafDir = new(std::nothrow) LeafDirectory(inode);
		if (leafDir == NULL)
			return NULL;

		if (leafDir->IsLeafType()) {
			status = leafDir->Init();
			if (status == B_OK)
				return leafDir;
		}

		delete leafDir;

		// Check if it is node based directory
		NodeDirectory* nodeDir = new(std::nothrow) NodeDirectory(inode);
		if (nodeDir == NULL)
			return NULL;

		if (nodeDir->IsNodeType()) {
			status = nodeDir->Init();
			if (status == B_OK)
				return nodeDir;
		}

		delete nodeDir;
	}

	if (inode->Format() == XFS_DINODE_FMT_BTREE) {
		TRACE("Iterator:Init(): B+TREE");
		TreeDirectory* treeDir = new(std::nothrow) TreeDirectory(inode);
		if (treeDir == NULL)
			return NULL;

		status_t status = treeDir->InitCheck();

		if (status == B_OK)
			return treeDir;
	}

	// Invalid format return NULL
	return NULL;
}