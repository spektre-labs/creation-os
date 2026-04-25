#include "text_similarity.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int f = cos_text_similarity_self_check();
    if (f != 0) {
        fprintf(stderr, "check-text-similarity: FAIL (%d)\n", f);
        return 1;
    }
    return 0;
}
