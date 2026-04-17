<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# Spektre v76 σ-Surface — native bindings

This directory carries the **platform façades** for the v76 σ-Surface
kernel.  The kernel itself is a single branchless integer-only C file
(`src/v76/surface.c`) that takes **zero third-party dependencies** —
the bindings are thin marshalling shims, not a runtime.

| Platform | Language | Entry |
|---|---|---|
| iOS / iPadOS / macOS / visionOS / watchOS / tvOS | Swift + ObjC | `ios/SpektreSurface.{h,swift,modulemap}` |
| Android (NDK) | Kotlin + JNI | `android/SpektreSurface.kt`, `android/jni/spektre_surface_jni.c`, `android/jni/CMakeLists.txt` |

## iOS

1. Add `src/v76/surface.c`, `src/v76/surface.h`,
   `src/license_kernel/license_attest.c`, `src/license_kernel/license_attest.h`
   to the Xcode target (or vend them as a static library via Swift
   Package Manager).
2. Import the module map from `bindings/ios/module.modulemap`.
3. From Swift:

```swift
import SpektreSurface

let v = Spektre.version
let touch = Spektre.Touch(x: 0.5, y: 0.5, pressure: 0.8,
                          phase: 1, timestampMs: UInt32(Date().timeIntervalSince1970 * 1000))
let (hv, ok) = Spektre.decode(touch)
let allow = Spektre.decide([1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1])
```

## Android (NDK)

1. Copy `bindings/android/jni/` into `app/src/main/jni/`.
2. Point Gradle's CMake block at `CMakeLists.txt`.
3. Add `SpektreSurface.kt` under `app/src/main/java/dev/spektre/surface/`.
4. From Kotlin:

```kotlin
import dev.spektre.surface.SpektreSurface

val v = SpektreSurface.version
val (hv, ok) = SpektreSurface.decode(
    SpektreSurface.Touch(0.5, 0.5, 0.8, phase = 1,
                         timestampMs = System.currentTimeMillis())
)
val allow = SpektreSurface.decide(ByteArray(16) { 1.toByte() })
```

## Messengers + legacy-software bridges

Every messenger protocol id (WhatsApp, Telegram, Signal, iMessage,
RCS, Matrix, XMPP, Discord, Slack, Line) and every legacy-software
capability id (Word, Excel, Outlook, Photoshop, AutoCAD, SolidWorks,
SAP, Salesforce, Figma, Xcode, PostgreSQL, Stripe, AWS, …) lives as
a **stable integer enum** in `src/v76/surface.h`.  The kernel
exposes branchless `match` / `classify` entry points that take a
query hypervector and return the nearest app or protocol together
with its Hamming distance and margin — directly usable as a
capability router on iOS, Android, or any other target without a
single LLM call on the hot path.

The full list is:

* **Messenger protocols (10)** — WhatsApp · Telegram · Signal · iMessage ·
  RCS · Matrix · XMPP · Discord · Slack · Line
* **Office (8)** — Word · Excel · PowerPoint · Outlook · Teams ·
  OneDrive · SharePoint · OneNote
* **Adobe (8)** — Photoshop · Illustrator · Lightroom · Premiere ·
  After Effects · InDesign · Acrobat · XD
* **CAD (8)** — AutoCAD · SolidWorks · Revit · Fusion 360 · Blender ·
  Rhino · CATIA · Inventor
* **ERP / CRM / finance (8)** — SAP · Oracle ERP · Salesforce ·
  HubSpot · QuickBooks · Stripe · Workday · NetSuite
* **Collab (8)** — Figma · Sketch · Notion · Obsidian · Slack · Zoom ·
  Meet · Webex
* **Dev tools (8)** — Xcode · Android Studio · VSCode · IntelliJ ·
  Git · GitHub · GitLab · Jira
* **Databases (8)** — PostgreSQL · MySQL · MongoDB · Redis · SQLite ·
  Elasticsearch · Kafka · Snowflake
* **Cloud / browsers (8)** — AWS · GCP · Azure · Cloudflare · Chrome ·
  Safari · Firefox · Edge

## License

Everything in this directory inherits the primary **SCSL-1.0**
source-available license + **AGPL-3.0** Change-Date fallback from
the repository root.  Commercial use in any for-profit mobile app
or legacy-software bridge requires a signed Commercial License from
Spektre Labs Oy — see [`../COMMERCIAL_LICENSE.md`](../COMMERCIAL_LICENSE.md).
