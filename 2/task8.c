#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;

Status report(const Status st) {
    if (st == ERROR_INVALID_ARG) fprintf(stderr, "Error: invalid argument or digit\n");
    else if (st == ERROR_OVERFLOW) fprintf(stderr, "Error: string too long\n");
    else if (st == ERROR_NO_MEMORY) fprintf(stderr, "Error: cannot allocate memory\n");
    else if (st == ERROR_FILE) fprintf(stderr, "Error: output failure\n");
    return st;
}

Status digit_value(const char c, const int base, int *out) {
    if (out == NULL || base < 2 || base > 36) return ERROR_INVALID_ARG;
    int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
    if (digit < 0 || digit >= base) return ERROR_INVALID_ARG;
    *out = digit;
    return OK;
}

Status check_number(const char *text, const int base, int *length) {
    if (text == NULL || length == NULL || base < 2 || base > 36 || *text == '\0')
        return ERROR_INVALID_ARG;
    int count = 0;
    while (text[count] != '\0') {
        int digit;
        Status st = digit_value(text[count], base, &digit);
        if (st != OK) return st;
        if (count == INT_MAX - 2) return ERROR_OVERFLOW;
        count++;
    }
    *length = count;
    return OK;
}

Status add_numbers(const char *left, const char *right, const int base, char **out) {
    if (out == NULL) return ERROR_INVALID_ARG;
    int a, b;
    Status st = check_number(left, base, &a);
    if (st == OK) st = check_number(right, base, &b);
    if (st != OK) return st;
    const int size = (a > b ? a : b) + 2;
    char *result = malloc(size);
    if (result == NULL) return ERROR_NO_MEMORY;
    int pos = size - 1, carry = 0;
    result[pos] = '\0';
    while (a > 0 || b > 0 || carry != 0) {
        int x = 0, y = 0;
        if (a > 0) digit_value(left[--a], base, &x);
        if (b > 0) digit_value(right[--b], base, &y);
        const int sum = x + y + carry;
        const int digit = sum % base;
        result[--pos] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        carry = sum / base;
    }
    while (result[pos] == '0' && result[pos + 1] != '\0') pos++;
    memmove(result, result + pos, size - pos);
    *out = result;
    return OK;
}

Status sum_numbers(const int base, const int count, char **out, ...) {
    if (base < 2 || base > 36 || count <= 0 || out == NULL) return ERROR_INVALID_ARG;
    char *sum = malloc(2);
    if (sum == NULL) return ERROR_NO_MEMORY;
    strcpy(sum, "0");
    va_list args;
    va_start(args, out);
    Status st = OK;
    for (int i = 0; i < count && st == OK; i++) {
        const char *text = va_arg(args, const char *);
        char *next = NULL;
        st = add_numbers(sum, text, base, &next);
        if (st == OK) { free(sum); sum = next; }
    }
    va_end(args);
    if (st != OK) { free(sum); return st; }
    *out = sum;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;
    Status st = OK;
    int base1 = 16;
    int base2 = 10;
    int base3 = 2;
    int base4 = 36;

    char *sum1 = NULL;
    char *sum2 = NULL;
    char *sum3 = NULL;
    char *sum4 = NULL;


    st = sum_numbers(base1, 3, &sum1, "FFFFFFFFFFFFFFFF", "1", "000A");
    if (st == OK) printf("base %d: %s\n", base1, sum1);
    else { free(sum1); return report(st); }
    free(sum1);

    st = sum_numbers(base2, 3, &sum2, "99999999999999999999", "1", "0000");
    if (st == OK) printf("base %d: %s\n", base2, sum2);
    else { free(sum2); return report(st); }
    free(sum2);

    st = sum_numbers(base3, 4, &sum3, "1010", "11", "0001", "11111111");
    if (st == OK) printf("base %d: %s\n", base3, sum3);
    else { free(sum3); return report(st); }
    free(sum3);

    st = sum_numbers(base4, 3, &sum4, "A", "1", "0");
    if (st == OK) printf("base %d: %s\n", base4, sum4);
    else { free(sum4); return report(st); }
    free(sum4);

    if (st == OK && (fflush(stdout) == EOF || ferror(stdout))) st = ERROR_FILE;
    return report(st);
}