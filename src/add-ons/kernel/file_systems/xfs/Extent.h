/*
 * Copyright 2026, Raghav Sharma, raghavself28@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _EXTENT_H_
#define _EXTENT_H_


#include "system_dependencies.h"
#include "xfs_types.h"


enum ExtentState {
	EXT_NORM,
	EXT_UNWRITTEN,
};


struct ExtentMapEntry {
			xfs_fileoff_t		br_startoff;
				// logical file block offset
			xfs_fsblock_t		br_startblock;
				// absolute block number
			xfs_filblks_t		br_blockcount;
				// # of blocks
			uint8				br_state;
				// state of the extent
};


/*
 * Bmap record descriptor.
 * first0:63 is an extent flag (value 1 indicated non-normal)
 * first0:9-62 are startoff.
 * first0:0-8 and second:21-63 are startblock.
 * second:0-20 are blockcount.
 */
#define EXTENT_EXNTFLAG_BITLEN 1
#define EXTENT_STARTOFF_BITLEN 54
#define EXTENT_STARTBLOCK_BITLEN 52
#define EXTENT_BLOCKCOUNT_BITLEN 21

#define EXTENT_STARTOFF_MASK ((1ULL << EXTENT_STARTOFF_BITLEN) - 1)
#define EXTENT_BLOCKCOUNT_MASK ((1ULL << EXTENT_BLOCKCOUNT_BITLEN) - 1)

#define EXTENT_MAX_EXTLEN ((xfs_extlen_t)(EXTENT_BLOCKCOUNT_MASK))
#define EXTENT_MAX_FILEOFF (EXTENT_STARTOFF_MASK + EXTENT_BLOCKCOUNT_MASK)


struct ExtentMapUnwrap {
			uint64				first;
			uint64				second;
};


struct ExtentLeaf;


constexpr int NODE_SIZE = 256;
constexpr int KEYS_PER_NODE = (NODE_SIZE / (sizeof(uint64_t) + sizeof(void*)));
constexpr int RECS_PER_LEAF =
	((NODE_SIZE - (2 * sizeof(ExtentLeaf*))) / sizeof(ExtentMapUnwrap));


struct ExtentNode {
			uint64				keys[KEYS_PER_NODE];
#define XFS_IEXT_KEY_INVALID (1ULL << 63)
			void*				ptrs[KEYS_PER_NODE];
};


struct ExtentLeaf {
			ExtentMapUnwrap		recs[RECS_PER_LEAF];
			ExtentLeaf*			prev;
			ExtentLeaf*			next;
};


struct ExtentCursor {
			ExtentLeaf*			leaf;
			int					pos;
};


class Inode;


/*
 * In-core extents btree layout:
 * 
 * The leaf blocks are made up by RECS_PER_LEAF extent records which contains
 * extents followed by pointers to previous and next leaf blocks.
 *
 * The non-leaf blocks first contain KEYS_PER_NODE lookup keys, followed by 
 * an equal number of pointers to the btree blocks at the next lower level.
 *
 * The lifecycle of this tree is mapped with lifecycle of inode so that we will
 * only need to construct this tree once based on need for extents and freeing 
 * inode will destroy this tree as well.
 *
 *		+-------+-------+-------+-------+-------+----------+----------+
 * Leaf:	| rec 1 | rec 2 | rec 3 | rec 4 | rec N | prev-ptr | next-ptr |
 *		+-------+-------+-------+-------+-------+----------+----------+
 *
 *		+-------+-------+-------+-------+-------+-------+------+-------+
 * Inner:	| key 1 | key 2 | key 3 | key N | ptr 1 | ptr 2 | ptr3 | ptr N |
 *		+-------+-------+-------+-------+-------+-------+------+-------+
 */
class Extent {
public:
								Extent(Inode* inode);
								~Extent();

			bool				Search(xfs_fileoff_t key, ExtentMapEntry* extent);
			xfs_fileoff_t		GetLastOffset();
			status_t			Insert(ExtentMapUnwrap* onDiskextent, ExtentCursor& cur);
			ExtentCursor		FirstExtentCursor() const;
			void				NextExtentCursor(ExtentCursor& cur) const;
private:
			status_t			_ReadExtents();
			status_t			_ReadExtentsFromInode();
			status_t			_ReadExtentsFromBtree();
			TreePointer*		_GetAddressFromRoot(int pos, char* root) const;
			TreePointer*		_GetAddressFromNode(int pos, char* buffer) const;
			size_t				_MaxRecordsPossibleInTreeRoot() const;
			size_t				_MaxRecordsPossibleInNode() const;
			void				_Destroy(void* ptr, int level);
			status_t			_Grow();
			int					_LeafEntriesCount(ExtentCursor& cur) const;
			xfs_fileoff_t		_LeafKey(ExtentLeaf* leaf, int pos) const;
			ExtentLeaf*			_SplitLeaf(ExtentCursor& cur, int& leafEntries);
			status_t			_InsertNode(xfs_fileoff_t offset, void* ptr, int level);
			void				_UpdateNode(xfs_fileoff_t oldOffset,
									xfs_fileoff_t newOffset, int level, void* ptr);
			ExtentLeaf*			_FindFirstLeaf() const;
			ExtentLeaf*			_FindLastLeaf() const;
			void*				_FindNode(xfs_fileoff_t offset, int level) const;
			int					_NodeInsertPosition(ExtentNode* node,
									xfs_fileoff_t offset) const;
			int					_NodeEntriesCount(ExtentNode* node, int start) const;
			ExtentNode*			_SplitNode(ExtentNode*& node, int& pos,
									int& nodeEntries);
			ExtentMapUnwrap		_SwapEndian(ExtentMapUnwrap* onDiskExtent) const;
			void				_UnwrapExtent(ExtentMapUnwrap* extent,
									ExtentMapEntry* entry) const;
			bool				_ValidateExtent(ExtentMapEntry* entry) const;
			bool				_ValidateExtentCursor(ExtentCursor& cur) const;
private:
				Inode*			fInode;
				int				fHeight;
					// Height of extent tree
				int64_t			fBytes;
					// Bytes size in fData
				xfs_extnum_t	fNextents;
					// Number of extents currently in tree
				void*			fData;
					// Extent tree root or inline data
};

#endif