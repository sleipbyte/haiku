/*
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BPlusTree.h"


uint32
LongBlock::ExpectedMagic(int8 whichDirectory, Inode* inode)
{
	if (inode->Version() == 3)
		return XFS_BMAP_CRC_MAGIC;
	else
		return XFS_BMAP_MAGIC;
}


uint32
LongBlock::CRCOffset()
{
	return offsetof(LongBlock, bb_crc);
}


uint32
LongBlock::Size(Inode* inode)
{
	if (inode->Version() == 3)
		return sizeof(LongBlock);
	else
		return LongBlock::Offset_v5();
}
