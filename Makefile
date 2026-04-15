# SPDX-License-Identifier: AGPL-3.0-or-later
# One command: make all
CC = cc
CFLAGS = -O2 -march=native -Wall -std=c11
LDFLAGS = -lm
BUILDDIR = .build

.PHONY: help merge-gate push-main publish-github standalone standalone-v6 standalone-v7 standalone-v9 standalone-v10 standalone-v11 standalone-v12 standalone-v15 standalone-v16 standalone-v20 standalone-v21 standalone-v22 standalone-v23 standalone-v24 standalone-v25 standalone-v26 core oracle bench bench-coherence bench-agi-gate physics test test-v6 test-v7 test-v9 test-v10 test-v11 test-v12 test-v15 test-v16 test-v20 test-v21 test-v22 test-v23 test-v24 test-v25 test-v26 check check-v6 check-v7 check-v9 check-v10 check-v11 check-v12 check-v15 check-v16 check-v20 check-v21 check-v22 check-v23 check-v24 check-v25 check-v26 all clean

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

help:
	@echo "Creation OS — make targets"
	@echo "  help       — this list"
	@echo "  standalone — build creation_os from creation_os_v2.c"
	@echo "  standalone-v6 — build creation_os_v6 (Living Kernel + self-test)"
	@echo "  standalone-v7 — build creation_os_v7 (Hallucination Killer + self-test)"
	@echo "  standalone-v9 — build creation_os_v9 (Parameters in Silicon + self-test)"
	@echo "  standalone-v10 — build creation_os_v10 (The Real Mind + self-test)"
	@echo "  standalone-v11 — build creation_os_v11 (MatMul-free mind + self-test)"
	@echo "  standalone-v12 — build creation_os_v12 (Tensor mind + self-test)"
	@echo "  standalone-v15 — build creation_os_v15 (Silicon mind + self-test)"
	@echo "  standalone-v16 — build creation_os_v16 (Unified field + self-test)"
	@echo "  standalone-v20 — build creation_os_v20 (Ship mode + self-test)"
	@echo "  standalone-v21 — build creation_os_v21 (AGI sovereign stack + self-test)"
	@echo "  standalone-v22 — build creation_os_v22 (twenty colossal insights + self-test)"
	@echo "  standalone-v23 — build creation_os_v23 (AGI-schematic affordances + self-test)"
	@echo "  standalone-v24 — build creation_os_v24 (arXiv sci-fi echo latches + self-test)"
	@echo "  standalone-v25 — build creation_os_v25 (enterprise pain ledger + self-test)"
	@echo "  standalone-v26 — build creation_os_v26 (Fortune Global 500 echo orbit + self-test)"
	@echo "  test       — run tests/test_bsc_core (sigma, Noether, crystal)"
	@echo "  test-v6    — ./creation_os_v6 --self-test (30 checks; builds v6 first)"
	@echo "  test-v7    — ./creation_os_v7 --self-test (35 checks; builds v7 first)"
	@echo "  test-v9    — ./creation_os_v9 --self-test (41 checks; builds v9 first)"
	@echo "  test-v10   — ./creation_os_v10 --self-test (46 checks; builds v10 first)"
	@echo "  test-v11   — ./creation_os_v11 --self-test (49 checks; builds v11 first)"
	@echo "  test-v12   — ./creation_os_v12 --self-test (52 checks; builds v12 first)"
	@echo "  test-v15   — ./creation_os_v15 --self-test (58 checks; builds v15 first)"
	@echo "  test-v16   — ./creation_os_v16 --self-test (66 checks; builds v16 first)"
	@echo "  test-v20   — ./creation_os_v20 --self-test (86 checks; builds v20 first)"
	@echo "  test-v21   — ./creation_os_v21 --self-test (99 checks; builds v21 first)"
	@echo "  test-v22   — ./creation_os_v22 --self-test (120 checks; builds v22 first)"
	@echo "  test-v23   — ./creation_os_v23 --self-test (141 checks; builds v23 first)"
	@echo "  test-v24   — ./creation_os_v24 --self-test (162 checks; builds v24 first)"
	@echo "  test-v25   — ./creation_os_v25 --self-test (183 checks; builds v25 first)"
	@echo "  test-v26   — ./creation_os_v26 --self-test (204 checks; builds v26 first)"
	@echo "  check      — standalone + test (use before PR / CI)"
	@echo "  check-v6   — standalone-v6 + test-v6 (v6 only)"
	@echo "  check-v7   — standalone-v7 + test-v7 (v7 only)"
	@echo "  check-v9   — standalone-v9 + test-v9 (v9 only)"
	@echo "  check-v10  — standalone-v10 + test-v10 (v10 only)"
	@echo "  check-v11  — standalone-v11 + test-v11 (v11 only)"
	@echo "  check-v12  — standalone-v12 + test-v12 (v12 only)"
	@echo "  check-v15  — standalone-v15 + test-v15 (v15 only)"
	@echo "  check-v16  — standalone-v16 + test-v16 (v16 only)"
	@echo "  check-v20  — standalone-v20 + test-v20 (v20 only)"
	@echo "  check-v21  — standalone-v21 + test-v21 (v21 only)"
	@echo "  check-v22  — standalone-v22 + test-v22 (v22 only)"
	@echo "  check-v23  — standalone-v23 + test-v23 (v23 only)"
	@echo "  check-v24  — standalone-v24 + test-v24 (v24 only)"
	@echo "  check-v25  — standalone-v25 + test-v25 (v25 only)"
	@echo "  check-v26  — standalone-v26 + test-v26 (v26 only)"
	@echo "  merge-gate — portable check + every flagship self-test (v6..v26); same as CI / publish preflight"
	@echo "  push-main    — push THIS tree to https://github.com/spektre-labs/creation-os main (alias of publish-github)"
	@echo "  publish-github — same as push-main (merge-gate + rsync + commit + push; needs auth)"
	@echo "  bench      — GEMM vs BSC microbench (host-dependent throughput)"
	@echo "  bench-coherence — batch Hamming gate (NEON on AArch64, else scalar)"
	@echo "  bench-agi-gate — K-agent parliament + memory-bank argmin (NEON scan)"
	@echo "  oracle     — oracle_speaks, oracle_ultimate"
	@echo "  physics    — genesis, qhdc"
	@echo "  core       — compile core/*.c to .build/*.o"
	@echo "  all        — standalone + oracle + bench + physics + test"
	@echo "  clean      — remove build artifacts"

