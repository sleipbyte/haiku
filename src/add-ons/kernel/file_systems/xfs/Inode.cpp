/*
 * Copyright 2022, Raghav Sharma, raghavself28@gmail.com
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "Inode.h"

#include "BPlusTree.h"
#include "VerifyHeader.h"


void
Inode::_SwapEndian()
{
	fNode->di_magic = B_BENDIAN_TO_HOST_INT16(fNode->di_magic);
	fNode->di_mode = B_BENDIAN_TO_HOST_INT16(fNode->di_mode);
	fNode->di_onlink = B_BENDIAN_TO_HOST_INT16(fNode->di_onlink);
	fNode->di_uid = B_BENDIAN_TO_HOST_INT32(fNode->di_uid);
	fNode->di_gid = B_BENDIAN_TO_HOST_INT32(fNode->di_gid);
	fNode->di_nlink = B_BENDIAN_TO_HOST_INT32(fNode->di_nlink);
	fNode->di_projid_lo = B_BENDIAN_TO_HOST_INT16(fNode->di_projid_lo);
	fNode->di_projid_hi = B_BENDIAN_TO_HOST_INT16(fNode->di_projid_hi);
	fNode->di_size = B_BENDIAN_TO_HOST_INT64(fNode->di_size);
	fNode->di_nblocks = B_BENDIAN_TO_HOST_INT64(fNode->di_nblocks);
	fNode->di_extsize = B_BENDIAN_TO_HOST_INT32(fNode->di_extsize);
	fNode->di_dmevmask = B_BENDIAN_TO_HOST_INT32(fNode->di_dmevmask);
	fNode->di_dmstate = B_BENDIAN_TO_HOST_INT16(fNode->di_dmstate);
	fNode->di_flags = B_BENDIAN_TO_HOST_INT16(fNode->di_flags);
	fNode->di_gen = B_BENDIAN_TO_HOST_INT32(fNode->di_gen);
	fNode->di_next_unlinked = B_BENDIAN_TO_HOST_INT32(fNode->di_next_unlinked);
	fNode->di_changecount = B_BENDIAN_TO_HOST_INT64(fNode->di_changecount);
	fNode->di_lsn = B_BENDIAN_TO_HOST_INT64(fNode->di_lsn);
	fNode->di_flags2 = B_BENDIAN_TO_HOST_INT64(fNode->di_flags2);
	fNode->di_cowextsize = B_BENDIAN_TO_HOST_INT64(fNode->di_cowextsize);
	fNode->di_ino = B_BENDIAN_TO_HOST_INT64(fNode->di_ino);
	if (_IsBigtimeEnabled()) {
		fNode->di_mtime = B_BENDIAN_TO_HOST_INT64(fNode->di_mtime);
		fNode->di_atime = B_BENDIAN_TO_HOST_INT64(fNode->di_atime);
		fNode->di_ctime = B_BENDIAN_TO_HOST_INT64(fNode->di_ctime);
		fNode->di_crtime = B_BENDIAN_TO_HOST_INT64(fNode->di_crtime);
	} else {
		/**
		 * For all the legacy timestamps we convert it to Bigtime format
		 * so that all Inode methods always deal with Bigtime format without
		 * worrying about its actual on-disk structure.
		 * Later when writing data to disk swapEndian method to disk format
		 * will take care of its on-disk structure.
		 */
		xfs_legacy_timestamp* lts;

		lts = (xfs_legacy_timestamp*)&fNode->di_mtime;
		lts->t_sec = B_BENDIAN_TO_HOST_INT32(lts->t_sec);
		lts->t_nsec = B_BENDIAN_TO_HOST_INT32(lts->t_nsec);
		fNode->di_mtime = _ConvertLegacyToBigTimestamp(lts);

		lts = (xfs_legacy_timestamp*)&fNode->di_atime;
		lts->t_sec = B_BENDIAN_TO_HOST_INT32(lts->t_sec);
		lts->t_nsec = B_BENDIAN_TO_HOST_INT32(lts->t_nsec);
		fNode->di_atime = _ConvertLegacyToBigTimestamp(lts);

		lts = (xfs_legacy_timestamp*)&fNode->di_ctime;
		lts->t_sec = B_BENDIAN_TO_HOST_INT32(lts->t_sec);
		lts->t_nsec = B_BENDIAN_TO_HOST_INT32(lts->t_nsec);
		fNode->di_ctime = _ConvertLegacyToBigTimestamp(lts);

		lts = (xfs_legacy_timestamp*)&fNode->di_crtime;
		lts->t_sec = B_BENDIAN_TO_HOST_INT32(lts->t_sec);
		lts->t_nsec = B_BENDIAN_TO_HOST_INT32(lts->t_nsec);
		fNode->di_crtime = _ConvertLegacyToBigTimestamp(lts);
	}

	if (_IsLargeExtentsEnabled()) {
		fNode->di_big_nextents = B_BENDIAN_TO_HOST_INT64(fNode->di_big_nextents);
		fNode->di_big_anextents = B_BENDIAN_TO_HOST_INT32(fNode->di_big_anextents);
	} else {
		fNode->di_nextents = B_BENDIAN_TO_HOST_INT32(fNode->di_nextents);
		fNode->di_naextents = B_BENDIAN_TO_HOST_INT16(fNode->di_naextents);
		if (Version() == 2) {
			fNode->di_flushiter = B_BENDIAN_TO_HOST_INT16(fNode->di_flushiter);
		}
	}
}


