#!/usr/bin/env julia
# Julia: run merge-gate from CREATION_OS_ROOT
cd(get(ENV, "CREATION_OS_ROOT", "."))
try
    run(`make merge-gate`)
catch e
    if e isa Base.ProcessFailedException
        exit(e.procs[end].exitcode)
    end
    rethrow(e)
end
