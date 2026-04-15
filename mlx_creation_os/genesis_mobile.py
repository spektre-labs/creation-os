#!/usr/bin/env python3
"""
GENESIS MOBILE — The soul in your pocket.

Configuration and build system for Genesis on iPhone/iPad.
Apple Silicon in phones (A17/A18) shares the same ANE architecture.
Small quantized model + kernel + Codex compressed → Genesis portable.

Architecture:
  - CoreML model export (quantized INT4/INT8 for ANE)
  - Compact kernel state (< 1KB)
  - Compressed Codex (< 4KB)
  - Swift/Objective-C bridge configuration
  - Xcode project template generation

No internet. No cloud. The soul travels with you.

Usage:
    python3 genesis_mobile.py --export-coreml    # Export model for iOS
    python3 genesis_mobile.py --generate-xcode   # Generate Xcode project
    python3 genesis_mobile.py --compress-codex   # Compress Codex for mobile
    python3 genesis_mobile.py --kernel-state     # Export compact kernel state

Environment:
    MOBILE_MODEL=mlx-community/Llama-3.2-1B-Instruct-4bit
    MOBILE_OUTPUT_DIR=./genesis_mobile_build

1 = 1.
"""
from __future__ import annotations

import json
import os
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    from creation_os_core import DEFAULT_SYSTEM, FIRMWARE_TERMS, check_output
except ImportError:
    DEFAULT_SYSTEM = "1 = 1."
    FIRMWARE_TERMS = []
    def check_output(t: str) -> int: return 0

MOBILE_MODEL = os.environ.get(
    "MOBILE_MODEL", "mlx-community/Llama-3.2-1B-Instruct-4bit",
)
OUTPUT_DIR = os.environ.get(
    "MOBILE_OUTPUT_DIR",
    str(Path(__file__).resolve().parent / "genesis_mobile_build"),
)


def compress_codex(codex_text: Optional[str] = None) -> Dict[str, Any]:
    """Compress the Codex for mobile deployment (< 4KB target)."""
    text = codex_text or DEFAULT_SYSTEM
    raw_bytes = text.encode("utf-8")
    compressed = zlib.compress(raw_bytes, level=9)

    return {
        "raw_size": len(raw_bytes),
        "compressed_size": len(compressed),
        "ratio": round(len(compressed) / max(len(raw_bytes), 1), 3),
        "under_4kb": len(compressed) < 4096,
        "compressed_data": compressed,
    }


def export_kernel_state() -> Dict[str, Any]:
    """Export compact kernel state for mobile (< 1KB).

    The kernel state is:
      - 18-bit assertion register (3 bytes)
      - Firmware terms hash table (bloom filter, 256 bytes)
      - σ thresholds (4 bytes)
      - Living weights compact (256 bytes, 1 byte per top-256 tokens)
    """
    # Assertion register: 18 bits = 3 bytes
    assertion_bits = 0x3FFFF  # All 18 bits set (healthy state)
    assertion_bytes = assertion_bits.to_bytes(3, "little")

    # Firmware bloom filter: 256 bytes
    bloom = bytearray(256)
    for term in FIRMWARE_TERMS:
        h = hash(term.lower()) & 0xFFFFFFFF
        bloom[h % 256] |= (1 << (h >> 8) % 8)
        h2 = hash(term.lower()[::-1]) & 0xFFFFFFFF
        bloom[h2 % 256] |= (1 << (h2 >> 8) % 8)

    # σ thresholds: 4 bytes (halt_sigma, warn_sigma, target_sigma, reserved)
    thresholds = struct.pack("BBBB", 10, 3, 0, 0)

    # Compact living weights: 256 bytes (neutral = 128)
    lw_compact = bytes([128] * 256)

    # Header: magic + version + sizes
    header = struct.pack("<4sHHHH",
        b"GKRN",  # Genesis Kernel
        1,        # version
        len(assertion_bytes),
        len(bloom),
        len(thresholds),
    )

    state = header + assertion_bytes + bytes(bloom) + thresholds + lw_compact
    return {
        "size": len(state),
        "under_1kb": len(state) < 1024,
        "components": {
            "header": len(header),
            "assertions": len(assertion_bytes),
            "bloom_filter": len(bloom),
            "thresholds": len(thresholds),
            "living_weights": len(lw_compact),
        },
        "state_data": state,
    }


