/*
 * Copyright 2026, Raghav Sharma, raghavself28@gmail.com
 * Distributed under the terms of the MIT License.
 */
#ifndef UTILITY_H
#define UTILITY_H

// returns true if name1 and name2 matches
static inline bool 
xfs_da_name_comp(const char* name1, size_t length1, const unsigned char* name2, size_t length2)
{
    return length1 == length2 && memcmp(name1, name2, length1) == 0;
}

#endif // UTILITY_H