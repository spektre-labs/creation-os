/* AGI-4 / AGI-6 — Ω-loop live entry points (--daemon added in AGI-6). */

#ifndef COS_SIGMA_OMEGA_AGI_LIVE_H
#define COS_SIGMA_OMEGA_AGI_LIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns -1 if argv does not request an AGI Ω mode; otherwise the
 * process exit code (0–255). */
int cos_agi_omega_live_dispatch(int argc, char **argv);

#ifdef __cplusplus
}
#endif
#endif
