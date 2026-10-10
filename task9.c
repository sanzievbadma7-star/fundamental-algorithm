#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE
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

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number out of range\n"); break;
        case ERROR_NO_MEMORY: fprintf(stderr, "Error: cannot allocate memory\n"); break;
        case ERROR_FILE: fprintf(stderr, "Error: cannot read, write or open file\n"); break;
    }
    return status;
}

#define FIXED_SIZE 20

Status fill(int *array, const int n, const int a, const int b) {
    if (array == NULL || n <= 0 || a > b) return ERROR_INVALID_ARG;
    long long width = (long long)b - a + 1;
    for (int i = 0; i < n; i++) {
        long long random = rand();
        random = random * (RAND_MAX + 1LL) + rand();
        array[i] = a + random % width;
    }
    return OK;
}

Status swap_min_max(int *array, const int n) {
    if (array == NULL || n <= 0) return ERROR_INVALID_ARG;
    int min_index = 0, max_index = 0;
    for (int i = 1; i < n; i++) {
        if (array[i] < array[min_index]) min_index = i;
        if (array[i] > array[max_index]) max_index = i;
    }
    int temp = array[min_index];
    array[min_index] = array[max_index];
    array[max_index] = temp;
    return OK;
}

Status print_array(const int *array, const int n) {
    if (array == NULL || n <= 0) return ERROR_INVALID_ARG;
    for (int i = 0; i < n; i++) if (printf("%d%c", array[i], i + 1 == n ? '\n' : ' ') < 0) return ERROR_FILE;
    return OK;
}

Status build_c(const int *a, const int n, const int *b, const int m, int *c) {
    if (a == NULL || b == NULL || c == NULL || n <= 0 || m <= 0) return ERROR_INVALID_ARG;
    for (int i = 0; i < n; i++) {
        int best = 0;
        long long distance = llabs((long long)a[i] - b[0]);
        for (int j = 1; j < m; j++) {
            long long current = llabs((long long)a[i] - b[j]);
            if (current < distance) { distance = current; best = j; }
        }
        long long value = (long long)a[i] + b[best];
        if (value < INT_MIN || value > INT_MAX) return ERROR_OVERFLOW;
        c[i] = value;
    }
    return OK;
}

int main(int argc, char *argv[]) {
    // srand принимает unsigned int. Приведение здесь не требуется.
    srand(time(NULL));
    if (argc == 3) {
        long long a, b;
        Status st = parse_integer(argv[1], 10, &a);
        if (st == OK) st = parse_integer(argv[2], 10, &b);
        if (st == OK && (a < INT_MIN || b > INT_MAX || a > b)) st = ERROR_INVALID_ARG;
        int array[FIXED_SIZE];
        if (st == OK) st = fill(array, FIXED_SIZE, a, b);
        if (st == OK) st = print_array(array, FIXED_SIZE);
        if (st == OK) st = swap_min_max(array, FIXED_SIZE);
        if (st == OK) st = print_array(array, FIXED_SIZE);
        return report(st);
    }
    if (argc != 1) return report(ERROR_INVALID_ARG);
    int n = 10 + rand() % 9991, m = 10 + rand() % 9991;
    int *a = malloc(n * sizeof(int));
    int *b = malloc(m * sizeof(int));
    int *c = malloc(n * sizeof(int));
    Status st = a == NULL || b == NULL || c == NULL ? ERROR_NO_MEMORY : OK;
    if (st == OK) st = fill(a, n, -1000, 1000);
    if (st == OK) st = fill(b, m, -1000, 1000);
    if (st == OK) st = build_c(a, n, b, m, c);
    if (st == OK) {
        printf("sizes: %d %d\nA: ", n, m);
        st = print_array(a, n);
        if (st == OK) { printf("B: "); st = print_array(b, m); }
        if (st == OK) { printf("C: "); st = print_array(c, n); }
    }
    free(a); free(b); free(c);
    return report(st);
}
