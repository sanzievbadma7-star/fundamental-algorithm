#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_CONVERGENCE
} Status;

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
        case ERROR_NO_CONVERGENCE: fprintf(stderr, "Error: requested accuracy was not reached\n"); break;
    }
    return status;
}

Status series(const char mode, const double x, const double eps, double *out) {
    if (out == NULL || !isfinite(x) || !isfinite(eps) || eps <= 0 ||
        mode < 'a' || mode > 'd') return ERROR_INVALID_ARG;
    if ((mode == 'c' || mode == 'd') && fabs(x) >= 1 - eps) return ERROR_INVALID_ARG;
    double term = 1, sum = mode == 'd' ? 0 : 1, absolute_sum = fabs(sum);
    for (int n = 1; n <= 1000000; n++) {
        double ratio;
        switch (mode) {
            case 'a': ratio = x / n; break;
            case 'b': ratio = -x * x / ((2.0 * n - 1) * (2.0 * n)); break;
            // 27n^3/(3n) = 9n^2. Факториалы каждый раз не считаем.
            case 'c': ratio = 9.0 * n * n * x * x / ((3.0 * n - 1) * (3.0 * n - 2)); break;
            default: ratio = -x * x * (2.0 * n - 1) / (2.0 * n); break;
        }
        term *= ratio;
        sum += term;
        absolute_sum += fabs(term);
        if (!isfinite(sum) || !isfinite(term)) return ERROR_OVERFLOW;
        // Большие слагаемые могут уничтожить точность при вычитании.
        if (absolute_sum * DBL_EPSILON > eps / 8) return ERROR_NO_CONVERGENCE;
        double next_ratio;
        int k = n + 1;
        switch (mode) {
            case 'a': next_ratio = fabs(x) / k; break;
            case 'b': next_ratio = x * x / ((2.0 * k - 1) * (2.0 * k)); break;
            case 'c': next_ratio = 9.0 * k * k * x * x / ((3.0 * k - 1) * (3.0 * k - 2)); break;
            default: next_ratio = x * x; break;
        }
        // Хвост оцениваем геометрической прогрессией.
        if (next_ratio < 1 - eps &&
            fabs(term) * next_ratio / (1 - next_ratio) <= eps / 2) {
            *out = sum;
            return OK;
        }
    }
    return ERROR_NO_CONVERGENCE;
}

Status integrand(const char mode, const double x, const double eps, double *out) {
    if (out == NULL || !isfinite(x) || !isfinite(eps) || eps <= 0 ||
        x < 0 || x > 1 || mode < 'a' || mode > 'd') return ERROR_INVALID_ARG;
    switch (mode) {
        case 'a': *out = fpclassify(x) == FP_ZERO ? 1 : log1p(x) / x; break;
        case 'b': *out = exp(-x * x / 2); break;
        case 'c':
            if (x >= 1) return ERROR_INVALID_ARG;
            *out = -log(1 - x); break;
        default: *out = fpclassify(x) == FP_ZERO ? 1 : pow(x, x); break;
    }
    return isfinite(*out) ? OK : ERROR_OVERFLOW;
}

// Средние прямоугольники не попадают в особые точки 0 и 1.
Status rectangles(const char mode, const int n, const double eps, double *out) {
    if (out == NULL || n <= 0 || !isfinite(eps) || eps <= 0 ||
        mode < 'a' || mode > 'd') return ERROR_INVALID_ARG;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        double value;
        Status st = integrand(mode, (i + 0.5) / n, eps, &value);
        if (st != OK) return st;
        sum += value;
    }
    *out = sum / n;
    return isfinite(*out) ? OK : ERROR_OVERFLOW;
}

Status integrate(const char mode, const double eps, double *out) {
    if (out == NULL || !isfinite(eps) || eps <= 0 ||
        mode < 'a' || mode > 'd') return ERROR_INVALID_ARG;
    double previous;
    Status st = rectangles(mode, 1, eps, &previous);
    if (st != OK) return st;
    for (int n = 2; n <= 8388608; n *= 2) {
        double current;
        st = rectangles(mode, n, eps, &current);
        if (st != OK) return st;
        // Для логарифмической особенности c ошибка убывает как 1/n,
        // для остальных интегралов быстрее. Берём запас в четыре раза.
        if (fabs(current - previous) <= eps / 4) { *out = current; return OK; }
        previous = current;
    }
    return ERROR_NO_CONVERGENCE;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return report(ERROR_INVALID_ARG);
    const char *flag = argv[1];
    if (strlen(flag) != 3 || (flag[0] != '-' && flag[0] != '/') ||
        (flag[1] != '1' && flag[1] != '2') || flag[2] < 'a' || flag[2] > 'd' ||
        argc != (flag[1] == '1' ? 4 : 3)) return report(ERROR_INVALID_ARG);
    double eps, x = 0, result;
    Status st = parse_real(argv[argc - 1], &eps);
    if (st == OK && (eps < 1e-12 || eps > 0.1)) st = ERROR_INVALID_ARG;
    if (st == OK && flag[1] == '1') st = parse_real(argv[2], &x);
    if (st == OK) st = flag[1] == '1' ? series(flag[2], x, eps, &result) : integrate(flag[2], eps, &result);
    if (st == OK) printf("%.12g\n", result);
    return report(st);
}