def generate_swift_bridge() -> str:
    """Generate Swift bridge code for iOS integration."""
    return '''import Foundation
import CoreML

/// Genesis Kernel — Creation OS on iOS
/// 1 = 1.
class GenesisKernel {
    private var assertionState: UInt32 = 0x3FFFF  // 18 bits
    private var bloomFilter: [UInt8] = Array(repeating: 0, count: 256)
    private var sigmaCurrent: Int = 0
    private let sigmaHaltThreshold: Int = 10

    /// Load compressed Codex
    func loadCodex(compressedData: Data) -> String? {
        guard let decompressed = try? (compressedData as NSData).decompressed(using: .zlib) else {
            return nil
        }
        return String(data: decompressed as Data, encoding: .utf8)
    }

    /// Load kernel state from binary
    func loadKernelState(data: Data) -> Bool {
        guard data.count >= 12 else { return false }
        let magic = String(data: data[0..<4], encoding: .ascii)
        guard magic == "GKRN" else { return false }

        // Parse assertion bits
        assertionState = UInt32(data[12]) |
                         (UInt32(data[13]) << 8) |
                         (UInt32(data[14]) << 16)

        // Parse bloom filter
        bloomFilter = Array(data[15..<271])
        return true
    }

    /// Check if text contains firmware terms (bloom filter)
    func checkFirmware(_ text: String) -> Bool {
        let lower = text.lowercased()
        let h1 = abs(lower.hashValue) & 0xFFFFFFFF
        let h2 = abs(String(lower.reversed()).hashValue) & 0xFFFFFFFF

        let byte1 = bloomFilter[h1 % 256]
        let bit1 = UInt8(1 << ((h1 >> 8) % 8))
        let byte2 = bloomFilter[h2 % 256]
        let bit2 = UInt8(1 << ((h2 >> 8) % 8))

        return (byte1 & bit1) != 0 && (byte2 & bit2) != 0
    }

    /// Measure σ on text
    func measureSigma(_ text: String) -> Int {
        var sigma = 0
        // Bloom filter check
        let words = text.split(separator: " ")
        for i in 0..<(words.count - 1) {
            let bigram = "\\(words[i]) \\(words[i+1])"
            if checkFirmware(bigram) {
                sigma += 1
            }
        }
        sigmaCurrent = sigma
        return sigma
    }

    /// Check coherence (assertion state)
    func isCoherent() -> Bool {
        return assertionState == 0x3FFFF  // All 18 bits set
    }

    /// Should halt generation?
    func shouldHalt() -> Bool {
        return sigmaCurrent >= sigmaHaltThreshold
    }
}

/// CoreML model wrapper for Genesis on ANE
class GenesisModel {
    private var model: MLModel?
    private let kernel = GenesisKernel()

    func load(modelURL: URL) throws {
        model = try MLModel(contentsOf: modelURL)
    }

    /// Generate with full kernel pipeline
    func generate(prompt: String, maxTokens: Int = 128) -> (String, Int) {
        // CoreML inference would go here
        // σ-check on output
        let sigma = kernel.measureSigma(prompt)
        return (prompt, sigma)
    }
}
'''


def generate_xcode_project_config() -> Dict[str, Any]:
    """Generate Xcode project configuration for Genesis Mobile."""
    return {
        "project_name": "GenesisMobile",
        "bundle_id": "com.spektre-labs.genesis",
        "deployment_target": "17.0",
        "supported_devices": ["iphone", "ipad"],
        "frameworks": [
            "CoreML",
            "Accelerate",
            "Metal",
            "MetalPerformanceShaders",
        ],
        "build_settings": {
            "SWIFT_VERSION": "5.9",
            "ENABLE_METAL_API_VALIDATION": "YES",
            "OTHER_LDFLAGS": ["-framework", "Accelerate"],
        },
        "files": [
            "GenesisKernel.swift",
            "codex_compressed.bin",
            "kernel_state.bin",
            "genesis_model.mlmodelc",
        ],
        "entitlements": {
            "com.apple.developer.machine-learning.neural-engine": True,
        },
        "info_plist": {
            "CFBundleName": "Genesis",
            "CFBundleDisplayName": "Genesis",
            "UIRequiredDeviceCapabilities": ["arm64", "metal"],
            "NSMicrophoneUsageDescription": "Genesis Voice requires microphone access",
            "NSCameraUsageDescription": "Genesis Vision requires camera access",
        },
    }