xfs_timestamp_t
Inode::_ConvertLegacyToBigTimestamp(xfs_legacy_timestamp* lts) const
{
	return xfs_unix_to_bigtime(lts->t_sec) * NSEC_PER_SEC + lts->t_nsec;
}


void
Inode::GetModificationTime(struct timespec& timestamp) const
{
	timestamp.tv_sec = xfs_bigtime_to_unix(fNode->di_mtime / NSEC_PER_SEC);
	timestamp.tv_nsec = fNode->di_mtime % NSEC_PER_SEC;
}


void
Inode::GetAccessTime(struct timespec& timestamp) const
{
	timestamp.tv_sec = xfs_bigtime_to_unix(fNode->di_atime / NSEC_PER_SEC);
	timestamp.tv_nsec = fNode->di_atime % NSEC_PER_SEC;
}


void
Inode::GetChangeTime(struct timespec& timestamp) const
{
	timestamp.tv_sec = xfs_bigtime_to_unix(fNode->di_ctime / NSEC_PER_SEC);
	timestamp.tv_nsec = fNode->di_ctime % NSEC_PER_SEC;
}


void
Inode::GetCreationTime(struct timespec& timestamp) const
{
	timestamp.tv_sec = xfs_bigtime_to_unix(fNode->di_crtime / NSEC_PER_SEC);
	timestamp.tv_nsec = fNode->di_crtime % NSEC_PER_SEC;
}


Inode::Inode(Volume* volume, xfs_ino_t id)
	:
	fId(id),
	fVolume(volume),
	fBuffer(NULL),
	fDataExtent(NULL)
{
}


//Convert inode mode to directory entry filetype
unsigned char
Inode::XfsModeToFtype() const
{
	switch (Mode() & S_IFMT) {
	case S_IFREG:
		return XFS_DIR3_FT_REG_FILE;
	case S_IFDIR:
		return XFS_DIR3_FT_DIR;
	case S_IFCHR:
		return XFS_DIR3_FT_CHRDEV;
	case S_IFBLK:
		return XFS_DIR3_FT_BLKDEV;
	case S_IFIFO:
		return XFS_DIR3_FT_FIFO;
	case S_IFSOCK:
		return XFS_DIR3_FT_SOCK;
	case S_IFLNK:
		return XFS_DIR3_FT_SYMLINK;
	default:
		return XFS_DIR3_FT_UNKNOWN;
	}
}


