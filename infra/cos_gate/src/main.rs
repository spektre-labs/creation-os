//! `cos-gate` — spawn `make merge-gate` from `CREATION_OS_ROOT` (default `.`).
//! Run from repo: `cargo run -p cos_gate --manifest-path infra/cos_gate/Cargo.toml`

use std::env;
use std::process::{exit, Command};

fn main() {
    let root = env::var("CREATION_OS_ROOT").unwrap_or_else(|_| ".".into());
    let st = Command::new("make")
        .args(["merge-gate"])
        .current_dir(&root)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("cos-gate: failed to spawn make: {e}");
            exit(127);
        });
    exit(st.code().unwrap_or(1));
}
