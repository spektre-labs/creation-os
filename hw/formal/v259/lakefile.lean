import Lake
open Lake DSL

package «sigma_formal» where
  -- zero third-party deps; core Lean 4 only.

@[default_target]
lean_lib «Measurement» where
  -- Measurement.lean is the single source file.
  roots := #[`Measurement]