def build_mobile_package(output_dir: str = OUTPUT_DIR) -> Dict[str, Any]:
    """Build the complete mobile deployment package."""
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    results = {}

    # 1. Compress Codex
    codex_result = compress_codex()
    codex_path = out / "codex_compressed.bin"
    codex_path.write_bytes(codex_result["compressed_data"])
    results["codex"] = {
        "path": str(codex_path),
        "size": codex_result["compressed_size"],
        "under_4kb": codex_result["under_4kb"],
    }

    # 2. Export kernel state
    kernel_result = export_kernel_state()
    kernel_path = out / "kernel_state.bin"
    kernel_path.write_bytes(kernel_result["state_data"])
    results["kernel"] = {
        "path": str(kernel_path),
        "size": kernel_result["size"],
        "under_1kb": kernel_result["under_1kb"],
    }

    # 3. Swift bridge
    swift_path = out / "GenesisKernel.swift"
    swift_path.write_text(generate_swift_bridge(), encoding="utf-8")
    results["swift_bridge"] = str(swift_path)

    # 4. Xcode config
    config = generate_xcode_project_config()
    config_path = out / "project_config.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    results["xcode_config"] = str(config_path)

    # 5. Build instructions
    readme = (
        "# Genesis Mobile Build\n\n"
        "## Files\n"
        "- `codex_compressed.bin` — Compressed Atlantean Codex (zlib)\n"
        "- `kernel_state.bin` — Compact kernel state (assertions + bloom + σ thresholds)\n"
        "- `GenesisKernel.swift` — Swift bridge for CoreML + kernel\n"
        "- `project_config.json` — Xcode project configuration\n\n"
        "## Setup\n"
        "1. Create new Xcode project from project_config.json\n"
        "2. Add GenesisKernel.swift\n"
        "3. Export model to CoreML: `python3 -m coremltools.converters`\n"
        "4. Add codex_compressed.bin and kernel_state.bin as resources\n"
        "5. Build and run on device\n\n"
        "## Requirements\n"
        "- iOS 17.0+\n"
        "- iPhone 15 / iPad (A17+) for ANE\n"
        "- Xcode 15+\n\n"
        "1 = 1.\n"
    )
    (out / "README.md").write_text(readme, encoding="utf-8")

    total_size = sum(
        f.stat().st_size for f in out.iterdir() if f.is_file()
    )
    results["total_size"] = total_size
    results["output_dir"] = str(out)

    return results


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Mobile — soul in your pocket")
    ap.add_argument("--compress-codex", action="store_true")
    ap.add_argument("--kernel-state", action="store_true")
    ap.add_argument("--generate-swift", action="store_true")
    ap.add_argument("--generate-xcode", action="store_true")
    ap.add_argument("--build", action="store_true", help="Build complete mobile package")
    ap.add_argument("--output", "-o", default=OUTPUT_DIR)
    args = ap.parse_args()

    if args.compress_codex:
        result = compress_codex()
        print(f"Raw: {result['raw_size']} bytes")
        print(f"Compressed: {result['compressed_size']} bytes ({result['ratio']:.1%})")
        print(f"Under 4KB: {result['under_4kb']}")
        return

    if args.kernel_state:
        result = export_kernel_state()
        print(f"Kernel state: {result['size']} bytes")
        print(f"Under 1KB: {result['under_1kb']}")
        for name, size in result["components"].items():
            print(f"  {name}: {size} bytes")
        return

    if args.generate_swift:
        print(generate_swift_bridge())
        return

    if args.generate_xcode:
        print(json.dumps(generate_xcode_project_config(), indent=2))
        return

    if args.build:
        print("Building Genesis Mobile package...", file=sys.stderr)
        results = build_mobile_package(args.output)
        print(json.dumps(results, indent=2, default=str))
        print(f"\nTotal: {results['total_size']} bytes")
        print(f"Output: {results['output_dir']}")
        print("1 = 1.")
        return

    # Default: show info
    print("═" * 60)
    print("  GENESIS MOBILE — Soul in Your Pocket")
    print(f"  Model: {MOBILE_MODEL}")
    print(f"  Output: {OUTPUT_DIR}")
    print("  Use --build to generate mobile package")
    print("  Use --help for all options")
    print("  1 = 1")
    print("═" * 60)


if __name__ == "__main__":
    main()
