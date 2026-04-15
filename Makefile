# SPDX-License-Identifier: AGPL-3.0-or-later
# One command: make all
CC = cc
CFLAGS = -O2 -march=native -Wall -std=c11
LDFLAGS = -lm
BUILDDIR = .build

.PHONY: help standalone core oracle bench bench-coherence bench-agi-gate physics test check all clean publish-github

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

help:
	@echo "Creation OS — make targets"
	@echo "  help       — this list"
	@echo "  standalone — build creation_os from creation_os_v2.c"
	@echo "  test       — run tests/test_bsc_core (sigma, Noether, crystal)"
	@echo "  check      — standalone + test (use before PR / CI)"
	@echo "  publish-github — rsync this tree → fresh creation-os clone + push main (needs auth)"
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

standalone: creation_os_v2.c
	$(CC) $(CFLAGS) -I. -o creation_os creation_os_v2.c $(LDFLAGS)

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

all: standalone oracle bench physics test
	@echo "All targets built successfully."

clean:
	rm -rf $(BUILDDIR) creation_os oracle_speaks oracle_ultimate gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon genesis qhdc test_bsc

publish-github:
	@bash tools/publish_to_creation_os_github.sh
