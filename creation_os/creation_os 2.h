/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * =============================================================================
 * CREATION OS v2.0 — Core Source
 * Copyright (C) 2026 Lauri Elias Rainio | Spektre Labs
 * All Rights Reserved.
 * =============================================================================
 *
 * Dual licensing: AGPL-3.0 (open) / commercial licence from Spektre Labs.
 * Full notice: creation_os/LEGAL/SOURCE_FILE_HEADER.c
 * Contact: spektrelabs@proton.me
 *
 * creation_os.h — CREATION OS umbrella: GDA AGI coherence layer (version + scaling).
 * ORCID: 0009-0006-0903-8541
 *
 * Geometric Distributed Architecture (GDA): Binary Spatter Code, 4096 bits,
 * XOR / POPCNT / MAJ; sigma^2 as curvature; L = 1 - 2*sigma; 1=1.
 *
 * Native modules live under creation_os/native_m4/ (gda_*_m4.c, the_mind.c, ...).
 * Optional llama.cpp bridge: native_m4/gda_ghost_boot.h (sigma gate on projected logits).
 */
#ifndef CREATION_OS_H
#define CREATION_OS_H

#define CREATION_OS_VERSION_MAJOR 2
#define CREATION_OS_VERSION_MINOR 0
#define CREATION_OS_VERSION_PATCH 0

/* GDA native modules: core ten + Layer 11 (virtual quantum) + Layer 12 (quantum agency). */
#define CREATION_OS_MODULES   12
#define CREATION_OS_LINES     5362

#define CREATION_OS_DIM_BITS  4096
#define CREATION_OS_DIM_BYTES 512

#define CREATION_OS_AXIOM     "1=1"

/*
 * Scaling profiles — sizes only; architecture unchanged.
 * Override before including this header: #define CREATION_OS_PROFILE CREATION_OS_PROFILE_SERVER
 */
#define CREATION_OS_PROFILE_EDGE    0
#define CREATION_OS_PROFILE_SERVER  1
#define CREATION_OS_PROFILE_CLUSTER 2

#ifndef CREATION_OS_PROFILE
#define CREATION_OS_PROFILE CREATION_OS_PROFILE_EDGE
#endif

#if CREATION_OS_PROFILE == CREATION_OS_PROFILE_EDGE
#define CREATION_OS_CODEBOOK_MAX   4096
#define CREATION_OS_OBS_WINDOW     32
#define CREATION_OS_PAIN_MEMORY    64
#define CREATION_OS_ACTION_MAX     64
#define CREATION_OS_TOOL_MAX       32
#define CREATION_OS_PLAN_MAX       16
#elif CREATION_OS_PROFILE == CREATION_OS_PROFILE_SERVER
#define CREATION_OS_CODEBOOK_MAX   65536
#define CREATION_OS_OBS_WINDOW     256
#define CREATION_OS_PAIN_MEMORY    512
#define CREATION_OS_ACTION_MAX     256
#define CREATION_OS_TOOL_MAX       128
#define CREATION_OS_PLAN_MAX       64
#elif CREATION_OS_PROFILE == CREATION_OS_PROFILE_CLUSTER
#define CREATION_OS_CODEBOOK_MAX   1048576
#define CREATION_OS_OBS_WINDOW     1024
#define CREATION_OS_PAIN_MEMORY    4096
#define CREATION_OS_ACTION_MAX     1024
#define CREATION_OS_TOOL_MAX       512
#define CREATION_OS_PLAN_MAX       256
#endif

#endif /* CREATION_OS_H */
