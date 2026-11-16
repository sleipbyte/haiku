/*
 * Copyright 2026, Raghav Sharma, raghavself28@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "Extent.h"

#include "BPlusTree.h"
#include "Inode.h"


Extent::Extent(Inode* inode)
    :
	fInode(inode),
	fHeight(0),
	fBytes(0),
	fNextents(0),
	fData(NULL)
{
}


Extent::~Extent()
{
	_Destroy(fData, fHeight);
}


/*
 * Lookup an extent which covering blockNo key.
 * If there is an extent covering key return true and store expanded 
 * structure in entry.
 * If there is no extent covering key, but there is an extent after it
 * (eg: it lies in a hole) return the next extent.
 */
bool
Extent::Search(xfs_fileoff_t key, ExtentMapEntry* entry)
{
	status_t status;
	// Make sure we have already populated the extent tree
	status = _ReadExtents();
	if (status != B_OK)
		return false;

	ExtentLeaf* leaf = (ExtentLeaf*)_FindNode(key, 1);
	if (leaf == NULL)
		return false;
	for (int pos = 0; pos < RECS_PER_LEAF; pos++) {
		if (leaf->recs[pos].second == 0)
			break;
		xfs_fileoff_t leafOffset = _LeafKey(leaf, pos);
		xfs_filblks_t len = leaf->recs[pos].second & EXTENT_BLOCKCOUNT_MASK;
		if (leafOffset > key || leafOffset + len > key) {
			_UnwrapExtent(&leaf->recs[pos], entry);
			return true;
		}
	}

	// Check for the first entry in next leaf
	if (fHeight == 1 || !leaf->next)
		return false;
	leaf = leaf->next;
	if (leaf->recs[0].second == 0)
		return false;
	_UnwrapExtent(&leaf->recs[0], entry);
	return true;
}


status_t
Extent::_ReadExtents()
{
	if (fData != NULL) // Tree is already constructed
		return B_OK;

	if (fInode->Format() == XFS_DINODE_FMT_EXTENTS)
		return _ReadExtentsFromInode();
	if (fInode->Format() == XFS_DINODE_FMT_BTREE)
		return _ReadExtentsFromBtree();
	return B_BAD_VALUE;
}


status_t
Extent::_ReadExtentsFromInode()
{
	status_t status;

	ExtentMapUnwrap* extent = (ExtentMapUnwrap*) DIR_DFORK_PTR(fInode->Buffer(),
		fInode->CoreInodeSize());
	
	ExtentCursor cur = FirstExtentCursor();
	for (xfs_extnum_t i = 1; i <= fInode->DataExtentsCount(); i++, extent++) {
		status = Insert(extent, cur);
		if (status != B_OK) {
			// Destroy the already constructed tree till now
			_Destroy(fData, fHeight);
			return status;
		}
		NextExtentCursor(cur);
	}
	
	return B_OK;
}


/*
 * We have implemented B+Tree walker here, basically we go to first leaf 
 * node and then traverse all leaf nodes using next pointer.
 * Note that this is a generic B+Tree operation so ideally this should
 * shift to BPlusTree class but the current implementation at BPlusTree
 * class implements directory operation which should be supported by 
 * node directory handler.
 *
 * Now that we have generic extent tree which can be used everywhere
 * we can remove all the code in BPlusTree after node directories uptake
 * this extent tree after which generic B+Tree logic can reside in new
 * BPlusTree class instead of here.
 */
