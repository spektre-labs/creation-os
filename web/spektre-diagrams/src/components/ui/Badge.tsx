import type { HTMLAttributes } from "react";
import { cn } from "../../lib/cn";

const variants = {
  default: "border-[var(--color-spektre-border)] bg-[var(--color-spektre-elevated)] text-[var(--color-spektre-text)]",
  accept: "border-[var(--color-spektre-accept)]/35 bg-[var(--color-spektre-accept)]/12 text-[var(--color-spektre-accept)]",
  rethink: "border-[var(--color-spektre-rethink)]/35 bg-[var(--color-spektre-rethink)]/12 text-[var(--color-spektre-rethink)]",
  abstain: "border-[var(--color-spektre-abstain)]/35 bg-[var(--color-spektre-abstain)]/12 text-[var(--color-spektre-abstain)]",
  muted: "border-transparent bg-[var(--color-spektre-border)]/40 text-[var(--color-spektre-muted)]",
  blue: "border-[var(--color-spektre-blue)]/40 bg-[var(--color-spektre-blue)]/10 text-[var(--color-spektre-blue)]",
} as const;

export type BadgeVariant = keyof typeof variants;

export function Badge({
  className,
  variant = "default",
  ...p
}: HTMLAttributes<HTMLSpanElement> & { variant?: BadgeVariant }) {
  return (
    <span
      className={cn(
        "inline-flex items-center rounded-md border px-2 py-0.5 text-xs font-medium leading-none tracking-wide",
        variants[variant],
        className,
      )}
      {...p}
    />
  );
}
