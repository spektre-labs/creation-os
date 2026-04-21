/* Stand-alone self-test entry for AGI-1 ICL composer. */
#include "inplace_ttt.h"

#include <stdio.h>

int main(void) {
    int rc = cos_inplace_ttt_self_test();
    if (rc != 0) fprintf(stderr, "inplace_ttt: self-test FAIL (rc=%d)\n", rc);
    return rc == 0 ? 0 : 1;
}
