#ifndef CREATION_OS_ORION_INTEGRATION_H
#define CREATION_OS_ORION_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Placeholder for Orion MIL → ANE dispatch (full path lives in libcreation_os + ObjC).
 * Returns 0 if stub “success” for smoke, -1 if unavailable.
 */
int creation_os_orion_ane_forward_stub(const float *in, float *out, int n_floats);

#ifdef __cplusplus
}
#endif

#endif
