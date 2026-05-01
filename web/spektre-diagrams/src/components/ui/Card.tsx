import type { HTMLAttributes } from "react";
import { cn } from "../../lib/cn";

export function Card({ className, ...p }: HTMLAttributes<HTMLDivElement>) {
  return (
    <div
      className={cn(
        "rounded-xl border border-[var(--color-spektre-border)] bg-[var(--color-spektre-surface)]/92 shadow-[0_1px_0_0_var(--color-spektre-border-subtle)_inset,0_18px_48px_-24px_oklch(0.08_0.02_260)] backdrop-blur-md",
        className,
      )}
      {...p}
    />
  );
}

export function CardHeader({ className, ...p }: HTMLAttributes<HTMLDivElement>) {
  return <div className={cn("flex flex-col gap-1.5 border-b border-[var(--color-spektre-border)]/80 p-5", className)} {...p} />;
}

export function CardTitle({ className, ...p }: HTMLAttributes<HTMLHeadingElement>) {
  return (
    <h3 className={cn("text-base font-semibold tracking-tight text-[var(--color-spektre-text)]", className)} {...p} />
  );
}

export function CardDescription({ className, ...p }: HTMLAttributes<HTMLParagraphElement>) {
  return <p className={cn("text-sm leading-relaxed text-[var(--color-spektre-muted)]", className)} {...p} />;
}

export function CardContent({ className, ...p }: HTMLAttributes<HTMLDivElement>) {
  return <div className={cn("p-5 pt-4", className)} {...p} />;
}
