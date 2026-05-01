import {
  createContext,
  useCallback,
  useContext,
  useId,
  useMemo,
  useState,
  type HTMLAttributes,
  type ReactNode,
} from "react";
import { cn } from "../../lib/cn";

type TabsCtx = { value: string; setValue: (v: string) => void; baseId: string };

const Ctx = createContext<TabsCtx | null>(null);

export function Tabs({
  value: valueControlled,
  onValueChange,
  defaultValue = "",
  children,
  className,
}: {
  value?: string;
  onValueChange?: (v: string) => void;
  defaultValue?: string;
  children: ReactNode;
  className?: string;
}) {
  const baseId = useId();
  const [uncontrolled, setUncontrolled] = useState(defaultValue);
  const isControlled = valueControlled !== undefined;
  const value = isControlled ? valueControlled : uncontrolled;

  const setValue = useCallback(
    (v: string) => {
      if (isControlled) onValueChange?.(v);
      else {
        setUncontrolled(v);
        onValueChange?.(v);
      }
    },
    [isControlled, onValueChange],
  );

  const api = useMemo(() => ({ value, setValue, baseId }), [value, setValue, baseId]);
  return (
    <Ctx.Provider value={api}>
      <div className={cn("flex flex-col gap-3", className)}>{children}</div>
    </Ctx.Provider>
  );
}

export function TabsList({ className, ...p }: HTMLAttributes<HTMLDivElement>) {
  return (
    <div
      role="tablist"
      className={cn(
        "inline-flex h-9 flex-wrap items-center gap-1 rounded-lg bg-[var(--color-spektre-border)]/35 p-1",
        className,
      )}
      {...p}
    />
  );
}

export function TabsTrigger({
  value,
  className,
  children,
  ...p
}: HTMLAttributes<HTMLButtonElement> & { value: string }) {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("TabsTrigger outside Tabs");
  const selected = ctx.value === value;
  return (
    <button
      type="button"
      role="tab"
      aria-selected={selected}
      id={`${ctx.baseId}-tab-${value}`}
      aria-controls={`${ctx.baseId}-panel-${value}`}
      onClick={() => ctx.setValue(value)}
      className={cn(
        "rounded-md px-3 py-1.5 text-xs font-medium transition-colors",
        selected
          ? "bg-[var(--color-spektre-surface)] text-[var(--color-spektre-text)] shadow-sm"
          : "text-[var(--color-spektre-muted)] hover:text-[var(--color-spektre-text)]",
        className,
      )}
      {...p}
    >
      {children}
    </button>
  );
}

export function TabsContent({
  value,
  className,
  children,
  ...p
}: HTMLAttributes<HTMLDivElement> & { value: string }) {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("TabsContent outside Tabs");
  if (ctx.value !== value) return null;
  return (
    <div
      role="tabpanel"
      id={`${ctx.baseId}-panel-${value}`}
      aria-labelledby={`${ctx.baseId}-tab-${value}`}
      className={cn("text-sm text-[var(--color-spektre-muted)]", className)}
      {...p}
    >
      {children}
    </div>
  );
}
