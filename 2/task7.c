#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;
typedef struct { const double *coordinates; } Vector;
typedef struct { double p; const double *matrix; double eps; } NormOptions;
typedef Status (*Norm)(const Vector *, int, const NormOptions *, double *);
typedef struct { Vector *vectors; int count; double maximum; } Longest;

Status report(const Status st) {
    if (st == ERROR_INVALID_ARG) fprintf(stderr, "Error: invalid vector, matrix or argument\n");
    else if (st == ERROR_OVERFLOW) fprintf(stderr, "Error: numerical result out of range\n");
    else if (st == ERROR_NO_MEMORY) fprintf(stderr, "Error: cannot allocate memory\n");
    else if (st == ERROR_FILE) fprintf(stderr, "Error: output failure\n");
    return st;
}

Status check_vector(const Vector *vector, const int n, const NormOptions *options, double *out) {
    if (vector == NULL || vector->coordinates == NULL || n <= 0 || options == NULL || out == NULL ||
        !isfinite(options->eps) || options->eps <= 0) return ERROR_INVALID_ARG;
    for (int i = 0; i < n; i++) if (!isfinite(vector->coordinates[i])) return ERROR_INVALID_ARG;
    return OK;
}

Status norm_infinity(const Vector *vector, const int n, const NormOptions *options, double *out) {
    Status st = check_vector(vector, n, options, out);
    if (st != OK) return st;
    double maximum = 0;
    for (int i = 0; i < n; i++) maximum = fmax(maximum, fabs(vector->coordinates[i]));
    *out = maximum;
    return OK;
}

Status norm_p(const Vector *vector, const int n, const NormOptions *options, double *out) {
    Status st = check_vector(vector, n, options, out);
    if (st != OK) return st;
    if (!isfinite(options->p) || options->p < 1 - options->eps) return ERROR_INVALID_ARG;
    const double p = options->p < 1 ? 1 : options->p;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += pow(fabs(vector->coordinates[i]), p);
        if (!isfinite(sum)) return ERROR_OVERFLOW;
    }
    const double value = pow(sum, 1 / p);
    if (!isfinite(value)) return ERROR_OVERFLOW;
    *out = value;
    return OK;
}

Status check_matrix(const double *matrix, const int n, const double eps) {
    if (matrix == NULL || n <= 0 || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    if ((size_t)n > SIZE_MAX / n / sizeof(double)) return ERROR_OVERFLOW;
    double *copy = malloc((size_t)n * n * sizeof(double));
    if (copy == NULL) return ERROR_NO_MEMORY;
    Status st = OK;
    for (int i = 0; i < n && st == OK; i++) for (int j = 0; j < n; j++) {
        const double a = matrix[(size_t)i * n + j], b = matrix[(size_t)j * n + i];
        if (!isfinite(a) || !isfinite(b) || fabs(a - b) > eps) { st = ERROR_INVALID_ARG; break; }
        copy[(size_t)i * n + j] = a / 2 + b / 2;
    }
    // Для симметричной положительно определённой матрицы все эти ведущие элементы положительны.
    for (int k = 0; k < n && st == OK; k++) {
        const double pivot = copy[(size_t)k * n + k];
        if (!isfinite(pivot)) { st = ERROR_OVERFLOW; break; }
        if (pivot <= eps) { st = ERROR_INVALID_ARG; break; }
        for (int i = k + 1; i < n && st == OK; i++) {
            const double factor = copy[(size_t)i * n + k] / pivot;
            for (int j = k + 1; j < n; j++) {
                copy[(size_t)i * n + j] -= factor * copy[(size_t)k * n + j];
                if (!isfinite(copy[(size_t)i * n + j])) { st = ERROR_OVERFLOW; break; }
            }
        }
    }
    free(copy);
    return st;
}

Status norm_matrix(const Vector *vector, const int n, const NormOptions *options, double *out) {
    Status st = check_vector(vector, n, options, out);
    if (st == OK) st = check_matrix(options->matrix, n, options->eps);
    if (st != OK) return st;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        double row = 0;
        for (int j = 0; j < n; j++) row += options->matrix[(size_t)i * n + j] * vector->coordinates[j];
        sum += row * vector->coordinates[i];
        if (!isfinite(sum) || !isfinite(row)) return ERROR_OVERFLOW;
    }
    if (sum < -options->eps) return ERROR_OVERFLOW;
    *out = sqrt(fmax(sum, 0));
    return OK;
}

