#lang racket/base
;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; racket polyglot/racket/cos_gate.rkt
(require racket/system)
(define root (or (getenv "CREATION_OS_ROOT") "."))
(define sh (format "cd ~a && exec make merge-gate" root))
(exit (if (system* "/bin/sh" "-c" sh) 0 1))