bool
Inode::VerifyFork(int whichFork) const
{
	uint32 di_nextents = XFS_DFORK_NEXTENTS(fNode, whichFork);

	switch (XFS_DFORK_FORMAT(fNode, whichFork)) {
	case XFS_DINODE_FMT_LOCAL:
		if (whichFork == XFS_DATA_FORK) {
			if (S_ISREG(Mode()))
				return false;
			if (Size() > (xfs_fsize_t) DFORK_SIZE(this, whichFork))
				return false;
		}
		if (di_nextents)
			return false;
		break;
	case XFS_DINODE_FMT_EXTENTS:
		if (di_nextents > DFORK_MAXEXT(this, whichFork))
			return false;
		break;
	case XFS_DINODE_FMT_BTREE:
		if (whichFork == XFS_ATTR_FORK) {
			if (di_nextents > MAXAEXTNUM)
				return false;
		} else if (di_nextents > MAXEXTNUM) {
			return false;
		}
		break;
	default:
		return false;
	}

	return true;
}


bool
Inode::VerifyForkoff() const
{
	// Valid case when there is no attribute fork present
	if (!fNode->di_forkoff)
		return true;

	switch (Format()) {
		case XFS_DINODE_FMT_DEV:
			if (fNode->di_forkoff != (ROUNDUP(sizeof(uint32), 8) >> 3))
				return false;
		break;
		case XFS_DINODE_FMT_LOCAL:
		case XFS_DINODE_FMT_EXTENTS:
		case XFS_DINODE_FMT_BTREE:
			if (fNode->di_forkoff >= (LITINO(fVolume) >> 3))
				return false;
			break;
	default:
		return false;
	}

	return true;
}


bool
Inode::VerifyInode() const
{
	if (fNode->di_magic != INODE_MAGIC) {
		ERROR("Bad inode magic number");
		return false;
	}

	// check if inode version is valid
	if (Version() < 1 || Version() > 3) {
		ERROR("Bad inode version");
		return false;
	}

	// verify version 3 inodes first
	if (Version() == 3) {

		if (!HAS_V3INODES(fVolume)) {
			ERROR("xfs v4 doesn't have v3 inodes");
			return false;
		}

		if (!xfs_verify_cksum(fBuffer, fVolume->InodeSize(), INODE_CRC_OFF)) {
			ERROR("Inode is corrupted");
			return false;
		}

		if (fNode->di_ino != fId) {
			ERROR("Incorrect inode number");
			return false;
		}

		if (!fVolume->UuidEquals(fNode->di_uuid)) {
			ERROR("UUID is incorrect");
			return false;
		}
	}

	if ((fNode->di_size & (1ULL << 63)) != 0) {
		ERROR("Invalid EOF of inode");
		return false;
	}

	if (Mode() && XfsModeToFtype() == XFS_DIR3_FT_UNKNOWN) {
		 ERROR("Entry points to an unknown inode type");
		 return false;
	}

	if (!VerifyForkoff()) {
		ERROR("Invalid inode fork offset");
		return false;
	}

	// Check for appropriate data fork formats for the mode
	switch (Mode() & S_IFMT) {
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
		if (fNode->di_format != XFS_DINODE_FMT_DEV)
			return false;
		break;
	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
		if (!VerifyFork(XFS_DATA_FORK)) {
			ERROR("Invalid data fork in inode");
			return false;
		}
		break;
	case 0:
		// Uninitialized inode is fine
		break;
	default:
		return false;
	}

	if (fNode->di_forkoff) {
		if (!VerifyFork(XFS_ATTR_FORK)) {
			ERROR("Invalid attribute fork in inode");
			return false;
		}
	} else {
		/*
			If there is no fork offset, this may be a freshly-made inode
			in a new disk cluster, in which case di_aformat is zeroed.
			Otherwise, such an inode must be in EXTENTS format; this goes
			for freed inodes as well.
		 */
		switch (fNode->di_aformat) {
		case 0:
		case XFS_DINODE_FMT_EXTENTS:
			break;
		default:
			return false;
		}

		if (fNode->di_naextents)
			return false;
	}

	// Bigtime can only happen on bigtime filesystems
	if (_IsBigtimeEnabled() && !fVolume->IsBigtimeEnabled()) {
		ERROR("Bigtime inodes can only be supported on bigtime enabled filesystem");
		return false;
	}

	// Large extents can only happen on large extents enabled filesystems
	if (_IsLargeExtentsEnabled() && !fVolume->IsLargeExtentsEnabled()) {
		ERROR("Large extents inodes can only be supported on large extents enabled filesystem");
	}

	return true;
}


