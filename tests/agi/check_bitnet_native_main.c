#include "bitnet_native.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (cos_bitnet_native_self_test() != 0) {
        fprintf(stderr, "FAIL bitnet_native self_test\n");
        return 1;
    }
    return 0;
}
