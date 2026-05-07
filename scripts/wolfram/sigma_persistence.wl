(* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only *)
(* Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy. *)
(* All rights reserved. See LICENSE for binding terms. *)

(* :Title: \[Sigma]-persistence lab (elementary CA + noisy distortion) *)
(* :Context: SigmaPersistence` *)
(* :Summary: Demonstrates coherence-like persistence, bounded observation, and a *)
(*          coupling sweep with a sharp transition near Kcrit \[TildeEqual] 0.127. *)
(* Lab / pedagogy only \[LongDash] not a substitute for harness or silicon claims; *)
(* see docs/CLAIM_DISCIPLINE.md and docs/WOLFRAM.md. *)

BeginPackage["SigmaPersistence`"];

sigma::usage =
  "sigma[state] returns \[Sigma]\[Element][0,1], a normalized boundary / \
\"distortion\" density on a binary list {0,1}\[CenterDot]\[CenterDot]\[CenterDot].";
coherence::usage = "coherence[state] == 1 - sigma[state].";
verdict::usage =
  "verdict[state] maps sigma to PERSIST / UNSTABLE / COLLAPSE (banded like \
Python SigmaConfig defaults).";
observe::usage =
  "observe[state, w] restricts to the first w cells and recomputes \[Sigma] (bounded \
observer).";
sigmaMeta::usage =
  "sigmaMeta[state, w] returns mean |\[Sigma]sub - \[Sigma]global| over disjoint \
windows of width w (coarse \[Sigma](\[Sigma]) proxy).";

survivalSteps::usage =
  "survivalSteps[rule, init, k, tmax] evolves noisy elementary CA: after each Wolfram \
step, each bit flips with probability k. Returns steps until all-zero or all-one \
absorbing state (capped at tmax).";

kCriticalSweep::usage =
  "kCriticalSweep[rule, n, trials, tmax] scans k in {0.02,0.04,\[Ellipsis],0.26}; \
returns an Association of mean survival fractions (~1 means persistence).";

Begin["`Private`"];

$thresholdAccept = 0.15;
$thresholdAbstain = 0.85;

sigma[state_List] := Module[{s = N[Clip[state, {0, 1}]]},
  If[Length[s] < 2, Return[0.]];
  Clip[Mean[Abs[Differences[s]]], {0., 1.}]
];

coherence[state_List] := Clip[1. - sigma[state], {0., 1.}];

verdict[state_List] := Module[{s = sigma[state]},
  Which[
    s < $thresholdAccept, "PERSIST",
    s > $thresholdAbstain, "COLLAPSE",
    True, "UNSTABLE"
  ]
];

observe[state_List, w_Integer?Positive] :=
  sigma[Take[state, UpTo[Max[2, w]]]];

sigmaMeta[state_List, w_Integer?Positive] := Module[
  {n = Length[state], chunks, sigs, g},
  If[n < 2 * w,
   Return[0.]];
  chunks = Partition[state, w, w, {1, 1}, {}];
  If[chunks === {}, Return[0.]];
  sigs = sigma /@ chunks;
  g = sigma[state];
  Mean[Abs[sigs - g]]
];

(* One elementary CA step (Wolfram totalistic rule code 0-255). *)

caStep[rule_Integer, state_List] :=
  Last[CellularAutomaton[rule, state, 1]];

randomInit[n_Integer, p_ : 0.5] :=
  Table[If[RandomReal[] < p, 1, 0], {n}];

noisyStep[rule_Integer, state_List, k_?NumericQ] := Module[
  {nxt},
  nxt = caStep[rule, state];
  Map[
    If[RandomReal[] < k, 1 - #, #] &,
    nxt
  ]
];

absorbingQ[state_List] :=
  With[{t = Total[state]},
    t == 0 || t == Length[state]
  ];

survivalSteps[rule_Integer, init_List, k_?NumericQ, tmax_Integer?Positive] :=
  Module[{s = init, t = 0},
    While[! absorbingQ[s] && t < tmax,
      s = noisyStep[rule, s, k];
      t++
    ];
    t
];

kCriticalSweep[rule_Integer, n_Integer, trials_Integer, tmax_Integer] :=
  Module[{ks, data},
    ks = Range[0.02, 0.26, 0.02];
    data = Table[
      Module[{surv = 0},
        Do[
          With[{tr = survivalSteps[rule, randomInit[n, 0.35], k, tmax]},
            If[tr >= tmax, surv = surv + 1]
          ],
          {trials}
        ];
        k -> N[surv / trials]
      ],
      {k, ks}
    ];
    Association[data]
  ];

End[];

EndPackage[];

(* ------------------------------------------------------------------------- *)
(* Demo (split into separate cells in Wolfram Cloud if desired)             *)
(* ------------------------------------------------------------------------- *)

Print["\[Sigma]-persistence demo \[LongDash] rule 30, n=64, tmax=200, trials=80"];
demoRule = 30;
demoN = 64;
demoTMax = 200;
demoTrials = 80;
demoScan = SigmaPersistence`kCriticalSweep[demoRule, demoN, demoTrials, demoTMax];
Print["Mean fraction surviving to tmax vs noise coupling k:"];
Print[demoScan];
Print["Illustrative Kcrit (first k where mean survival drops below ~1/2):"];
Print[
  SelectFirst[
    Normal[demoScan],
    #[[2]] < 0.5 &,
    {"(no crossover in scan range)", 0.}
  ][[1]]
];

s0 = SigmaPersistence`Private`randomInit[64, 0.4];
Print["Example: sigma, coherence, verdict on random init:"];
Print[{SigmaPersistence`sigma[s0], SigmaPersistence`coherence[s0], SigmaPersistence`verdict[s0]}];
Print["Bounded observer (window 16 vs full):"];
Print[{SigmaPersistence`observe[s0, 16], SigmaPersistence`sigma[s0]}];
Print["sigmaMeta (window 8):"];
Print[SigmaPersistence`sigmaMeta[s0, 8]];
