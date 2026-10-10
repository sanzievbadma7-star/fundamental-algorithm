#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW
} Status;

// Накопляем отрицательное число, чтобы не брать модуль LLONG_MIN.
Status parse_integer(const char *text, const int base, long long *out) {
    if (text == NULL || out == NULL || base < 2 || base > 36) return ERROR_INVALID_ARG;
    int negative = 0;
    if (*text == '-' || *text == '+') {
        negative = *text == '-';
        text++;
    }
    if (*text == '\0') return ERROR_INVALID_ARG;
    long long value = 0;
    const long long limit = negative ? LLONG_MIN : -LLONG_MAX;
    while (*text != '\0') {
        int digit;
        if (*text >= '0' && *text <= '9') digit = *text - '0';
        else if (*text >= 'A' && *text <= 'Z') digit = *text - 'A' + 10;
        else return ERROR_INVALID_ARG;
        if (digit >= base) return ERROR_INVALID_ARG;
        if (value < (limit + digit) / base) return ERROR_OVERFLOW;
        value = value * base - digit;
        text++;
    }
    *out = negative ? value : -value;
    return OK;
}

Status parse_real(const char *text, double *out) {
    if (text == NULL || out == NULL || *text == '\0') return ERROR_INVALID_ARG;
    const char *p = text;
    int digits = 0, nonzero = 0;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') {
        if (*p != '0') nonzero = 1;
        digits++; p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (*p != '0') nonzero = 1;
            digits++; p++;
        }
    }
    if (digits == 0) return ERROR_INVALID_ARG;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (*p < '0' || *p > '9') return ERROR_INVALID_ARG;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p != '\0') return ERROR_INVALID_ARG;
    double value = strtod(text, NULL);
    if (!isfinite(value) || (nonzero && fabs(value) < DBL_MIN)) return ERROR_OVERFLOW;
    *out = value;
    return OK;
}

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number out of range\n"); break;
    }
    return status;
}

Status single_flag(const char *text, char *mode) {
    if (text == NULL || mode == NULL || strlen(text) != 2 ||
        (text[0] != '-' && text[0] != '/')) return ERROR_INVALID_ARG;
    *mode = text[1];
    return OK;
}

// count = -1: тождество 0 = 0.
Status quadratic(const double a, const double b, const double c, const double eps,
                 double *x1, double *x2, int *count) {
    if (!isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(eps) ||
        eps <= 0 || x1 == NULL || x2 == NULL || count == NULL) return ERROR_INVALID_ARG;
    *count = 0;
    if (fabs(a) <= eps) {
        if (fabs(b) <= eps) {
            if (fabs(c) <= eps) *count = -1;
        } else { *count = 1; *x1 = -c / b; }
    } else {
        double d = b * b - 4 * a * c;
        if (!isfinite(d)) return ERROR_OVERFLOW;
        if (d < -eps) return OK;
        *count = 1;
        *x1 = -b / (2 * a);
        if (d > eps) {
            *count = 2;
            *x1 = (-b - sqrt(d)) / (2 * a);
            *x2 = (-b + sqrt(d)) / (2 * a);
        }
    }
    if (*count > 0 && !isfinite(*x1)) return ERROR_OVERFLOW;
    if (*count == 2 && !isfinite(*x2)) return ERROR_OVERFLOW;
    return OK;
}

Status right_triangle(double a, double b, double c, const double eps, int *result) {
    if (result == NULL || !isfinite(eps) || eps <= 0 ||
        !isfinite(a) || !isfinite(b) || !isfinite(c)) return ERROR_INVALID_ARG;
    *result = 0;
    if (a <= eps || b <= eps || c <= eps) return OK;
    double temp;
    if (a > b + eps) { temp = a; a = b; b = temp; }
    if (b > c + eps) { temp = b; b = c; c = temp; }
    if (a > b + eps) { temp = a; a = b; b = temp; }
    double difference = a * a + b * b - c * c;
    if (!isfinite(difference) || !isfinite(a + b)) return ERROR_OVERFLOW;
    *result = a + b > c + eps && fabs(difference) <= eps;
    return OK;
}

int main(int argc, char *argv[]) {
    char mode;
    if (argc < 2 || single_flag(argv[1], &mode) != OK) return report(ERROR_INVALID_ARG);
    if (mode == 'm') {
        long long a, b;
        if (argc != 4) return report(ERROR_INVALID_ARG);
        Status st = parse_integer(argv[2], 10, &a);
        if (st == OK) st = parse_integer(argv[3], 10, &b);
        if (st != OK) return report(st);
        if (a == 0 || b == 0) return report(ERROR_INVALID_ARG);
        // Остаток LLONG_MIN / -1 вычислять нельзя.
        puts(b == -1 || a % b == 0 ? "divisible" : "not divisible");
        return OK;
    }
    if ((mode != 'q' && mode != 't') || argc != 6) return report(ERROR_INVALID_ARG);
    double eps, v[3];
    Status st = parse_real(argv[2], &eps);
    if (st != OK) return report(st);
    if (eps <= 0) return report(ERROR_INVALID_ARG);
    for (int i = 0; i < 3; i++) {
        st = parse_real(argv[i + 3], &v[i]);
        if (st != OK) return report(st);
    }
    if (mode == 't') {
        int result;
        st = right_triangle(v[0], v[1], v[2], eps, &result);
        if (st == OK) puts(result ? "right triangle" : "not a right triangle");
        return report(st);
    }
    double used[6][3];
    int used_count = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (i == j) continue;
            int k = 3 - i - j;
            double a = v[i], b = v[j], c = v[k];
            int repeated = 0;
            for (int p = 0; p < used_count; p++) {
                if (fabs(a - used[p][0]) <= eps && fabs(b - used[p][1]) <= eps &&
                    fabs(c - used[p][2]) <= eps) repeated = 1;
            }
            if (repeated) continue;
            used[used_count][0] = a; used[used_count][1] = b; used[used_count][2] = c;
            used_count++;
            int count;
            double x1 = 0, x2 = 0;
            st = quadratic(a, b, c, eps, &x1, &x2, &count);
            if (st != OK) return report(st);
            printf("(%g, %g, %g): ", a, b, c);
            if (count == -1) puts("any real x");
            else if (count == 0) puts("no real roots");
            else if (count == 1) printf("x = %.12g\n", x1);
            else printf("x1 = %.12g, x2 = %.12g\n", x1, x2);
        }
    }
    return OK;
}
