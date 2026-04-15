/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Creation OS — master header (scaling + module index).
 * Full runnable integration: ../creation_os_v2.c
 */
#ifndef CREATION_OS_H
#define CREATION_OS_H

#include <stdint.h>
#include "cos_bsc.h"

#define CREATION_OS_VERSION_MAJOR 2
#define CREATION_OS_VERSION_MINOR 0

/* Hypervector geometry (must match creation_os_v2.c) */
#ifndef CREATION_OS_D
#define CREATION_OS_D COS_D
#endif
#ifndef CREATION_OS_W
#define CREATION_OS_W COS_W
#endif

#endif /* CREATION_OS_H */