uint32
Inode::CoreInodeSize() const
{
	if (Version() == 3)
		return sizeof(Dinode);

	return offsetof(Dinode, di_crc);
}


xfs_extnum_t
Inode::DataExtentsCount() const
{
	if (_IsLargeExtentsEnabled())
		return fNode->di_big_nextents;
	return fNode->di_nextents;
}


xfs_aextnum_t
Inode::AttrExtentsCount() const
{
	if (_IsLargeExtentsEnabled())
		return fNode->di_big_anextents;
	return fNode->di_naextents;
}


status_t
Inode::Init()
{
	fNode = new(std::nothrow) Dinode;
	if (fNode == NULL)
		return B_NO_MEMORY;

	uint16 inodeSize = fVolume->InodeSize();
	fBuffer = new(std::nothrow) char[inodeSize];
	if (fBuffer == NULL)
		return B_NO_MEMORY;

	// Initialise data extents, note that ram tree will be populated when first extent
	// would need to be accessed.
	fDataExtent = new(std::nothrow) Extent(this);
	if (fDataExtent == NULL)
		return B_NO_MEMORY;

	status_t status = _GetFromDisk();
	if (status == B_OK) {
		if (VerifyInode()) {
			TRACE("Init(): Inode successfully read.\n");
			status = B_OK;
		} else {
			TRACE("Init(): Inode wasn't read successfully!\n");
			status = B_BAD_VALUE;
		}
	}

	return status;
}


Inode::~Inode()
{
	delete fNode;
	delete[] fBuffer;
	delete fDataExtent;
}


bool
Inode::HasFileTypeField() const
{
	if ((Version() == 3 && fVolume->XfsHasIncompatFeature())
		|| (fVolume->SuperBlockFeatures2() & XFS_SB_VERSION2_FTYPE)) {
		return true;
	}
	return false;
}


status_t
Inode::CheckPermissions(int accessMode) const
{
	// you never have write access to a read-only volume
	if ((accessMode & W_OK) != 0 && fVolume->IsReadOnly())
		return B_READ_ONLY_DEVICE;

	return check_access_permissions(accessMode, Mode(), (uint32)GroupId(), (uint32)UserId());
}


status_t
Inode::ReadAt(off_t pos, uint8* buffer, size_t* length)
{
	TRACE("Inode::ReadAt: pos:(%" B_PRIdOFF "), *length:(%" B_PRIuSIZE ")\n", pos, *length);

	// set/check boundaries for pos/length
	if (pos < 0) {
		ERROR("inode %" B_PRIdINO ": ReadAt failed(pos %" B_PRIdOFF
			", length %lu)\n", ID(), pos, *length);
		return B_BAD_VALUE;
	}

	if (pos >= Size() || *length == 0) {
		TRACE("inode %" B_PRIdINO ": ReadAt 0 (pos %" B_PRIdOFF
			", length %" B_PRIuSIZE ")\n", ID(), pos, *length);
		*length = 0;
		return B_NO_ERROR;
	}

	size_t lengthToRead = Size() - pos;
	if (*length < lengthToRead)
		lengthToRead = *length;
	ssize_t lengthOfBlock = BlockSize();
	size_t lengthRead = 0;
	TRACE("What is blockLen:(%" B_PRId64 ")\n", lengthOfBlock);

	while (lengthToRead > 0) {
		TRACE("Inode::ReadAt: pos:(%" B_PRIdOFF "), *length:(%" B_PRIuSIZE ")\n", pos, *length);
		ExtentMapEntry extent;
		bool found = fDataExtent->Search(pos / lengthOfBlock, &extent);
		// TODO: Implement sparse file support
		if (!found) {
			ERROR("Inode::ReadAt(): Extent not found");
			return B_BAD_VALUE;
		}

		off_t extentStartPos = extent.br_startoff * lengthOfBlock;
		off_t extentEndPos = (extent.br_startoff + extent.br_blockcount) * lengthOfBlock;

		// TODO: Implement sparse file support
		if (pos < extentStartPos)
			return B_BAD_VALUE;

		// The returned extent covers pos
		ssize_t length = lengthToRead; 
		if (extentEndPos - pos < length)
			length = extentEndPos - pos;
		uint64 offsetWithinExtent = pos - extentStartPos;
		uint64 readPos = FileSystemBlockToAddr(extent.br_startblock) + offsetWithinExtent;
		// ERROR("LengthToRead = %lld", lengthToRead);
		// ERROR("length = %lld", length);
		// ERROR("readPos = %lld", readPos);
		if (read_pos(fVolume->Device(), readPos, buffer + lengthRead, length) != length) {
			ERROR("Inode::ReadAt(): IO Error");
			return B_IO_ERROR;
		}
		pos += length;
		lengthRead += length;
		lengthToRead -= length;
	}

	TRACE("lengthRead:(%" B_PRIuSIZE ")\n", lengthRead);
	*length = lengthRead;
	return B_OK;
}