check: standalone test
	@echo "check: OK (standalone + test)"

# Portable kernel test + all standalone --self-test matrices (204 checks on v26). CI and publish script use this.
merge-gate:
	@$(MAKE) check
	@$(MAKE) check-v6
	@$(MAKE) check-v7
	@$(MAKE) check-v9
	@$(MAKE) check-v10
	@$(MAKE) check-v11
	@$(MAKE) check-v12
	@$(MAKE) check-v15
	@$(MAKE) check-v16
	@$(MAKE) check-v20
	@$(MAKE) check-v21
	@$(MAKE) check-v22
	@$(MAKE) check-v23
	@$(MAKE) check-v24
	@$(MAKE) check-v25
	@$(MAKE) check-v26
	@echo "merge-gate: OK (Creation OS portable + v6..v26 self-tests)"

standalone: creation_os_v2.c
	$(CC) $(CFLAGS) -I. -o creation_os creation_os_v2.c $(LDFLAGS)

standalone-v6: creation_os_v6.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v6 creation_os_v6.c $(LDFLAGS)

standalone-v7: creation_os_v7.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v7 creation_os_v7.c $(LDFLAGS)

standalone-v9: creation_os_v9.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v9 creation_os_v9.c $(LDFLAGS)

standalone-v10: creation_os_v10.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v10 creation_os_v10.c $(LDFLAGS)

standalone-v11: creation_os_v11.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v11 creation_os_v11.c $(LDFLAGS)

standalone-v12: creation_os_v12.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v12 creation_os_v12.c $(LDFLAGS)

standalone-v15: creation_os_v15.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v15 creation_os_v15.c $(LDFLAGS)

standalone-v16: creation_os_v16.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v16 creation_os_v16.c $(LDFLAGS)

standalone-v20: creation_os_v20.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v20 creation_os_v20.c $(LDFLAGS)

standalone-v21: creation_os_v21.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v21 creation_os_v21.c $(LDFLAGS)

standalone-v22: creation_os_v22.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v22 creation_os_v22.c $(LDFLAGS)

standalone-v23: creation_os_v23.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v23 creation_os_v23.c $(LDFLAGS)

standalone-v24: creation_os_v24.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v24 creation_os_v24.c $(LDFLAGS)

standalone-v25: creation_os_v25.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v25 creation_os_v25.c $(LDFLAGS)

standalone-v26: creation_os_v26.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v26 creation_os_v26.c $(LDFLAGS)

