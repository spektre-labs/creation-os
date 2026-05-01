import { render, screen } from "@testing-library/react";
import type { FC } from "react";
import { EvidenceLadder } from "./components/EvidenceLadder";
import { EngramMemory } from "./components/EngramMemory";
import { EvalDashboard } from "./components/EvalDashboard";
import { GateFlow } from "./components/GateFlow";
import { HeroOverview } from "./components/HeroOverview";
import { OmegaLoop } from "./components/OmegaLoop";
import { SigmaCore } from "./components/SigmaCore";
import { SignalCascade } from "./components/SignalCascade";

const cases: Array<[string, FC]> = [
  ["hero-overview", HeroOverview],
  ["sigma-core", SigmaCore],
  ["gate-flow", GateFlow],
  ["signal-cascade", SignalCascade],
  ["evidence-ladder", EvidenceLadder],
  ["omega-loop", OmegaLoop],
  ["engram-memory", EngramMemory],
  ["eval-dashboard", EvalDashboard],
];

describe("diagram shells", () => {
  test.each(cases)("%s renders", (id, Cmp) => {
    render(<Cmp />);
    expect(screen.getByTestId(id)).toBeInTheDocument();
  });
});
