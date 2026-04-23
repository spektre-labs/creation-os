#include "state_ledger.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rc = cos_state_ledger_self_test();
    if (rc != 0)
        fprintf(stderr, "check-state-ledger failed rc=%d\n", rc);
    return rc != 0 ? 1 : 0;
}