status_t
Extent::_ReadExtentsFromBtree()
{
	status_t status;
	BlockInDataFork* root = (BlockInDataFork*) DIR_DFORK_PTR(fInode->Buffer(),
		fInode->CoreInodeSize());
	uint16 levels = root->Levels();
	ssize_t blockSize = fInode->BlockSize();
	char* buffer = new(std::nothrow) char[blockSize];
		// Buffer to hold internal node and leaf block data
	if (buffer == NULL)
		return B_NO_MEMORY;
	ArrayDeleter<char> bufferDeleter(buffer);
	
	// Take the first pointer (leftmost) from root
	TreePointer* addr = _GetAddressFromRoot(1, (char*)root);
	xfs_fsblock_t blockNo = B_BENDIAN_TO_HOST_INT64(*addr);
	uint64 readPos = fInode->FileSystemBlockToAddr(blockNo);
	while (levels > 1) {
		if (read_pos(fInode->GetVolume()->Device(), readPos, buffer,
				blockSize) != blockSize) {
			ERROR("Extent::_ReadExtentsFromBtree(): IO Error");
			return B_IO_ERROR;
		}
		addr = _GetAddressFromNode(1, buffer);
		blockNo = B_BENDIAN_TO_HOST_INT64(*addr);
		readPos = fInode->FileSystemBlockToAddr(blockNo);
		levels--;
	}
	// addr now points to first leaf block, read all the leaf blocks till end
	ExtentCursor cur = FirstExtentCursor();
	while (1) {
		if (read_pos(fInode->GetVolume()->Device(), readPos, buffer,
				blockSize) != blockSize) {
			ERROR("Extent::_ReadExtentsFromBtree(): IO Error");
			return B_IO_ERROR;
		}
		LongBlock* block = (LongBlock*)buffer;
		uint16 nrecs = block->NumRecs();
		ExtentMapUnwrap* extent = (ExtentMapUnwrap*)(buffer + LongBlock::Size(fInode));
		for (int i = 0; i < nrecs; i++, extent++) {
			status = Insert(extent, cur);
			if (status != B_OK) {
				// Destroy the already constructed tree till now
				_Destroy(fData, fHeight);
				return status;
			}
			NextExtentCursor(cur);
		}
		blockNo = block->Right();
		if (blockNo == -1ULL) // We reached end
			break;
		readPos = fInode->FileSystemBlockToAddr(blockNo);
	}
	return B_OK;
}


TreePointer*
Extent::_GetAddressFromRoot(int pos, char* root) const
{
	size_t maxRecords = _MaxRecordsPossibleInTreeRoot();
	off_t offset = sizeof(BlockInDataFork)
		+ maxRecords * XFS_KEY_SIZE + (pos - 1) * XFS_PTR_SIZE;
	return (TreePointer*)(root + offset);
}


TreePointer*
Extent::_GetAddressFromNode(int pos, char* buffer) const
{
	size_t maxRecords = _MaxRecordsPossibleInNode();
	off_t offset = LongBlock::Size(fInode) + maxRecords * XFS_KEY_SIZE
		+ (pos - 1) * XFS_PTR_SIZE;
	return (TreePointer*)(buffer + offset);
}


size_t
Extent::_MaxRecordsPossibleInTreeRoot() const
{
	size_t dataForkSize = DFORK_DSIZE(fInode);
	dataForkSize -= sizeof(BlockInDataFork);
	return (dataForkSize / (XFS_KEY_SIZE + XFS_PTR_SIZE));
}


size_t
Extent::_MaxRecordsPossibleInNode() const
{
	size_t blockSize = fInode->BlockSize();
	blockSize -= LongBlock::Size(fInode);
	return (blockSize / (XFS_KEY_SIZE + XFS_PTR_SIZE));
}


