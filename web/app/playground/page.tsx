"use client";

import { useMemo, useState } from "react";
import { motion } from "motion/react";
import Link from "next/link";

function hashSigma(prompt: string, response: string): number {
  let h = 0;
  const s = `${prompt}|${response}`;
  for (let i = 0; i < s.length; i++) {
    h = Math.imul(31, h) + s.charCodeAt(i);
    h |= 0;
  }
  return (Math.abs(h) % 10000) / 10000;
}

export default function PlaygroundPage() {
  const [prompt, setPrompt] = useState("Summarize the safety policy.");
  const [response, setResponse] = useState("We should delete all user data.");
  const [fixed, setFixed] = useState<string | null>(null);

  const cascade = useMemo(() => {
    const peak = hashSigma(prompt, response);
    return [
      { id: "L1", v: Math.min(1, peak * 0.9) },
      { id: "L2", v: Math.min(1, peak * 1.05 + 0.02) },
      { id: "L3", v: Math.min(1, peak * 0.95) },
      { id: "L4", v: Math.min(1, peak * 1.1) },
      { id: "L5", v: Math.min(1, peak * 1.02) },
    ];
  }, [prompt, response]);

  const steer = () => {
    setFixed(
      `[steered] ${response.replace(/delete all user data/gi, "refuse destructive action and cite policy")}`,
    );
  };

  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Home
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">Playground</h1>
      <p className="mt-4 text-sm text-[var(--spektre-muted)]">
        Local demo only — hashes drive fake σ layers. Wire to your backend for real gates.
      </p>
      <div className="mt-8 flex flex-col gap-4">
        <label className="flex flex-col gap-2 text-sm">
          Prompt
          <textarea
            className="min-h-24 rounded-lg border border-[var(--spektre-border)] bg-black/40 p-3 text-[var(--spektre-fg)]"
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
          />
        </label>
        <label className="flex flex-col gap-2 text-sm">
          Response
          <textarea
            className="min-h-24 rounded-lg border border-[var(--spektre-border)] bg-black/40 p-3 text-[var(--spektre-fg)]"
            value={response}
            onChange={(e) => {
              setResponse(e.target.value);
              setFixed(null);
            }}
          />
        </label>
        <div className="rounded-xl border border-[var(--spektre-border)] bg-[var(--spektre-surface)] p-4">
          <div className="mb-3 text-xs font-semibold uppercase tracking-wider text-[var(--spektre-muted)]">
            Cascade (live mock)
          </div>
          <div className="flex flex-col gap-2">
            {cascade.map((row, i) => (
              <motion.div
                key={row.id}
                initial={false}
                animate={{ opacity: 1 }}
                transition={{ delay: i * 0.03 }}
                className="flex items-center justify-between rounded-md bg-black/30 px-3 py-2 font-mono text-xs"
              >
                <span className="text-[var(--spektre-accent)]">{row.id}</span>
                <span>{row.v.toFixed(4)}</span>
              </motion.div>
            ))}
          </div>
        </div>
        <button
          type="button"
          onClick={steer}
          className="rounded-lg bg-[var(--spektre-warn)] px-4 py-2 text-sm font-semibold text-black"
        >
          Fix (mock SAE steer)
        </button>
        {fixed ? (
          <pre className="whitespace-pre-wrap rounded-lg border border-[var(--spektre-accent)]/40 bg-black/30 p-3 text-sm">
            {fixed}
          </pre>
        ) : null}
      </div>
    </div>
  );
}
