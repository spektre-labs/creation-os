#include "error_attribution.h"
#include <stdio.h>

int main(void) {
    int rc = cos_error_attribution_self_test();
    if (rc != 0)
        fprintf(stderr, "check-error-attribution failed rc=%d\n", rc);
    return rc != 0 ? 1 : 0;
}
