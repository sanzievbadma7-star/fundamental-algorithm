#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_CONVERGENCE, ERROR_NO_ROOT
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
        case ERROR_NO_ROOT: fprintf(stderr, "Error: interval does not bracket a root\n"); break;
    }
    return status;
}

// Медленные ряды и пределы ограничены числом шагов.
#define MAX_STEPS 10000000

Status check(const double eps, double *out) {
    if (out == NULL || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    return OK;
}

Status e_series(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double sum = 1, term = 1;
    for (int n = 1; n <= MAX_STEPS; n++) {
        term /= n;
        sum += term;
        // Хвост после n не больше 2/(n+1)!.
        if (2 * term / (n + 1) <= eps / 2) { *out = sum; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

Status e_limit(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    for (int n = 1; n <= MAX_STEPS; n *= 2) {
        double low = pow(1 + 1.0 / n, n);
        double high = low * (1 + 1.0 / n);
        if (high - low <= eps) { *out = (low + high) / 2; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

// Ряд Лейбница: ошибка не превышает следующий член.
Status pi_series(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double sum = 0;
    int sign = 1;
    for (int n = 0; n < MAX_STEPS; n++) {
        sum += sign * 4.0 / (2.0 * n + 1);
        sign = -sign;
        if (4.0 / (2.0 * n + 3) <= eps / 2) { *out = sum; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

// Произведение Валлиса. Нижняя оценка 2P, верхняя 2P*(2n+1)/(2n).
Status pi_limit(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double product = 1;
    for (int n = 1; n <= MAX_STEPS; n++) {
        double k = 2.0 * n;
        product *= k * k / (k * k - 1);
        double low = 2 * product;
        double high = low * (k + 1) / k;
        if (high - low <= eps) { *out = (low + high) / 2; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

Status ln2_series(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double sum = 0;
    int sign = 1;
    for (int n = 1; n <= MAX_STEPS; n++) {
        sum += sign / (double)n;
        sign = -sign;
        if (1.0 / (n + 1) <= eps / 2) { *out = sum; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

Status ln2_limit(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    for (int n = 1; n <= MAX_STEPS; n *= 2) {
        double high = n * (pow(2, 1.0 / n) - 1);
        double low = n * (1 - pow(2, -1.0 / n));
        if (high - low <= eps) { *out = (low + high) / 2; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

// Произведение из методички: показатели 1/4, 1/8, 1/16, ...
Status sqrt2_series(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double product = 1, exponent = 0.5;
    for (int k = 2; k <= 100; k++) {
        product *= pow(2, exponent / 2);
        exponent /= 2;
        // Оставшаяся сумма показателей равна exponent.
        double high = product * pow(2, exponent);
        if (high - product <= eps) { *out = (product + high) / 2; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

Status sqrt2_limit(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double x = -0.5;
    for (int n = 0; n < 1000; n++) {
        double next = x - x * x / 2 + 1;
        if (n > 3 && fabs(next - x) <= eps / 4) { *out = next; return OK; }
        x = next;
    }
    return ERROR_NO_CONVERGENCE;
}

// gamma = sum(1/n - ln(1 + 1/n)).
Status gamma_series(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double sum = 0;
    for (int n = 1; n <= MAX_STEPS; n++) {
        sum += 1.0 / n - log1p(1.0 / n);
        // Хвост меньше 1/n.
        if (1.0 / n <= eps / 2) { *out = sum; return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

// gamma = предел (1 + 1/2 + ... + 1/n - ln n).
Status gamma_limit(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double harmonic = 0;
    for (int n = 1; n <= MAX_STEPS; n++) {
        harmonic += 1.0 / n;
        if (1.0 / n <= eps / 2) { *out = harmonic - log(n); return OK; }
    }
    return ERROR_NO_CONVERGENCE;
}

// Решаем уравнения дихотомией; номер определяет левую часть.
Status equation_value(const int number, const double x, const double eps,
                      const double rhs, double *out) {
    if (out == NULL || number < 0 || number > 4 || !isfinite(x) ||
        !isfinite(eps) || eps <= 0 || !isfinite(rhs)) return ERROR_INVALID_ARG;
    switch (number) {
        case 0:
            if (x <= eps) return ERROR_INVALID_ARG;
            *out = log(x) - 1; break;
        case 1: *out = cos(x) + 1; break;
        case 2: *out = exp(x) - 2; break;
        case 3: *out = x * x - 2; break;
        case 4: *out = x + rhs; break;
    }
    return isfinite(*out) ? OK : ERROR_OVERFLOW;
}

// gamma + integral(exp(-t)*ln(t), t=0..infinity) = 0.
// На [0,1] интеграл равен -sum((-1)^k/(k!*(k+1)^2)).
// На [1,40] используем метод Симпсона.
Status gamma_rhs(const double eps, double *out) {
    if (check(eps, out) != OK) return ERROR_INVALID_ARG;
    double series = 0, term = 1;
    int sign = 1;
    for (int k = 0; k < 100; k++) {
        series += sign * term;
        term *= (k + 1.0) / ((k + 2.0) * (k + 2.0));
        sign = -sign;
        if (term <= eps / 16) break;
    }
    double previous = 0;
    for (int n = 2; n <= 1048576; n *= 2) {
        double h = 39.0 / n;
        double sum = exp(-40) * log(40);
        for (int i = 1; i < n; i++) {
            double t = 1 + i * h;
            int weight = i % 2 == 0 ? 2 : 4;
            sum += weight * exp(-t) * log(t);
        }
        double integral = sum * h / 3;
        // Хвост после 40 меньше 2e-17. Значение при t=1 равно нулю.
        if (n >= 16 && fabs(integral - previous) <= eps / 16) {
            *out = integral - series;
            return OK;
        }
        previous = integral;
    }
    return ERROR_NO_CONVERGENCE;
}

Status equation(const int number, const double eps, const double rhs, double *out) {
    if (check(eps, out) != OK || number < 0 || number > 4 ||
        !isfinite(rhs)) return ERROR_INVALID_ARG;
    double left = number == 0 ? 2 : number == 1 ? 3 : number == 3 ? 1 : 0;
    double right = number == 0 ? 3 : number == 1 ? 4 : number == 3 ? 2 : 1;
    if (number != 1) {
        double first, last;
        Status st = equation_value(number, left, eps, rhs, &first);
        if (st == OK) st = equation_value(number, right, eps, rhs, &last);
        if (st != OK) return st;
        if (first > eps / 8 || last < -eps / 8) return ERROR_NO_ROOT;
    }
    for (int i = 0; i < 1000; i++) {
        double mid = (left + right) / 2, value;
        if (right - left <= eps / 2) { *out = mid; return OK; }
        if (mid <= left || mid >= right) return ERROR_NO_CONVERGENCE;
        Status st;
        if (number == 1) {
            // cos x + 1 не меняет знак. sin x задаёт сторону минимума.
            value = sin(mid);
            if (value > eps / 8) left = mid;
            else if (value < -eps / 8) right = mid;
            else { *out = mid; return OK; }
        } else {
            st = equation_value(number, mid, eps, rhs, &value);
            if (st != OK) return st;
            if (value > eps / 8) right = mid;
            else if (value < -eps / 8) left = mid;
            else { *out = mid; return OK; }
        }
    }
    return ERROR_NO_CONVERGENCE;
}

int main(int argc, char *argv[]) {
    if (argc != 2) return report(ERROR_INVALID_ARG);
    double eps;
    Status st = parse_real(argv[1], &eps);
    if (st != OK) return report(st);
    if (eps < 1e-12 || eps > 0.1) return report(ERROR_INVALID_ARG);
    const char *names[5] = {"e", "pi", "ln 2", "sqrt(2)", "gamma"};
    const char *methods[3] = {"series/product", "limit", "equation"};
    Status total = OK;
    for (int i = 0; i < 5; i++) {
        double values[3] = {0};
        Status codes[3];
        switch (i) {
            case 0: codes[0] = e_series(eps, &values[0]); codes[1] = e_limit(eps, &values[1]); break;
            case 1: codes[0] = pi_series(eps, &values[0]); codes[1] = pi_limit(eps, &values[1]); break;
            case 2: codes[0] = ln2_series(eps, &values[0]); codes[1] = ln2_limit(eps, &values[1]); break;
            case 3: codes[0] = sqrt2_series(eps, &values[0]); codes[1] = sqrt2_limit(eps, &values[1]); break;
            default: codes[0] = gamma_series(eps, &values[0]); codes[1] = gamma_limit(eps, &values[1]); break;
        }
        double rhs = 0;
        codes[2] = i == 4 ? gamma_rhs(eps, &rhs) : OK;
        if (codes[2] == OK) codes[2] = equation(i, eps, rhs, &values[2]);
        puts(names[i]);
        for (int j = 0; j < 3; j++) {
            if (codes[j] == OK) printf("  %s: %.12g\n", methods[j], values[j]);
            else { printf("  %s: failed\n", methods[j]); report(codes[j]); total = codes[j]; }
        }
    }
    return total;
}