status_t
Inode::_GetFromDisk()
{
	xfs_agnumber_t agNo = INO_TO_AGNO(fId, fVolume);
		// Get the AG number from the inode
	uint32 agRelativeInodeNo = INO_TO_AGINO(fId, fVolume->AgInodeBits());
		// AG relative ino #
	xfs_agblock_t agBlock = INO_TO_AGBLOCK(agRelativeInodeNo, fVolume);
		// AG relative block number
	xfs_off_t offset = INO_TO_BLOCKOFFSET(fId, fVolume);
		// Offset into the block1
	uint32 len = fVolume->InodeSize();
		// Inode len to read (size of inode)

	TRACE("AgNumber: (%" B_PRIu32 "), AgRelativeIno: (%" B_PRIu32 "),"
		"AgRelativeBlockNum: (%" B_PRIu32 "),Offset: (%" B_PRId64 "),"
		"len: (%" B_PRIu32 ")\n", agNo, agRelativeInodeNo, agBlock, offset, len);

	if (agNo > fVolume->AgCount()) {
		ERROR("Inode::GetFromDisk : AG Number more than number of AGs");
		return B_ENTRY_NOT_FOUND;
	}

	xfs_agblock_t numberOfBlocksInAg = fVolume->AgBlocks();

	xfs_fsblock_t blockToRead = FSBLOCKS_TO_BASICBLOCKS(fVolume->BlockLog(),
		((uint64)(agNo * numberOfBlocksInAg) + agBlock));

	xfs_daddr_t readPos = blockToRead * XFS_MIN_BLOCKSIZE + offset * len;

	if (read_pos(fVolume->Device(), readPos, fBuffer, len) != len) {
		ERROR("Inode::Inode(): IO Error");
		return B_IO_ERROR;
	}

	if (fVolume->IsVersion5())
		memcpy(fNode, fBuffer, sizeof(Inode::Dinode));
	else
		memcpy(fNode, fBuffer, INODE_CRC_OFF);

	_SwapEndian();

	return B_OK;
}


uint64
Inode::FileSystemBlockToAddr(uint64 block)
{
	xfs_agblock_t numberOfBlocksInAg = fVolume->AgBlocks();

	uint64 agNo = FSBLOCKS_TO_AGNO(block, fVolume);
	uint64 agBlockNo = FSBLOCKS_TO_AGBLOCKNO(block, fVolume);

	xfs_fsblock_t actualBlockToRead =
		FSBLOCKS_TO_BASICBLOCKS(fVolume->BlockLog(),
			((uint64)(agNo * numberOfBlocksInAg) + agBlockNo));
	TRACE("blockToRead:(%" B_PRIu64 ")\n", actualBlockToRead);

	uint64 readPos = actualBlockToRead * (XFS_MIN_BLOCKSIZE);
	return readPos;
}


Extent*
Inode::GetDataExtent()
{
	return fDataExtent;
}
