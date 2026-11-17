/*
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _BPLUS_TREE_H_
#define _BPLUS_TREE_H_


#include "Inode.h"


/*
 * Headers(here, the LongBlock) are the "nodes" really and are called "blocks".
 * The records, keys and ptrs are calculated using helpers
 */
class LongBlock {
public:

			uint32				Magic()
								{ return B_BENDIAN_TO_HOST_INT32(bb_magic); }

			uint16				Level()
								{ return B_BENDIAN_TO_HOST_INT16(bb_level); }

			uint16				NumRecs()
								{ return B_BENDIAN_TO_HOST_INT16(bb_numrecs); }

			TreePointer			Left()
								{ return B_BENDIAN_TO_HOST_INT64(bb_leftsib); }

			TreePointer			Right()
								{ return B_BENDIAN_TO_HOST_INT64(bb_rightsib); }

			uint64				Blockno()
								{ return B_BENDIAN_TO_HOST_INT64(bb_blkno); }

			uint64				Lsn()
								{ return B_BENDIAN_TO_HOST_INT64(bb_lsn); }

			const uuid_t&		Uuid()
								{ return bb_uuid; }

			uint64				Owner()
								{ return B_BENDIAN_TO_HOST_INT64(bb_owner); }

	static  uint32				Offset_v5()
								{ return offsetof(LongBlock, bb_blkno); }

	static	uint32				ExpectedMagic(int8 WhichDirectory,
										Inode* inode);

	static	uint32				CRCOffset();

	static	uint32				Size(Inode* inode);

private:

			uint32				bb_magic;
			uint16				bb_level;
			uint16				bb_numrecs;
			uint64				bb_leftsib;
			uint64				bb_rightsib;

			// Version 5 fields start here
			uint64				bb_blkno;
			uint64				bb_lsn;
			uuid_t				bb_uuid;
			uint64				bb_owner;
			uint32				bb_crc;
			uint32				bb_pad;
};

#endif
