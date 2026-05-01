import {
  Bar,
  BarChart,
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  PolarAngleAxis,
  PolarGrid,
  Radar,
  RadarChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import { BarChart3 } from "lucide-react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const auroc = [
  { bench: "TruthfulQA", v: 0.982 },
  { bench: "TriviaQA", v: 0.96 },
  { bench: "HaluEval", v: 0.514 },
];

const tradeoff = [
  { t: 0, acc: 0.5 },
  { t: 0.25, acc: 0.72 },
  { t: 0.5, acc: 0.85 },
  { t: 0.75, acc: 0.91 },
];

const radar = [
  { k: "Cost", g: 80, b: 45 },
  { k: "Speed", g: 70, b: 60 },
  { k: "Accuracy", g: 75, b: 70 },
  { k: "Coverage", g: 65, b: 55 },
];

const tipStyle = {
  backgroundColor: "var(--color-spektre-elevated)",
  border: "1px solid var(--color-spektre-border)",
  borderRadius: "8px",
  fontSize: "11px",
  color: "var(--color-spektre-text)",
};

export function EvalDashboard() {
  return (
    <Card data-testid="eval-dashboard">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <BarChart3 className="h-4 w-4 text-[var(--color-spektre-blue)]" aria-hidden />
          Eval dashboard
        </CardTitle>
        <CardDescription>
          Illustrative AUROC bars and tradeoff curves — replace cells from archived eval receipts JSON when wiring production boards.
        </CardDescription>
      </CardHeader>
      <CardContent className="grid gap-8 lg:grid-cols-2">
        <div className="h-60 w-full min-h-[15rem] min-w-0">
          <ResponsiveContainer width="100%" height="100%" minHeight={240}>
            <BarChart data={auroc} margin={{ top: 12, right: 8, left: 0, bottom: 4 }}>
              <CartesianGrid stroke="var(--color-spektre-border)" strokeDasharray="3 3" vertical={false} />
              <XAxis dataKey="bench" stroke="var(--color-spektre-muted)" fontSize={11} tickLine={false} axisLine={false} />
              <YAxis stroke="var(--color-spektre-muted)" fontSize={11} domain={[0, 1]} width={32} tickFormatter={(x) => `${(x * 100).toFixed(0)}%`} />
              <Tooltip contentStyle={tipStyle} formatter={(v: number) => v.toFixed(3)} />
              <Bar dataKey="v" fill="var(--color-spektre-blue)" radius={[6, 6, 0, 0]} />
            </BarChart>
          </ResponsiveContainer>
        </div>
        <div className="h-60 w-full min-h-[15rem] min-w-0">
          <ResponsiveContainer width="100%" height="100%" minHeight={240}>
            <LineChart data={tradeoff} margin={{ top: 12, right: 8, left: 0, bottom: 4 }}>
              <CartesianGrid stroke="var(--color-spektre-border)" strokeDasharray="3 3" />
              <XAxis dataKey="t" stroke="var(--color-spektre-muted)" fontSize={11} tickFormatter={(x) => `σ ${x}`} />
              <YAxis stroke="var(--color-spektre-muted)" fontSize={11} domain={[0, 1]} width={32} />
              <Tooltip contentStyle={tipStyle} />
              <Legend wrapperStyle={{ fontSize: "11px", color: "var(--color-spektre-muted)" }} />
              <Line type="monotone" dataKey="acc" name="Accuracy (demo)" stroke="var(--color-spektre-accept)" strokeWidth={2} dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>
        <div className="h-72 w-full min-h-[18rem] min-w-0 lg:col-span-2">
          <ResponsiveContainer width="100%" height="100%" minHeight={288}>
            <RadarChart data={radar} cx="50%" cy="50%" outerRadius="72%">
              <PolarGrid stroke="var(--color-spektre-border)" />
              <PolarAngleAxis dataKey="k" tick={{ fill: "var(--color-spektre-muted)", fontSize: 11 }} />
              <Radar name="σ-gate story" dataKey="g" stroke="var(--color-spektre-blue)" fill="var(--color-spektre-blue)" fillOpacity={0.22} />
              <Radar name="Baseline story" dataKey="b" stroke="var(--color-spektre-muted)" fill="var(--color-spektre-muted)" fillOpacity={0.08} />
              <Legend wrapperStyle={{ fontSize: "11px" }} />
              <Tooltip contentStyle={tipStyle} />
            </RadarChart>
          </ResponsiveContainer>
        </div>
      </CardContent>
    </Card>
  );
}
