#include "neural_symbolic.h"

#include "codex_smt.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (cos_codex_smt_self_test() != 0) {
        fprintf(stderr, "FAIL codex_smt self_test\n");
        return 1;
    }
    if (cos_neural_symbolic_self_test() != 0) {
        fprintf(stderr, "FAIL neural_symbolic self_test\n");
        return 2;
    }
    return 0;
}
