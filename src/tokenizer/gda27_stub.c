/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Tier-3 stand-in: base-27 literal codec (morphology / GDA integration is roadmap).
 */
#include <ctype.h>
#include <string.h>

#include "cos_tokenizer.h"

static const char GDA27_DIGITS[] = "0123456789abcdefghijklmnopq";

int gda27_encode_uint(uint32_t v, char *out27, int cap)
{
    if (cap < 2)
        return -1;
    if (v == 0) {
        out27[0] = GDA27_DIGITS[0];
        out27[1] = '\0';
        return 1;
    }
    char tmp[16];
    int n = 0;
    uint32_t x = v;
    while (x > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = GDA27_DIGITS[x % 27u];
        x /= 27u;
    }
    int w = 0;
    for (int i = n - 1; i >= 0 && w < cap - 1; i--)
        out27[w++] = tmp[i];
    out27[w] = '\0';
    return w;
}

uint32_t gda27_decode_uint(const char *in27)
{
    uint32_t acc = 0;
    for (const unsigned char *p = (const unsigned char *)in27; *p; p++) {
        const char *pos = strchr(GDA27_DIGITS, tolower(*p));
        if (!pos)
            break;
        acc = acc * 27u + (uint32_t)(pos - GDA27_DIGITS);
    }
    return acc;
}
