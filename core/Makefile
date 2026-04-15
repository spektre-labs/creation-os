# Creation OS — core production targets (does not modify legacy trees).
CC      := clang
CFLAGS  := -O2 -march=native -Wall -Wextra -Werror -std=c11

.PHONY: all clean test bench daemon agi arc test-arc agitb alm test-alm

all: creation_kernel creation_daemon agi_proof arc_agent agitb_binary_tests alm

agi: agi_proof

agi_proof: agi_proof.c hdc.c facts_store.c orchestrator.c \
		kernel_shared.h hdc.h orchestrator.h facts_store.h kernel.h
	$(CC) $(CFLAGS) -o $@ agi_proof.c hdc.c facts_store.c orchestrator.c

creation_kernel: creation_kernel.c hdc.c orchestrator.c facts_store.c \
		kernel_shared.h hdc.h orchestrator.h facts_store.h kernel.h
	$(CC) $(CFLAGS) -o $@ creation_kernel.c hdc.c orchestrator.c facts_store.c

creation_daemon: creation_daemon.c facts_store.c hdc.c facts_store.h kernel_shared.h hdc.h kernel.h
	$(CC) $(CFLAGS) -o $@ creation_daemon.c facts_store.c hdc.c

arc: arc_agent

arc_agent: arc_agent.c kernel_shared.h orchestrator.h
	$(CC) $(CFLAGS) -o $@ arc_agent.c orchestrator.c

bench: creation_kernel
	./creation_kernel --benchmark facts.json

test: creation_kernel
	cd "$(CURDIR)" && ./test_kernel.sh

test-arc: arc_agent
	cd "$(CURDIR)" && ./test_arc_agent.sh

agitb_binary_tests: agitb_binary_tests.c
	$(CC) $(CFLAGS) -o $@ agitb_binary_tests.c

alm: alm_cli.c alm_engine.c alm_bridge.c cognitive_hook.c cognitive_selftest.c learner.c \
		metacognition.c epistemic_drive.c router.c tri_memory.c verifier.c sigma_sampler.c world_model.c \
		world_model_eval.c causal_model.c causal_model_eval.c reasoner.c reasoner_eval.c \
		mcts_reasoner.c facts_store.c hdc.c alm.h cognitive_hook.h learner.h \
		metacognition.h epistemic_drive.h router.h tri_memory.h verifier.h world_model.h causal_model.h \
		reasoner.h mcts_reasoner.h facts_store.h hdc.h kernel.h
	$(CC) $(CFLAGS) -o alm alm_cli.c alm_engine.c alm_bridge.c cognitive_hook.c \
		cognitive_selftest.c learner.c metacognition.c epistemic_drive.c router.c tri_memory.c verifier.c \
		sigma_sampler.c world_model.c world_model_eval.c causal_model.c causal_model_eval.c \
		reasoner.c reasoner_eval.c mcts_reasoner.c facts_store.c hdc.c -lm

test-alm: alm
	cd "$(CURDIR)" && ./alm facts.json "mikä on suomen pääkaupunki"
	cd "$(CURDIR)" && ./alm facts.json "Explain quantum entanglement in one sentence"

clean:
	rm -f creation_kernel creation_daemon agi_proof arc_agent agitb_binary_tests alm
