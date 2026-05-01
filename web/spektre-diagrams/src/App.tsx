import { EvidenceLadder } from "./components/EvidenceLadder";
import { EngramMemory } from "./components/EngramMemory";
import { EvalDashboard } from "./components/EvalDashboard";
import { GateFlow } from "./components/GateFlow";
import { HeroOverview } from "./components/HeroOverview";
import { OmegaLoop } from "./components/OmegaLoop";
import { SigmaCore } from "./components/SigmaCore";
import { SignalCascade } from "./components/SignalCascade";

const nav = [
  ["Hero", "#spektre-hero"],
  ["σ core", "#spektre-sigma"],
  ["Pipeline", "#spektre-pipeline"],
  ["Cascade", "#spektre-cascade"],
  ["Evidence", "#spektre-evidence"],
  ["Ω loop", "#spektre-omega"],
  ["Memory", "#spektre-engram"],
  ["Eval", "#spektre-eval"],
] as const;

function ShellLink({ href, children }: { href: string; children: string }) {
  return (
    <a
      href={href}
      className="rounded-md px-2 py-1 text-xs text-[var(--color-spektre-muted)] transition-colors hover:bg-[var(--color-spektre-border)]/40 hover:text-[var(--color-spektre-text)]"
    >
      {children}
    </a>
  );
}

export default function App() {
  return (
    <div className="min-h-screen pb-20">
      <header className="sticky top-0 z-20 border-b border-[var(--color-spektre-border)]/60 bg-[var(--color-spektre-bg)]/75 backdrop-blur-xl">
        <div className="mx-auto flex max-w-6xl flex-col gap-3 px-5 py-4 sm:flex-row sm:items-center sm:justify-between">
          <div>
            <p className="text-[10px] font-medium uppercase tracking-[0.2em] text-[var(--color-spektre-faint)]">
              Spektre · diagrams
            </p>
            <h1 className="mt-0.5 text-lg font-semibold tracking-tight text-[var(--color-spektre-text)]">
              Creation OS — σ visuals
            </h1>
          </div>
          <nav className="flex flex-wrap gap-1" aria-label="Section navigation">
            {nav.map(([label, href]) => (
              <ShellLink key={href} href={href}>
                {label}
              </ShellLink>
            ))}
          </nav>
        </div>
      </header>

      <main className="mx-auto flex max-w-6xl flex-col gap-10 px-5 pt-10">
        <section id="spektre-hero" className="scroll-mt-28">
          <HeroOverview />
        </section>
        <section id="spektre-sigma" className="scroll-mt-28">
          <SigmaCore />
        </section>
        <section id="spektre-pipeline" className="scroll-mt-28">
          <GateFlow />
        </section>
        <section id="spektre-cascade" className="scroll-mt-28">
          <SignalCascade />
        </section>
        <section id="spektre-evidence" className="scroll-mt-28">
          <EvidenceLadder />
        </section>
        <section id="spektre-omega" className="scroll-mt-28">
          <OmegaLoop />
        </section>
        <section id="spektre-engram" className="scroll-mt-28">
          <EngramMemory />
        </section>
        <section id="spektre-eval" className="scroll-mt-28">
          <EvalDashboard />
        </section>
      </main>
    </div>
  );
}
