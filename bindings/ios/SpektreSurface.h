/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SpektreSurface.h — C-ABI umbrella header for iOS / iPadOS / macOS
 * consumers of v76 σ-Surface.  Drop into an Xcode target that links
 * libspektre_surface.a (produced by `make v76-ios`) and import
 * `SpektreSurface` from Swift or `#import <SpektreSurface/SpektreSurface.h>`
 * from Objective-C / Objective-C++.
 *
 * The entire surface is pure C with no Foundation / UIKit / Metal
 * dependencies, so this header is equally usable from Swift Package
 * Manager, CocoaPods, Carthage, or a raw .framework bundle.
 */

#ifndef SPEKTRE_SURFACE_H
#define SPEKTRE_SURFACE_H

/* The real declarations live in the kernel header.  The iOS module
 * map re-exports this header as `SpektreSurface`. */
#include "../../src/v76/surface.h"

#endif /* SPEKTRE_SURFACE_H */
