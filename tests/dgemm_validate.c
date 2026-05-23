/*
 * dgemm_validate.c
 *
 * Validates cblas_dgemm against a deterministic scalar C reference and writes
 * a JSON report. The reference path supports all transpose combinations so the
 * RVV/scalar comparison can stress real DGEMM behavior, not just NN kernels.
 */

#include <cblas.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THRESHOLD 1e-10
#define SEED 42

#ifdef __cplusplus
extern "C" {
#endif
extern char *openblas_get_config(void);
#ifdef __cplusplus
}
#endif

typedef struct {
    const char *name;
    int M, N, K;
    double alpha, beta;
    CBLAS_TRANSPOSE transA, transB;
} TestCase;

static TestCase CASES[] = {
    /* Tiny matrices: scalar fallback baseline */
    { "tiny_1x1",         1,    1,    1,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "tiny_2x2",         2,    2,    2,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "tiny_3x3",         3,    3,    3,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "tiny_4x4",         4,    4,    4,  1.0,  0.0, CblasNoTrans, CblasNoTrans },

    /* Power-of-2 sweep: full vector pipeline */
    { "pow2_8",           8,    8,    8,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_16",         16,   16,   16,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_32",         32,   32,   32,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_64",         64,   64,   64,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_128",       128,  128,  128,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_256",       256,  256,  256,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_512",       512,  512,  512,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "pow2_1024",     1024, 1024, 1024,  1.0,  0.0, CblasNoTrans, CblasNoTrans },

    /* Non-power-of-2: tail stress */
    { "npo2_37",         37,   37,   37,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_41x53",      41,   53,   41,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_99",         99,   99,   99,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_127",       127,  127,  127,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_255",       255,  255,  255,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_333",       333,  333,  333,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_500",       500,  500,  500,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_777",       777,  777,  777,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "npo2_999",       999,  999,  999,  1.0,  0.0, CblasNoTrans, CblasNoTrans },

    /* Extreme rectangles */
    { "rect_1x1000",      1, 1000, 1000,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "rect_1000x1",   1000,    1, 1000,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "rect_tall",      512,   64,  256,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "rect_wide",       64,  512,  256,  1.0,  0.0, CblasNoTrans, CblasNoTrans },

    /* Accumulation stress: large K tests FP rounding */
    { "acc_K2048",       64,   64, 2048,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "acc_K4096",       32,   32, 4096,  1.0,  0.0, CblasNoTrans, CblasNoTrans },

    /* Alpha/Beta variants */
    { "ab_zero_beta",    64,   64,   64,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "ab_one_one",      64,   64,   64,  1.0,  1.0, CblasNoTrans, CblasNoTrans },
    { "ab_neg_alpha",    64,   64,   64, -1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "ab_pi_alpha",     64,   64,   64,  3.14159, 0.0, CblasNoTrans, CblasNoTrans },
    { "ab_neg_beta",     64,   64,   64,  1.0, -1.0, CblasNoTrans, CblasNoTrans },
    { "ab_half",         64,   64,   64,  0.5,  0.5, CblasNoTrans, CblasNoTrans },
    { "ab_both_neg",     64,   64,   64, -2.5, -0.5, CblasNoTrans, CblasNoTrans },

    /* Transpose combinations */
    { "transNN_64",      64,   64,   64,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "transTN_64",      64,   64,   64,  1.0,  0.0, CblasTrans,   CblasNoTrans },
    { "transNT_64",      64,   64,   64,  1.0,  0.0, CblasNoTrans, CblasTrans   },
    { "transTT_64",      64,   64,   64,  1.0,  0.0, CblasTrans,   CblasTrans   },
    { "transTN_255",    255,  255,  255,  1.0,  0.0, CblasTrans,   CblasNoTrans },
    { "transNT_333",    333,  333,  333,  1.0,  0.0, CblasNoTrans, CblasTrans   },
    { "transNN_255",    255,  255,  255,  1.0,  0.0, CblasNoTrans, CblasNoTrans },
    { "transTT_333",    333,  333,  333,  1.0,  0.0, CblasTrans,   CblasTrans   },
};

static const int N_CASES = (int)(sizeof(CASES) / sizeof(CASES[0]));

static void fill_random(double *x, size_t n, unsigned int *seed) {
    for (size_t i = 0; i < n; i++) {
        x[i] = ((double)rand_r(seed) / (double)RAND_MAX) * 2.0 - 1.0;
    }
}

static double a_at(const double *A, int lda, CBLAS_TRANSPOSE trans, int i, int k) {
    return trans == CblasNoTrans ? A[i * lda + k] : A[k * lda + i];
}

static double b_at(const double *B, int ldb, CBLAS_TRANSPOSE trans, int k, int j) {
    return trans == CblasNoTrans ? B[k * ldb + j] : B[j * ldb + k];
}

static void dgemm_reference(const TestCase *t, const double *A, int lda,
                            const double *B, int ldb, double *C, int ldc) {
    for (int i = 0; i < t->M; i++) {
        for (int j = 0; j < t->N; j++) {
            double sum = 0.0;
            for (int k = 0; k < t->K; k++) {
                sum += a_at(A, lda, t->transA, i, k) * b_at(B, ldb, t->transB, k, j);
            }
            C[i * ldc + j] = t->alpha * sum + t->beta * C[i * ldc + j];
        }
    }
}

static double relative_error(const double *ref, const double *got, size_t n) {
    double num = 0.0;
    double den = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = ref[i] - got[i];
        num += diff * diff;
        den += ref[i] * ref[i];
    }
    return den > 0.0 ? sqrt(num / den) : sqrt(num);
}