core: $(BUILDDIR)
	@for f in core/*.c; do \
	  b=$$(basename "$$f" .c); \
	  $(CC) $(CFLAGS) -c "$$f" -o "$(BUILDDIR)/$$b.o"; \
	done
	@echo "Core objects: $(BUILDDIR)/*.o"

oracle: oracle/oracle_speaks.c oracle/oracle_ultimate.c core/cos_bsc.h core/creation_os.h
	$(CC) $(CFLAGS) -o oracle_speaks oracle/oracle_speaks.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o oracle_ultimate oracle/oracle_ultimate.c $(LDFLAGS)

bench: bench/gemm_vs_bsc.c core/gemm_vs_bsc.c
	$(CC) $(CFLAGS) -o gemm_vs_bsc bench/gemm_vs_bsc.c core/gemm_vs_bsc.c $(LDFLAGS)
	./gemm_vs_bsc

bench-coherence: bench/coherence_gate_batch.c core/cos_bsc.h core/cos_neon_hamming.h
	$(CC) $(CFLAGS) -o coherence_gate_batch bench/coherence_gate_batch.c $(LDFLAGS)
	./coherence_gate_batch

bench-agi-gate: bench/hv_agi_gate_neon.c core/cos_bsc.h core/cos_neon_hamming.h core/cos_neon_retrieval.h core/cos_parliament.h
	$(CC) $(CFLAGS) -o hv_agi_gate_neon bench/hv_agi_gate_neon.c $(LDFLAGS)
	./hv_agi_gate_neon

physics: physics/genesis.c physics/qhdc_core.c
	$(CC) $(CFLAGS) -o genesis physics/genesis.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o qhdc physics/qhdc_core.c $(LDFLAGS)

test: tests/test_bsc_core.c core/creation_os.h core/cos_bsc.h
	$(CC) $(CFLAGS) -o test_bsc tests/test_bsc_core.c $(LDFLAGS)
	./test_bsc

test-v6: standalone-v6
	./creation_os_v6 --self-test

check-v6: standalone-v6 test-v6
	@echo "check-v6: OK (v6 Living Kernel)"

test-v7: standalone-v7
	./creation_os_v7 --self-test

check-v7: standalone-v7 test-v7
	@echo "check-v7: OK (v7 Hallucination Killer)"

test-v9: standalone-v9
	./creation_os_v9 --self-test

check-v9: standalone-v9 test-v9
	@echo "check-v9: OK (v9 Parameters in Silicon)"

test-v10: standalone-v10
	./creation_os_v10 --self-test

check-v10: standalone-v10 test-v10
	@echo "check-v10: OK (v10 The Real Mind)"

test-v11: standalone-v11
	./creation_os_v11 --self-test

check-v11: standalone-v11 test-v11
	@echo "check-v11: OK (v11 MatMul-free mind)"

test-v12: standalone-v12
	./creation_os_v12 --self-test

check-v12: standalone-v12 test-v12
	@echo "check-v12: OK (v12 Tensor mind)"

test-v15: standalone-v15
	./creation_os_v15 --self-test

check-v15: standalone-v15 test-v15
	@echo "check-v15: OK (v15 Silicon mind)"

test-v16: standalone-v16
	./creation_os_v16 --self-test

check-v16: standalone-v16 test-v16
	@echo "check-v16: OK (v16 Unified field)"

test-v20: standalone-v20
	./creation_os_v20 --self-test

check-v20: standalone-v20 test-v20
	@echo "check-v20: OK (v20 Ship mode)"

test-v21: standalone-v21
	./creation_os_v21 --self-test

check-v21: standalone-v21 test-v21
	@echo "check-v21: OK (v21 AGI sovereign stack)"

test-v22: standalone-v22
	./creation_os_v22 --self-test

check-v22: standalone-v22 test-v22
	@echo "check-v22: OK (v22 twenty colossal insights)"

test-v23: standalone-v23
	./creation_os_v23 --self-test

check-v23: standalone-v23 test-v23
	@echo "check-v23: OK (v23 AGI-schematic affordances)"

test-v24: standalone-v24
	./creation_os_v24 --self-test

check-v24: standalone-v24 test-v24
	@echo "check-v24: OK (v24 arXiv sci-fi echo latches)"

test-v25: standalone-v25
	./creation_os_v25 --self-test

check-v25: standalone-v25 test-v25
	@echo "check-v25: OK (v25 enterprise pain ledger)"

test-v26: standalone-v26
	./creation_os_v26 --self-test

check-v26: standalone-v26 test-v26
	@echo "check-v26: OK (v26 Fortune Global 500 echo orbit)"

all: standalone oracle bench physics test
	@echo "All targets built successfully."

clean:
	rm -rf $(BUILDDIR) creation_os creation_os_v6 creation_os_v7 creation_os_v9 creation_os_v10 creation_os_v11 creation_os_v12 creation_os_v15 creation_os_v16 creation_os_v20 creation_os_v21 creation_os_v22 creation_os_v23 creation_os_v24 creation_os_v25 creation_os_v26 oracle_speaks oracle_ultimate gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon genesis qhdc test_bsc

# Canonical product remote: spektre-labs/creation-os (main). See docs/CANONICAL_GIT_REPOSITORY.md
push-main publish-github:
	@bash tools/publish_to_creation_os_github.sh
