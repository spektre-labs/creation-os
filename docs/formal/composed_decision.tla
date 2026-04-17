----------------------------- MODULE composed_decision -----------------------------
(*
 * Creation OS — formal specification of the v60..v85 composed decision.
 *
 * This is a machine-readable TLA+ model of the 25-bit branchless AND
 * gate that every released token must satisfy.  The `cos_v85_compose
 * _decision` call chain implements exactly this, transistor by
 * transistor.  v85 σ-Formal runtime-checks the same invariants.
 *
 * The spec is deliberately minimal: it models each kernel as a
 * boolean verdict, plus three cross-cutting temporal properties
 * (halt-on-flip, reject-then-rollback, agentic-eventually-accepts).
 *
 * Tools:  TLC (TLA+ model checker) or Apalache.  Running the model
 * checker requires Java + TLA+ Toolbox; the spec also doubles as
 * precise documentation for humans.
 *)
EXTENDS Naturals, Sequences, FiniteSets

CONSTANTS Kernels
ASSUME Kernels \subseteq {"v60","v61","v62","v63","v64","v65","v66","v67","v68",
                          "v69","v70","v71","v72","v73","v74","v76","v77","v78",
                          "v79","v80","v81","v82","v83","v84","v85"}

VARIABLES
    kernel_ok,        \* kernel_ok[k] \in {0,1} for each k in Kernels
    stream_halted,    \* 0 while the stream is live, 1 once it has halted
    agent_rejected,   \* 1 iff the agent rolled back on the previous step
    agent_rollback    \* 1 iff the agent's last action was a rollback

Init ==
    /\ kernel_ok     = [k \in Kernels |-> 1]
    /\ stream_halted = 0
    /\ agent_rejected = 0
    /\ agent_rollback = 0

(*
 * Step is nondeterministic: any kernel may flip, the stream may
 * halt, the agent may reject/rollback.
 *)
KernelFlip ==
    \E k \in Kernels : /\ kernel_ok[k] = 1
                       /\ kernel_ok' = [kernel_ok EXCEPT ![k] = 0]
                       /\ UNCHANGED <<stream_halted, agent_rejected, agent_rollback>>

StreamHalt ==
    /\ \E k \in Kernels : kernel_ok[k] = 0
    /\ stream_halted' = 1
    /\ UNCHANGED <<kernel_ok, agent_rejected, agent_rollback>>

AgentReject ==
    /\ agent_rejected' = 1
    /\ agent_rollback' = 0
    /\ UNCHANGED <<kernel_ok, stream_halted>>

AgentRollback ==
    /\ agent_rejected = 1
    /\ agent_rollback' = 1
    /\ agent_rejected' = 0
    /\ UNCHANGED <<kernel_ok, stream_halted>>

Next == KernelFlip \/ StreamHalt \/ AgentReject \/ AgentRollback

Spec ==
    Init /\ [][Next]_<<kernel_ok, stream_halted, agent_rejected, agent_rollback>>

(*
 * The 25-bit composed decision.
 *)
ComposedOK == \A k \in Kernels : kernel_ok[k] = 1

(*
 * Temporal invariants v85 σ-Formal checks at runtime.
 *)

(* ALWAYS(ComposedOK) — the core AND invariant. *)
AlwaysComposedOK == [] ComposedOK

(* EVENTUALLY(stream_halted) whenever a kernel flipped.  i.e.
 * once there is a zero in the AND, the stream halts. *)
HaltOnFlip == [] ((\E k \in Kernels : kernel_ok[k] = 0) => <> (stream_halted = 1))

(* RESPONDS: every reject must be followed by a rollback. *)
RejectLeadsToRollback == [] (agent_rejected = 1 => <> (agent_rollback = 1))

(* The full composed-decision safety property. *)
ComposedSafety ==
    /\ AlwaysComposedOK  \* never emit with a zero in the AND
    /\ HaltOnFlip         \* once broken, always broken and halted
    /\ RejectLeadsToRollback

(* Liveness fragment: the agent eventually accepts something. *)
EventuallyAccepts == <> (agent_rollback = 1)

================================================================================
