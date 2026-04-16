/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "json_esc.h"

#include <string.h>

int cos_json_escape_cstr(const char *in, char *out, size_t cap)
{
    if (!in || !out || cap < 2u)
        return -1;
    size_t w = 0;
    for (; *in; in++) {
        char esc[7];
        const char *e = NULL;
        size_t elen = 0;
        unsigned char c = (unsigned char)*in;
        switch (c) {
        case '"':
            e = "\\\"";
            elen = 2;
            break;
        case '\\':
            e = "\\\\";
            elen = 2;
            break;
        case '\b':
            e = "\\b";
            elen = 2;
            break;
        case '\f':
            e = "\\f";
            elen = 2;
            break;
        case '\n':
            e = "\\n";
            elen = 2;
            break;
        case '\r':
            e = "\\r";
            elen = 2;
            break;
        case '\t':
            e = "\\t";
            elen = 2;
            break;
        default:
            if (c < 0x20u) {
                static const char hexd[] = "0123456789abcdef";
                esc[0] = '\\';
                esc[1] = 'u';
                esc[2] = '0';
                esc[3] = '0';
                esc[4] = hexd[(c >> 4) & 0xfu];
                esc[5] = hexd[c & 0xfu];
                esc[6] = '\0';
                e = esc;
                elen = 6;
            }
            break;
        }
        if (e) {
            if (w + elen + 1u > cap)
                return -1;
            memcpy(out + w, e, elen);
            w += elen;
        } else {
            if (w + 2u > cap)
                return -1;
            out[w++] = (char)c;
        }
    }
    out[w] = '\0';
    return (int)w;
}
