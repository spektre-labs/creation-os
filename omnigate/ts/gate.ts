#!/usr/bin/env -S deno run -A
/** Deno: merge-gate with cwd set on the command (no global chdir). */
const root = Deno.env.get("CREATION_OS_ROOT") ?? ".";
const c = new Deno.Command("make", {
  args: ["merge-gate"],
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
const { code } = await c.output();
Deno.exit(code);
