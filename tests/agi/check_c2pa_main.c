#include "c2pa_sigma.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (cos_c2pa_sigma_self_test() != 0) {
        fprintf(stderr, "FAIL c2pa_sigma self_test\n");
        return 1;
    }
    return 0;
}