Status free_longest(Longest *results, const int count) {
    if (count < 0 || (count > 0 && results == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) free(results[i].vectors);
    free(results);
    return OK;
}

Status longest_vectors(const int n, const int vector_count, const int norm_count,
                       const NormOptions *options, Longest **out, ...) {
    if (n <= 0 || vector_count <= 0 || norm_count <= 0 || options == NULL || out == NULL ||
        !isfinite(options->eps) || options->eps <= 0) return ERROR_INVALID_ARG;
    if ((size_t)vector_count > SIZE_MAX / sizeof(Vector) ||
        (size_t)vector_count > SIZE_MAX / sizeof(double) ||
        (size_t)norm_count > SIZE_MAX / sizeof(Longest)) return ERROR_OVERFLOW;
    Vector *vectors = malloc((size_t)vector_count * sizeof(Vector));
    double *values = malloc((size_t)vector_count * sizeof(double));
    Longest *results = calloc(norm_count, sizeof(Longest));
    if (vectors == NULL || values == NULL || results == NULL) {
        free(vectors); free(values); free(results); return ERROR_NO_MEMORY;
    }
    va_list args;
    va_start(args, out);
    for (int i = 0; i < vector_count; i++) vectors[i] = va_arg(args, Vector);
    Status st = OK;
    for (int k = 0; k < norm_count && st == OK; k++) {
        Norm norm = va_arg(args, Norm);
        if (norm == NULL) { st = ERROR_INVALID_ARG; break; }
        double maximum = 0;
        for (int i = 0; i < vector_count && st == OK; i++) {
            st = norm(&vectors[i], n, options, &values[i]);
            if (st == OK && !isfinite(values[i])) st = ERROR_OVERFLOW;
            if (st == OK) maximum = fmax(maximum, values[i]);
        }
        if (st != OK) break;
        results[k].vectors = malloc((size_t)vector_count * sizeof(Vector));
        if (results[k].vectors == NULL) { st = ERROR_NO_MEMORY; break; }
        results[k].maximum = maximum;
        for (int i = 0; i < vector_count; i++) {
            if (fabs(values[i] - maximum) <= options->eps)
                results[k].vectors[results[k].count++] = vectors[i];
        }
    }
    va_end(args);
    free(vectors); free(values);
    if (st != OK) { free_longest(results, norm_count); return st; }
    *out = results;
    return OK;
}

static void print_results(const Longest *results, int norm_count, int n) {
    for (int k = 0; k < norm_count; k++) {
        printf("norm %d, maximum %.10g:\n", k + 1, results[k].maximum);
        for (int i = 0; i < results[k].count; i++) {
            printf("  (");
            for (int j = 0; j < n; j++) {
                if (j) printf(", ");
                printf("%.10g", results[k].vectors[i].coordinates[j]);
            }
            printf(")\n");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;

    Status st = OK;
    Longest *results = NULL;

    // n = 2, A = diag(4, 1), p = 2
    {
        const double a[] = {3, 4}, b[] = {5, 0}, c[] = {-3, -4};
        const double matrix[] = {4, 0, 0, 1};
        const Vector va = {a}, vb = {b}, vc = {c};
        const NormOptions options = {2, matrix, 1e-9};
        st = longest_vectors(2, 3, 3, &options, &results, va, vb, vc,
                             norm_infinity, norm_p, norm_matrix);
        if (st != OK) return report(st);
        print_results(results, 3, 2);
        free_longest(results, 3);
        printf("\n");
    }

    // n = 3, A - единичная, p = 1 
    {
        const double a[] = {1, 2, 2}, b[] = {2, -2, 1}, c[] = {0, 3, 0};
        const double matrix[] = {1, 0, 0,
                                 0, 1, 0,
                                 0, 0, 1};
        const Vector va = {a}, vb = {b}, vc = {c};
        const NormOptions options = {1, matrix, 1e-9};
        st = longest_vectors(3, 3, 3, &options, &results, va, vb, vc,
                             norm_infinity, norm_p, norm_matrix);
        if (st != OK) return report(st);
        print_results(results, 3, 3);
        free_longest(results, 3);
        printf("\n");
    }

    //n = 3, A = [2 1 0; 1 2 1; 0 1 2], p = 2 
    {
        const double e1[] = {1, 0, 0}, v[] = {1, 1, 1},
                     e2[] = {0, 1, 0}, w[] = {1, -1, 1};
        const double matrix[] = {2, 1, 0,
                                 1, 2, 1,
                                 0, 1, 2};
        const Vector ve1 = {e1}, vv = {v}, ve2 = {e2}, vw = {w};
        const NormOptions options = {2, matrix, 1e-9};
        st = longest_vectors(3, 4, 2, &options, &results, ve1, vv, ve2, vw,
                             norm_p, norm_matrix);
        if (st != OK) return report(st);
        print_results(results, 2, 3);
        free_longest(results, 2);
    }

    if (fflush(stdout) == EOF || ferror(stdout)) st = ERROR_FILE;
    return report(st);
}
