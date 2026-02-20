/*
 * Copyright 2022, Raghav Sharma, raghavself28@gmail.com
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BlockDirectory.h"

#include "VerifyHeader.h"


BlockDirectory::BlockDirectory(Inode* inode)
	:
	fInode(inode),
	fOffset(0)
{
}


BlockDirectory::~BlockDirectory()
{
}


void
BlockDirectory::FillMapEntry(void* pointerToMap)
{
	uint64 firstHalf = *((uint64*)pointerToMap);
	uint64 secondHalf = *((uint64*)pointerToMap + 1);
		// dividing the 128 bits into 2 parts.
	firstHalf = B_BENDIAN_TO_HOST_INT64(firstHalf);
	secondHalf = B_BENDIAN_TO_HOST_INT64(secondHalf);
	fMap->br_state = (firstHalf >> 63);
	fMap->br_startoff = (firstHalf & MASK(63)) >> 9;
	fMap->br_startblock = ((firstHalf & MASK(9)) << 43) | (secondHalf >> 21);
	fMap->br_blockcount = (secondHalf & MASK(21));
	TRACE("Extent::Init: startoff:(%" B_PRIu64 "), startblock:(%" B_PRIu64 "),"
		"blockcount:(%" B_PRIu64 "),state:(%" B_PRIu8 ")\n", fMap->br_startoff, fMap->br_startblock,
		fMap->br_blockcount, fMap->br_state);
}


status_t
BlockDirectory::FillBlockBuffer()
{
	if (fMap->br_state != 0)
		return B_BAD_VALUE;

	int len = fInode->DirBlockSize();
	fBlockBuffer = new(std::nothrow) char[len];
	if (fBlockBuffer == NULL)
		return B_NO_MEMORY;

	xfs_daddr_t readPos =
		fInode->FileSystemBlockToAddr(fMap->br_startblock);

	if (read_pos(fInode->GetVolume()->Device(), readPos, fBlockBuffer, len)
		!= len) {
		ERROR("Extent::FillBlockBuffer(): IO Error");
		return B_IO_ERROR;
	}

	return B_OK;
}


status_t
BlockDirectory::Init()
{
	fMap = new(std::nothrow) ExtentMapEntry;
	if (fMap == NULL)
		return B_NO_MEMORY;

	ASSERT(IsBlockType() == true);
	void* pointerToMap = DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize());
	FillMapEntry(pointerToMap);
	ASSERT(fMap->br_blockcount == 1);
		// TODO: This is always true for block directories
		// If we use this implementation for leaf directories, this is not
		// always true
	status_t status = FillBlockBuffer();
	if (status != B_OK)
		return status;

	DataHeader* header = DataHeader::Create(fInode, fBlockBuffer);
	if (header == NULL)
		return B_NO_MEMORY;
	if (!VerifyHeader<DataHeader>(header, fBlockBuffer, fInode, 0, fMap, XFS_BLOCK)) {
		status = B_BAD_VALUE;
		ERROR("Extent:Init(): Bad Block!\n");
	}

	delete header;
	return status;
}


BlockTail*
BlockDirectory::GetBlockTail()
{
	return (BlockTail*)
		(fBlockBuffer + fInode->DirBlockSize() - sizeof(BlockTail));
}


uint32
BlockDirectory::GetOffsetFromAddress(uint32 address)
{
	address = address * 8;
		// block offset in eight bytes, hence multiple with 8
	return address & (fInode->DirBlockSize() - 1);
}


LeafEntry*
BlockDirectory::BlockFirstLeaf(BlockTail* tail)
{
	return (LeafEntry*)tail - B_BENDIAN_TO_HOST_INT32(tail->count);
}


bool
BlockDirectory::IsBlockType()
{
	bool status = true;
	if (fInode->BlockCount() != 1)
		status = false;
	if (fInode->Size() != fInode->DirBlockSize())
		status = false;
	void* pointerToMap = DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize());
	xfs_fileoff_t startoff = (*((uint64*)pointerToMap) & MASK(63)) >> 9;
	if (startoff != 0)
		status = false;
	return status;
}


int
BlockDirectory::EntrySize(int len) const
{
	int entrySize = sizeof(xfs_ino_t) + sizeof(uint8) + len + sizeof(uint16);
			// uint16 is for the tag
	if (fInode->HasFileTypeField())
		entrySize += sizeof(uint8);

	return (entrySize + 7) & -8;
			// rounding off to closest multiple of 8
}


status_t
BlockDirectory::Rewind()
{
	fOffset = 0;
	return B_OK;
}


status_t
BlockDirectory::GetNext(char* name, size_t* length, xfs_ino_t* ino)
{
	TRACE("Extend::GetNext\n");

	void* entry; // This could be unused entry so we should check

	entry = (void*)(fBlockBuffer + DataHeader::Size(fInode));

	int numberOfEntries = B_BENDIAN_TO_HOST_INT32(GetBlockTail()->count);
	int numberOfStaleEntries = B_BENDIAN_TO_HOST_INT32(GetBlockTail()->stale);

	// We don't read stale entries.
	numberOfEntries -= numberOfStaleEntries;
	TRACE("numberOfEntries:(%" B_PRId32 ")\n", numberOfEntries);
	uint16 currentOffset = (char*)entry - fBlockBuffer;

	for (int i = 0; i < numberOfEntries; i++) {
		UnusedEntry* unusedEntry = (UnusedEntry*)entry;

		if (B_BENDIAN_TO_HOST_INT16(unusedEntry->freetag) == DIR2_FREE_TAG) {
			TRACE("Unused entry found\n");
			currentOffset += B_BENDIAN_TO_HOST_INT16(unusedEntry->length);
			entry = (void*)
				((char*)entry + B_BENDIAN_TO_HOST_INT16(unusedEntry->length));
			i--;
			continue;
		}
		DataEntry* dataEntry = (DataEntry*) entry;

		if (fOffset >= currentOffset) {
			entry = (void*)((char*)entry + EntrySize(dataEntry->namelen));
			currentOffset += EntrySize(dataEntry->namelen);
			continue;
		}

		if ((size_t)(dataEntry->namelen) >= *length)
				return B_BUFFER_OVERFLOW;

		fOffset = currentOffset;
		memcpy(name, dataEntry->name, dataEntry->namelen);
		name[dataEntry->namelen] = '\0';
		*length = dataEntry->namelen;
		*ino = B_BENDIAN_TO_HOST_INT64(dataEntry->inumber);

		TRACE("Entry found. Name: (%s), Length: (%" B_PRIuSIZE "),ino: (%" B_PRIu64 ")\n", name,
			*length, *ino);
		return B_OK;
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
BlockDirectory::Lookup(const char* name, size_t length, xfs_ino_t* ino)
{
	TRACE("Extent: Lookup\n");
	TRACE("Name: %s\n", name);
	uint32 hashValueOfRequest = hashfunction(name, length);
	TRACE("Hashval:(%" B_PRIu32 ")\n", hashValueOfRequest);
	BlockTail* blockTail = GetBlockTail();
	LeafEntry* leafEntry = BlockFirstLeaf(blockTail);

	int numberOfLeafEntries = B_BENDIAN_TO_HOST_INT32(blockTail->count);
	int left = 0;
	int right = numberOfLeafEntries - 1;

	hashLowerBound<LeafEntry>(leafEntry, left, right, hashValueOfRequest);

	while (B_BENDIAN_TO_HOST_INT32(leafEntry[left].hashval)
			== hashValueOfRequest) {

		uint32 address = B_BENDIAN_TO_HOST_INT32(leafEntry[left].address);
		if (address == 0) {
			left++;
			continue;
		}

		uint32 offset = GetOffsetFromAddress(address);
		TRACE("offset:(%" B_PRIu32 ")\n", offset);
		DataEntry* entry = (DataEntry*)(fBlockBuffer + offset);

		if (xfs_da_name_comp(name, length, entry->name, entry->namelen)) {
			*ino = B_BENDIAN_TO_HOST_INT64(entry->inumber);
			TRACE("ino:(%" B_PRIu64 ")\n", *ino);
			return B_OK;
		}
		left++;
	}

	return B_ENTRY_NOT_FOUND;
}


DataHeader::~DataHeader()
{
}


/*
	First see which type of directory we reading then
	return magic number as per Inode Version.
*/
uint32
DataHeader::ExpectedMagic(int8 WhichDirectory, Inode* inode)
{
	if (WhichDirectory == XFS_BLOCK) {
		if (inode->Version() == 1 || inode->Version() == 2)
			return DIR2_BLOCK_HEADER_MAGIC;
		else
			return DIR3_BLOCK_HEADER_MAGIC;
	} else {
		if (inode->Version() == 1 || inode->Version() == 2)
			return V4_DATA_HEADER_MAGIC;
		else
			return V5_DATA_HEADER_MAGIC;
	}
}


