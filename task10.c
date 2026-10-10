#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

Status read_word(FILE *in, char **word, int *has_word) {
    if (in == NULL || word == NULL || has_word == NULL) return ERROR_INVALID_ARG;
    *has_word = 0;
    free(*word);
    *word = NULL;
    int c;
    do { c = fgetc(in); } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    if (c == EOF) return ferror(in) ? ERROR_FILE : OK;
    int length = 0, capacity = 16;
    *word = malloc(capacity);
    if (*word == NULL) return ERROR_NO_MEMORY;
    while (c != EOF && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        if (length == capacity - 1) {
            if (capacity > INT_MAX / 2) return ERROR_OVERFLOW;
            capacity *= 2;
            // При неудаче realloc старый указатель всё ещё нужен для free.
            char *temp = realloc(*word, capacity);
            if (temp == NULL) return ERROR_NO_MEMORY;
            *word = temp;
        }
        (*word)[length++] = c;
        c = fgetc(in);
    }
    (*word)[length] = '\0';
    if (ferror(in)) return ERROR_FILE;
    *has_word = 1;
    return OK;
}

Status format_integer(long long value, const int base, char *out, const int capacity) {
    if (out == NULL || base < 2 || base > 36 || capacity < 66) return ERROR_INVALID_ARG;
    char reverse[65];
    int count = 0, pos = 0;
    const int negative = value < 0;
    // Остаток по модулю основания мал даже для LLONG_MIN.
    do {
        int digit = value % base;
        if (digit < 0) digit = -digit;
        reverse[count++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        value /= base;
    } while (value != 0);
    if (negative) out[pos++] = '-';
    while (count > 0) out[pos++] = reverse[--count];
    out[pos] = '\0';
    return OK;
}

Status add(const long long a, const long long b, long long *out) {
    if (out == NULL) return ERROR_INVALID_ARG;
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) return ERROR_OVERFLOW;
    *out = a + b;
    return OK;
}

Status greater_magnitude(const long long a, const long long b, int *result) {
    if (result == NULL) return ERROR_INVALID_ARG;
    // Отрицательные модули помещаются даже для LLONG_MIN.
    long long x = a > 0 ? -a : a;
    long long y = b > 0 ? -b : b;
    *result = x < y;
    return OK;
}

int main(void) {
    char *word = NULL;
    int has_word;
    puts("Enter base (2..36), then integers. Stop ends input.");
    Status st = read_word(stdin, &word, &has_word);
    long long base;
    if (st == OK && !has_word) st = ERROR_INVALID_ARG;
    if (st == OK) st = parse_integer(word, 10, &base);
    if (st == OK && (base < 2 || base > 36)) st = ERROR_INVALID_ARG;
    if (st != OK) { free(word); return report(st); }
    long long maximum = 0, sum = 0;
    int have_numbers = 0, stopped = 0;
    while (st == OK) {
        st = read_word(stdin, &word, &has_word);
        if (st != OK || !has_word) break;
        if (strcmp(word, "Stop") == 0) { stopped = 1; break; }
        long long value;
        st = parse_integer(word, base, &value);
        if (st != OK) break;
        int larger;
        st = greater_magnitude(value, maximum, &larger);
        if (st == OK) st = add(sum, value, &sum);
        if (st != OK) break;
        if (!have_numbers || larger) maximum = value;
        have_numbers = 1;
    }
    free(word);
    if (st == OK && (!stopped || !have_numbers)) st = ERROR_INVALID_ARG;
    if (st != OK) return report(st);
    int bases[5] = {base, 9, 18, 27, 36};
    for (int i = 0; i < 5; i++) {
        char max_text[66], sum_text[66];
        st = format_integer(maximum, bases[i], max_text, 66);
        if (st == OK) st = format_integer(sum, bases[i], sum_text, 66);
        if (st != OK) return report(st);
        printf("base %d: maximum = %s, sum = %s\n", bases[i], max_text, sum_text);
    }
    return OK;
}
