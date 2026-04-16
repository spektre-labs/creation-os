#!/usr/bin/env nu
# Nushell: merge-gate with structured env
let root = ($env.CREATION_OS_ROOT? | default ".")
cd $root
^make merge-gate
