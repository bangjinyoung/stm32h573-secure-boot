/*
 * mcuboot_assert.h - minimal port using the toolchain's <assert.h>.
 */

#ifndef MCUBOOT_ASSERT_H
#define MCUBOOT_ASSERT_H

#include <assert.h>

#define ASSERT(cond) assert(cond)

#endif /* MCUBOOT_ASSERT_H */
