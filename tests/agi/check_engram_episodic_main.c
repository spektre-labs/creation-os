#include "engram_episodic.h"
#include <stdio.h>

int main(void) {
    int rc = cos_engram_episodic_self_test();
    if (rc != 0)
        fprintf(stderr, "check-engram-episodic failed rc=%d\n", rc);
    return rc != 0 ? 1 : 0;
}
