/* v8 AMX "lock" entrypoints — branch to sk9_bare_metal_amx.s (link both .o). */
.text
.align 2
.globl _sk9_amx_lock_enter
_sk9_amx_lock_enter:
    b       _sk9_bare_amx_enter

.globl _sk9_amx_lock_exit
_sk9_amx_lock_exit:
    b       _sk9_bare_amx_exit
