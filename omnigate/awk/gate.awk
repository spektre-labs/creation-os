# gawk: merge-gate — CREATION_OS_ROOT must be safe (no spaces recommended)
BEGIN {
    root = ENVIRON["CREATION_OS_ROOT"]
    if (root == "") root = "."
    cmd = sprintf("cd %s && exec make merge-gate", root)
    exit system(cmd)
}