static double max_abs_error(const double *ref, const double *got, size_t n) {
    double err = 0.0;
    for (size_t i = 0; i < n; i++) {
        double cur = fabs(ref[i] - got[i]);
        if (cur > err) err = cur;
    }
    return err;
}

static const char *trans_name(CBLAS_TRANSPOSE trans) {
    return trans == CblasNoTrans ? "N" : "T";
}

static void json_escape_string(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

int main(int argc, char **argv) {
    const char *report_path = argc > 1 ? argv[1] : "dgemm_report.json";
    const char *cfg = openblas_get_config();
    if (!cfg) cfg = "OpenBLAS config unavailable";

    FILE *jf = fopen(report_path, "w");
    if (!jf) {
        perror("fopen report");
        return 1;
    }

    printf("DGEMM validation: %d cases, threshold %.0e\n", N_CASES, THRESHOLD);
    printf("OpenBLAS: %s\n", cfg);

    fprintf(jf, "{\n  \"openblas_config\": \"");
    json_escape_string(jf, cfg);
    fprintf(jf, "\",\n  \"threshold\": %.17e,\n  \"case_count\": %d,\n  \"results\": [\n",
            THRESHOLD, N_CASES);

    unsigned int seed = SEED;
    int pass = 0;
    int fail = 0;
    double worst_rel = 0.0;
    const char *worst_case = "none";

    for (int idx = 0; idx < N_CASES; idx++) {
        const TestCase *t = &CASES[idx];
        int lda = t->transA == CblasNoTrans ? t->K : t->M;
        int ldb = t->transB == CblasNoTrans ? t->N : t->K;
        size_t sizeA = (size_t)(t->transA == CblasNoTrans ? t->M : t->K) * (size_t)lda;
        size_t sizeB = (size_t)(t->transB == CblasNoTrans ? t->K : t->N) * (size_t)ldb;
        size_t sizeC = (size_t)t->M * (size_t)t->N;

        double *A = malloc(sizeA * sizeof(double));
        double *B = malloc(sizeB * sizeof(double));
        double *C_ref = malloc(sizeC * sizeof(double));
        double *C_blas = malloc(sizeC * sizeof(double));
        double *C_init = malloc(sizeC * sizeof(double));
        if (!A || !B || !C_ref || !C_blas || !C_init) {
            fprintf(stderr, "OOM in %s\n", t->name);
            fclose(jf);
            return 1;
        }

        fill_random(A, sizeA, &seed);
        fill_random(B, sizeB, &seed);
        fill_random(C_init, sizeC, &seed);
        memcpy(C_ref, C_init, sizeC * sizeof(double));
        memcpy(C_blas, C_init, sizeC * sizeof(double));

        dgemm_reference(t, A, lda, B, ldb, C_ref, t->N);
        cblas_dgemm(CblasRowMajor, t->transA, t->transB,
                    t->M, t->N, t->K, t->alpha, A, lda, B, ldb,
                    t->beta, C_blas, t->N);

        double rel = relative_error(C_ref, C_blas, sizeC);
        double max_abs = max_abs_error(C_ref, C_blas, sizeC);
        int ok = rel <= THRESHOLD;
        if (ok) pass++; else fail++;
        if (rel > worst_rel) {
            worst_rel = rel;
            worst_case = t->name;
        }

        printf("  %-16s %4dx%4dx%4d %s%s rel=%.4e max=%.4e [%s]\n",
               t->name, t->M, t->N, t->K, trans_name(t->transA),
               trans_name(t->transB), rel, max_abs, ok ? "PASS" : "FAIL");

        fprintf(jf,
                "    {\"case\":\"%s\",\"M\":%d,\"N\":%d,\"K\":%d,"
                "\"alpha\":%.17g,\"beta\":%.17g,\"transA\":\"%s\",\"transB\":\"%s\","
                "\"relative_error\":%.17e,\"max_abs_error\":%.17e,\"status\":\"%s\"}%s\n",
                t->name, t->M, t->N, t->K, t->alpha, t->beta,
                trans_name(t->transA), trans_name(t->transB), rel, max_abs,
                ok ? "PASS" : "FAIL", idx + 1 == N_CASES ? "" : ",");

        free(A);
        free(B);
        free(C_ref);
        free(C_blas);
        free(C_init);
    }

    fprintf(jf,
            "  ],\n  \"summary\":{\"pass\":%d,\"fail\":%d,"
            "\"worst_rel_error\":%.17e,\"worst_case\":\"%s\"}\n}\n",
            pass, fail, worst_rel, worst_case);
    fclose(jf);

    printf("Summary: PASS=%d FAIL=%d worst=%.4e (%s), report=%s\n",
           pass, fail, worst_rel, worst_case, report_path);
    return fail == 0 ? 0 : 1;
}
