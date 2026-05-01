/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "proof_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PA(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s\n", msg); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    PA(cos_proof_receipt_self_test() == 0, "self_test");

    {
        struct cos_proof_receipt rec;
        struct cos_error_attribution attr;
        float sch[4] = { 0.f, 0.f, 0.f, 0.f };
        float mch[4] = { 1.f, 1.f, 1.f, 1.f };

        memset(&attr, 0, sizeof attr);
        PA(cos_proof_receipt_generate("test", 0.5f, sch, mch,
                                      0, &attr, 0xffULL, &rec) == 0,
           "generate");
        PA(cos_proof_receipt_verify(&rec) == 0, "verify_gen");
        PA(rec.kernels_passed <= 40, "popcount");
    }

    {
        char *js = NULL;
        struct cos_proof_receipt rec;
        struct cos_error_attribution attr;
        float sch[4] = { 0.1f, 0.2f, 0.f, 0.f };
        float mch[4];

        memset(&attr, 0, sizeof attr);
        memset(&rec, 0, sizeof rec);
        mch[0] = mch[1] = mch[2] = mch[3] = 0.f;
        cos_proof_receipt_generate("x", 1.f, sch, mch, 2, &attr, 1ULL,
                                   &rec);
        js = cos_proof_receipt_to_json(&rec);
        PA(js != NULL && strstr(js, "\"proof_receipt\"") != NULL, "json");
        free(js);
    }

    {
        struct cos_proof_receipt rec;
        struct cos_proof_receipt rec2;
        struct cos_error_attribution attr;
        float sch[4] = { 0.f, 0.f, 0.f, 0.f };
        float mch[4];

        memset(&attr, 0, sizeof attr);
        mch[0] = mch[1] = mch[2] = mch[3] = 0.f;
        PA(cos_proof_receipt_generate("persist", 0.f, sch, mch, 0, &attr,
                                     3ULL, &rec)
               == 0,
           "gen_persist");
        PA(cos_proof_receipt_verify(&rec) == 0, "verify_persist");
        memcpy(&rec2, &rec, sizeof rec);
        rec2.sigma_combined += 1.f;
        memcpy(rec2.receipt_hash, rec.receipt_hash, 32);
        PA(cos_proof_receipt_verify(&rec2) != 0, "tamper_json_hash");
    }

    {
        char tmpl[] = "/tmp/cos_pr_ck_XXXXXX";
        char path[512];
        int  fd = mkstemp(tmpl);

        PA(fd >= 0, "mkstemp_pr");
        close(fd);
        snprintf(path, sizeof path, "%s.json", tmpl);

        struct cos_proof_receipt rec;
        struct cos_error_attribution attr;
        float sch[4];

        memset(&attr, 0, sizeof attr);
        sch[0] = sch[1] = sch[2] = sch[3] = 0.25f;
        PA(cos_proof_receipt_generate("disk", 0.4f, sch, sch, 0, &attr,
                                      15ULL, &rec)
               == 0,
           "disk_gen");
        setenv("HOME", "/tmp", 1);
        PA(cos_proof_receipt_persist(&rec, path) == 0, "persist_file");
        remove(path);
        remove(tmpl);
    }

    PA(cos_proof_receipt_chain_verify_dir("/nonexistent_cos_rcpt_xyz")
           != 0,
       "missing_dir");

    {
        char              td[] = "/tmp/cos_pr_audit_XXXXXX";
        const char       *oldh;
        struct cos_proof_receipt       rec;
        struct cos_proof_receipt_options opt;
        struct cos_error_attribution   attr;
        float sch[4] = {0.11f, 0.12f, 0.13f, 0.14f};
        float mch[4] = {0.f, 0.f, 0.f, 0.f};
        char  ap[768];

        if (mkdtemp(td) == NULL)
            return 1;
        oldh = getenv("HOME");
        if (setenv("HOME", td, 1) != 0) {
            rmdir(td);
            return 1;
        }
        memset(&attr, 0, sizeof attr);
        memset(&opt, 0, sizeof opt);
        opt.codex_fnv64     = 0xabcdd001ULL;
        opt.model_id        = "audit-test";
        opt.codex_version   = "seed";
        opt.timestamp_ms    = 1700000000000LL;
        opt.kernels_run     = 10;
        opt.prompt_bind     = "What is 2+2?";
        PA(cos_proof_receipt_generate_with("4", 0.21f, sch, mch, 0, &attr,
                                           0xFULL, &opt, &rec)
               == 0,
           "audit_gen");
        PA(cos_proof_receipt_audit_append(&rec) == 0, "audit_append");
        PA(cos_proof_receipt_audit_default_path(ap, sizeof ap) == 0,
           "audit_default_path");
        PA(cos_proof_receipt_audit_verify_jsonl(ap, stdout) == 0,
           "audit_verify_jsonl");
        (void)unlink(ap);
        if (oldh != NULL)
            (void)setenv("HOME", oldh, 1);
        else
            (void)unsetenv("HOME");
    }

    return 0;
}