uint32
DataHeader::CRCOffset()
{
	return offsetof(DataHeaderV5::OnDiskData, crc);
}


DataHeader*
DataHeader::Create(Inode* inode, const char* buffer)
{
	if (inode->Version() == 1 || inode->Version() == 2) {
		DataHeaderV4* header = new (std::nothrow) DataHeaderV4(buffer);
		return header;
	} else {
		DataHeaderV5* header = new (std::nothrow) DataHeaderV5(buffer);
		return header;
	}
}


/*
	This Function returns Actual size of data header
	in all forms of directory.
	Never use sizeof() operator because we now have
	vtable as well and it will give wrong results
*/
uint32
DataHeader::Size(Inode* inode)
{
	if (inode->Version() == 1 || inode->Version() == 2)
		return sizeof(DataHeaderV4::OnDiskData);
	else
		return sizeof(DataHeaderV5::OnDiskData);
}


void
DataHeaderV4::_SwapEndian()
{
	fData.magic = (B_BENDIAN_TO_HOST_INT32(fData.magic));
}


DataHeaderV4::DataHeaderV4(const char* buffer)
{
	memcpy(&fData, buffer, sizeof(fData));
	_SwapEndian();
}


DataHeaderV4::~DataHeaderV4()
{
}


uint32
DataHeaderV4::Magic()
{
	return fData.magic;
}


