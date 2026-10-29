#ifndef CE_POSIX_INTERNAL_H
#define CE_POSIX_INTERNAL_H

#include <ce_posix.h>
#include <stdint.h>

#define CE_PATH_LENGTH 520

void ce_set_errno_from_last_error(void);
int64_t ce_unix_time_from_filetime(uint32_t low, uint32_t high);
void ce_filetime_from_unix_time(int64_t seconds, uint32_t *low, uint32_t *high);

#endif
