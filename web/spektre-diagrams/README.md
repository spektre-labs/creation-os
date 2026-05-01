# Spektre diagrams (v121 scaffold)

Interactive README-style figures: **React 18**, **Vite 6**, **Tailwind CSS v4** (`@tailwindcss/vite`), **Recharts**, **Framer Motion**, **Lucide**. Dark theme only; Spektre OKLCH tokens live in `src/index.css`.

## Commands

```bash
cd web/spektre-diagrams
npm install
npm run dev      # local preview
npm test         # Vitest + Testing Library
npm run build    # production bundle → dist/
```

`Card`/`CardHeader`/… under `src/components/ui/` follow shadcn-style composition; add Radix primitives as you grow features.

## Static PNGs

Static PNG export is **not** wired here yet. The app adds a **sticky section rail** (`#spektre-*` anchors), glass cards, Badge/Tabs primitives, and calmer chart chrome; numbers remain illustrative until bound to receipt JSON.