uint64
DataHeaderV4::Blockno()
{
	return B_BAD_VALUE;
}


uint64
DataHeaderV4::Lsn()
{
	return B_BAD_VALUE;
}


uint64
DataHeaderV4::Owner()
{
	return B_BAD_VALUE;
}


const uuid_t&
DataHeaderV4::Uuid()
{
	static uuid_t nullUuid;
	return nullUuid;
}


void
DataHeaderV5::_SwapEndian()
{
	fData.magic	=	B_BENDIAN_TO_HOST_INT32(fData.magic);
	fData.blkno	=	B_BENDIAN_TO_HOST_INT64(fData.blkno);
	fData.lsn		=	B_BENDIAN_TO_HOST_INT64(fData.lsn);
	fData.owner	=	B_BENDIAN_TO_HOST_INT64(fData.owner);
	fData.pad		=	B_BENDIAN_TO_HOST_INT32(fData.pad);
}


DataHeaderV5::DataHeaderV5(const char* buffer)
{
	memcpy(&fData, buffer, sizeof(fData));
	_SwapEndian();
}


DataHeaderV5::~DataHeaderV5()
{
}


uint32
DataHeaderV5::Magic()
{
	return fData.magic;
}


uint64
DataHeaderV5::Blockno()
{
	return fData.blkno;
}


uint64
DataHeaderV5::Lsn()
{
	return fData.lsn;
}


uint64
DataHeaderV5::Owner()
{
	return fData.owner;
}


const uuid_t&
DataHeaderV5::Uuid()
{
	return fData.uuid;
}