// Try to insert the new extent record at cursor
status_t
Extent::Insert(ExtentMapUnwrap* extent, ExtentCursor& cur)
{
	_SwapEndian(extent);
	// key will be logical file offset
	xfs_fileoff_t key = (extent->first >> 9) & EXTENT_STARTOFF_MASK;
	ExtentLeaf* newLeaf = NULL;
	status_t status;

	// We need to initialize root which will be leaf
	if (fHeight == 0) {
		ExtentLeaf* root = new(std::nothrow) ExtentLeaf();
		if (root == NULL)
			return B_NO_MEMORY;
		root->recs[0].first = extent->first;
		root->recs[0].second = extent->second;
		root->prev = root->next = NULL;
		// fData now points to this root
		fData = root;
		fHeight = 1;
		// now that we have a node step into it
		cur.leaf = root;
		cur.pos = 0;
	} else {
		// Get total leaf entries in current cursor leaf
		int leafEntries = _LeafEntriesCount(cur);

		if (leafEntries == RECS_PER_LEAF) { // leaf overflow!
			newLeaf = _SplitLeaf(cur, leafEntries);
			if (newLeaf == NULL)
				return B_NO_MEMORY;
		}

		/*
		 * Update the pointers in higher levels if the first entry
		 * changes in existing node.
		 */
		if (cur.leaf != newLeaf && cur.pos == 0 && leafEntries > 0)
			_UpdateNode(_LeafKey(cur.leaf, 0), key, 1, cur.leaf);

		for (int i = leafEntries; i > cur.pos; i--)
			cur.leaf->recs[i] = cur.leaf->recs[i - 1];
		cur.leaf->recs[cur.pos].first = extent->first;
		cur.leaf->recs[cur.pos].second = extent->second;
        
		// if we created new leaf then we need to insert key in parent nodes
		if (newLeaf) {
			status = _InsertNode(_LeafKey(newLeaf, 0), newLeaf, 2);
			if (status != B_OK)
				return status;
		}
	}

	fBytes += sizeof(ExtentMapUnwrap);
	fNextents++;

	return B_OK;
}


status_t
Extent::_Grow()
{
	ExtentNode* node = new(std::nothrow) ExtentNode();
	if (node == NULL)
		return B_NO_MEMORY;

	if (fHeight == 1) {
		ExtentLeaf* prev = (ExtentLeaf*)fData;
		node->keys[0] = _LeafKey(prev, 0);
		node->ptrs[0] = prev;
	} else {
		ExtentNode* prev = (ExtentNode*)fData;
		node->keys[0] = prev->keys[0];
		node->ptrs[0] = prev;
	}

	for (int i = 1; i < KEYS_PER_NODE; i++)
		node->keys[i] = XFS_IEXT_KEY_INVALID;
    
	fData = node;
	fHeight++;

	return B_OK;
}


/*
 * Insert a child pointer and its offset key into an internal node at level.
 * This is called after new leaf is created so as to update all its parent nodes.
 * 
 * The level in parameter is defined as:
 * level 1: leaves.
 * level 2: internal nodes pointing to leaves.
 * level 3: internal nodes pointing to level-2 nodes and so on....
 *
 * This is a recursive function so we need to be very careful with stack usage.
 */
