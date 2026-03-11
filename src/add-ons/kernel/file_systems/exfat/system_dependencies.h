/*
 * Copyright 2018, Your Name <your@email.address>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _SYSTEM_DEPENDENCIES_H
#define _SYSTEM_DEPENDENCIES_H

#ifdef FS_SHELL
// This needs to be included before the fs_shell wrapper
#include "fssh_api_wrapper.h"
#include "fssh_auto_deleter.h"
#include <new>
#include <util/convertutf.h>
#include <util/kernel_cpp.h>

#else // !FS_SHELL

#include <util/AutoLock.h>
#include <string.h>
#include <AutoDeleter.h>
#include <fs_cache.h>
#include <sys/stat.h>
#include <ByteOrder.h>
#include <fs_interface.h>
#include <KernelExport.h>
#include <fs_cache.h>
#include <lock.h>
#include <util/AutoLock.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <io_requests.h>
#include <real_time_clock.h>
#include <sys/stat.h>
#include <stdio.h>
#include <Errors.h>
#include <errno.h>
#include <unistd.h>
#include <fs_info.h>
#include <fs_volume.h>
#include <NodeMonitor.h>
#include <lock.h>
#endif // !FS_SHELL

#endif // _SYSTEM_DEPENDENCIES_H
