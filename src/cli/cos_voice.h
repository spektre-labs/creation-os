/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos voice — local STT (whisper.cpp HTTP) + cos-chat + macOS say / espeak.
 * External: whisper.cpp server, ffmpeg for mic capture, ./cos-chat binary.
 */
#ifndef COS_CLI_COS_VOICE_H
#define COS_CLI_COS_VOICE_H

#ifdef __cplusplus
extern "C" {
#endif

/** argv are flags after `cos voice`; exe0 is argv[0] from main (path to cos). */
int cos_voice_main(int argc, char **argv, const char *exe0);

#ifdef __cplusplus
}
#endif

#endif /* COS_CLI_COS_VOICE_H */