status_t
Extent::_InsertNode(xfs_fileoff_t offset, void* ptr, int level)
{
	status_t status;

	// If height of current tree is lesser than level we will need to create new root.
	if (fHeight < level) {
		status = _Grow();
		if (status != B_OK)
			return status;
	}

	ExtentNode* newNode = NULL;
	ExtentNode* node = (ExtentNode*)_FindNode(offset, level);
	int pos = _NodeInsertPosition(node, offset);
	int nodeEntries = _NodeEntriesCount(node, pos);

	if (nodeEntries == KEYS_PER_NODE) { // Node overflow!
		newNode = _SplitNode(node, pos, nodeEntries);
		if (newNode == NULL)
			return B_NO_MEMORY;
	}
    
	// Update the pointers in higher levels if the first entry changes in an existing node.
	if (node != newNode && pos == 0 && nodeEntries > 0)
		_UpdateNode(node->keys[0], offset, level, node);

	for (int i = nodeEntries; i > pos; i--) {
		node->keys[i] = node->keys[i - 1];
		node->ptrs[i] = node->ptrs[i - 1];
	}

	node->keys[pos] = offset;
	node->ptrs[pos] = ptr;

	if (newNode) {
		offset = newNode->keys[0];
		ptr = newNode;
		level++;
		// recursive call to create parent node of newNode
		status = _InsertNode(offset, ptr, level);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


void
Extent::_UpdateNode(xfs_fileoff_t oldOffset, xfs_fileoff_t newOffset,
	int level, void* ptr)
{
	ExtentNode* node = (ExtentNode*)fData;
	int height, i;

	for (height = fHeight; height > level; height--) {
		for (i = 0; i < KEYS_PER_NODE; i++) {
			if (i > 0 && node->keys[i] > oldOffset)
				break;
			if (node->keys[i] == oldOffset)
				node->keys[i] = newOffset;
		}
		node = (ExtentNode*)node->ptrs[i - 1];
		ASSERT(node);
	}

	ASSERT(node == ptr);
}


void*
Extent::_FindNode(xfs_fileoff_t offset, int level) const
{
	ExtentNode* node = (ExtentNode*)fData;

	if (!fHeight)
		return NULL;

	for (int height = fHeight; height > level; height--) {
		int i;
		for (i = 1; i < KEYS_PER_NODE; i++) {
			if (node->keys[i] > offset)
				break;
		}
		node = (ExtentNode*)node->ptrs[i - 1];
		if (!node)
			break;
	}

	return node;
}


int
Extent::_NodeInsertPosition(ExtentNode* node, xfs_fileoff_t offset) const
{
	for (int i = 0; i < KEYS_PER_NODE; i++) {
		if (node->keys[i] > offset)
			return i;
	}

	return KEYS_PER_NODE;
}


int
Extent::_NodeEntriesCount(ExtentNode* node, int start) const
{
	int i;

	for (i = start; i < KEYS_PER_NODE; i++) {
		if (node->keys[i] == XFS_IEXT_KEY_INVALID)
			break;
	}

	return i;
}


ExtentNode*
Extent::_SplitNode(ExtentNode*& nodep, int& pos, int& nodeEntries)
{
	ExtentNode* node = nodep;
	ExtentNode* newNode = new(std::nothrow) ExtentNode();
	if (newNode == NULL)
		return NULL;
	int nodeMove = KEYS_PER_NODE / 2;
	int nodeKeep = nodeMove + (KEYS_PER_NODE & 1); // Handling odd KEYS_PER_NODE case
	int i = 0;

	/*
	 * Performance optimization:
	 * For sequential append operations just spill over into the new node
	 * instead of moving nodeMove entries to new node.
	 */
	if (pos == KEYS_PER_NODE) {
		nodep = newNode;
		pos = 0;
		nodeEntries = 0;
	} else {
		for (i = 0; i < nodeMove; i++) {
			newNode->keys[i] = node->keys[nodeKeep + i];
			newNode->ptrs[i] = node->ptrs[nodeKeep + i];

			node->keys[nodeKeep + i] = XFS_IEXT_KEY_INVALID;
			node->ptrs[nodeKeep + i] = NULL;
		}

		if (pos >= nodeKeep) { // Insert position is in new split node
			nodep = newNode;
			pos -= nodeKeep;
			nodeEntries = nodeMove;
		} else {
			nodeEntries = nodeKeep;
		}
	}

	for (; i < KEYS_PER_NODE; i++)
		newNode->keys[i] = XFS_IEXT_KEY_INVALID;

	return newNode;
}


int
Extent::_LeafEntriesCount(ExtentCursor& cur) const
{
	int i;

	for (i = cur.pos; i < RECS_PER_LEAF; i++) {
		if (cur.leaf->recs[i].second == 0) // empty record
			break;
	}

	return i;
}


xfs_fileoff_t
Extent::_LeafKey(ExtentLeaf* leaf, int pos) const
{
	return (leaf->recs[pos].first >> 9) & EXTENT_STARTOFF_MASK;
}


ExtentLeaf*
Extent::_SplitLeaf(ExtentCursor& cur, int& leafEntries)
{
	ExtentLeaf* leaf = cur.leaf;
	ExtentLeaf* newLeaf = new(std::nothrow) ExtentLeaf();
	if (newLeaf == NULL)
		return NULL;
	int leafMove = RECS_PER_LEAF / 2;
	int leafKeep = leafMove + (RECS_PER_LEAF & 1); // handling odd RECS_PER_LEAF case

	/*
	 * Performance optimization:
	 * For sequential append operations just spill over into the new node
	 * instead of moving leafMove entries to new leaf.
	 */
	if (cur.pos == RECS_PER_LEAF) {
		cur.leaf = newLeaf;
		cur.pos = 0;
		leafEntries = 0;
	} else {
		for (int i = 0; i < leafMove; i++) {
			newLeaf->recs[i] = leaf->recs[leafKeep + i];
			leaf->recs[leafKeep + i].first = leaf->recs[leafKeep + i].second = 0;
		}

		if (cur.pos >= leafKeep) { // Insert position is in new split leaf
			cur.leaf = newLeaf;
			cur.pos -= leafKeep;
			leafEntries = leafMove;
		} else {
			leafEntries = leafKeep;
		}
	}

	if (leaf->next)
		leaf->next->prev = newLeaf;
	newLeaf->next = leaf->next;
	newLeaf->prev = leaf;
	leaf->next = newLeaf;
	return newLeaf;
}


ExtentCursor
Extent::FirstExtentCursor() const
{
	ExtentCursor cur;
	cur.pos = 0;
	cur.leaf = _FindFirstLeaf();
	return cur;
}


void
Extent::NextExtentCursor(ExtentCursor& cur) const
{
	if (!cur.leaf) {
		cur = FirstExtentCursor();
		return;
	}

	cur.pos++;
	if (fHeight > 1 && !_ValidateExtentCursor(cur) && cur.leaf->next) {
		cur.leaf = cur.leaf->next;
		cur.pos = 0;
	}
}


ExtentLeaf*
Extent::_FindFirstLeaf() const
{
	if (fHeight == 0)
		return NULL;

	void* node = fData;

	for (int height = fHeight; height > 1; height--)
		node = ((ExtentNode*)node)->ptrs[0];

	return (ExtentLeaf*)node;
}


void
Extent::_SwapEndian(ExtentMapUnwrap* extent) const
{
	extent->first = B_BENDIAN_TO_HOST_INT64(extent->first);
	extent->second = B_BENDIAN_TO_HOST_INT64(extent->second);
}


void
Extent::_UnwrapExtent(ExtentMapUnwrap* extent, ExtentMapEntry* entry) const
{
	entry->br_startoff = (extent->first & MASK((64 - EXTENT_EXNTFLAG_BITLEN))) >> 9;
	entry->br_startblock = ((extent->first & MASK(9)) << 43) | (extent->second >> 21);
	entry->br_blockcount = (extent->second & MASK(21));
	if ((extent->first >> (64 - EXTENT_EXNTFLAG_BITLEN)))
		entry->br_state = EXT_UNWRITTEN;
	else
		entry->br_state = EXT_NORM;
}


bool
Extent::_ValidateExtent(ExtentMapEntry* entry) const
{
	if (entry->br_startoff + entry->br_blockcount <= entry->br_startoff)
		return false;
	if (entry->br_startoff > EXTENT_MAX_FILEOFF
		|| entry->br_startoff + entry->br_blockcount - 1 > EXTENT_MAX_FILEOFF)
		return false;
	// check if all the blocks are inside same allocation group
	return FSBLOCKS_TO_AGNO(entry->br_startblock, fInode->GetVolume())
		== FSBLOCKS_TO_AGNO(entry->br_startblock + entry->br_blockcount - 1, fInode->GetVolume());
}


bool
Extent::_ValidateExtentCursor(ExtentCursor& cur) const
{
	if (!cur.leaf)
		return false;
	if (cur.pos < 0 || cur.pos >= RECS_PER_LEAF)
		return false;
	if (cur.leaf->recs[cur.pos].second == 0) // empty record
    	return false;
	return true;
}


// This is a recursive function so we need to be careful with stack usage.
void
Extent::_Destroy(void* ptr, int level)
{
	if (ptr == NULL)
		return;

	if (level == 1) {
		delete (ExtentLeaf*)ptr;
		return;
	}

	ExtentNode* node = (ExtentNode*)ptr;

	for (int i = 0; i < KEYS_PER_NODE; i++) {
		if (node->keys[i] == XFS_IEXT_KEY_INVALID)
			break;
		_Destroy(node->ptrs[i], level - 1);
	}

	delete node;
}
