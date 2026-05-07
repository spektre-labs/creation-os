# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(shutil.which("gcc") is None, reason="gcc not installed")
def test_sigma_gate_c89_compiles(tmp_path: Path) -> None:
    """Compile v49 σ-gate C unit (C99; uses isfinite)."""
    c_file = _REPO / "src" / "v49" / "sigma_gate.c"
    assert c_file.is_file()
    obj = tmp_path / "sigma_gate_v49_compile_test.o"
    subprocess.run(
        [
            "gcc",
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-c",
            str(c_file),
            "-o",
            str(obj),
        ],
        check=True,
        cwd=str(_REPO),
    )


@pytest.mark.skipif(shutil.which("g++") is None, reason="g++ not installed")
def test_esp32_sigma_compiles(tmp_path: Path) -> None:
    """Syntax-check ESP32 sketch with minimal Arduino stubs (no SDK)."""
    ino = _REPO / "scripts" / "hardware" / "esp32_sigma.ino"
    assert ino.is_file()
    (tmp_path / "Arduino.h").write_text(
        textwrap.dedent(
            """
            #pragma once
            #include <math.h>
            #include <stdio.h>
            #include <string>
            class Print {
            public:
                void println(const char* s) { (void)s; }
            };
            class String {
                std::string _b;

            public:
                String() = default;
                explicit String(const char* s) : _b(s ? s : "") {}
                const char* c_str() const { return _b.c_str(); }
                size_t length() const { return _b.size(); }
            };
            class HardwareSerial : public Print {
            public:
                void begin(unsigned) {}
                int available() { return 0; }
                String readStringUntil(char) { return String(""); }
                template <typename... Args>
                void printf(const char* fmt, Args... args) {
                    std::printf(fmt, args...);
                }
            };
            inline void delay(unsigned) {}
            extern HardwareSerial Serial;
            HardwareSerial Serial;
            """
        ),
        encoding="utf-8",
    )
    cpp = tmp_path / "esp32_sigma_syntax.cpp"
    cpp.write_text(
        "#include <Arduino.h>\n"
        + '#line 1 "esp32_sigma.ino"\n'
        + ino.read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    subprocess.run(
        ["g++", "-std=gnu++17", "-fsyntax-only", str(cpp), f"-I{tmp_path}"],
        check=True,
        cwd=str(_REPO),
    )


def test_bitnet_readme_exists() -> None:
    doc = _REPO / "docs" / "BITNET_L0_INTEGRATION.md"
    assert doc.is_file()
    body = doc.read_text(encoding="utf-8")
    assert "BitNet" in body
    assert "claim discipline" in body.lower()
    upstream = _REPO / "third_party" / "bitnet" / "README.md"
    assert upstream.is_file()


def test_risc_v_spec_exists() -> None:
    p = _REPO / "docs" / "RISC_V_XSIGMA.md"
    assert p.is_file()
    text = p.read_text(encoding="utf-8")
    assert "SPECIFICATION ONLY" in text
    assert "RISC-V" in text
