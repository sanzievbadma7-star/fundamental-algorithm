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
        else if (*text >= 'a' && *text <= 'z') digit = *text - 'a' + 10;
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

Status min_base(const char *word, int *base) {
    if (word == NULL || base == NULL || *word == '\0') return ERROR_INVALID_ARG;
    int biggest = 1, start = word[0] == '-' || word[0] == '+';
    if (word[start] == '\0') return ERROR_INVALID_ARG;
    for (int i = start; word[i]; i++) {
        int digit;
        if (word[i] >= '0' && word[i] <= '9') digit = word[i] - '0';
        else if (word[i] >= 'A' && word[i] <= 'Z') digit = word[i] - 'A' + 10;
        else if (word[i] >= 'a' && word[i] <= 'z') digit = word[i] - 'a' + 10;
        else return ERROR_INVALID_ARG;
        if (digit > biggest) biggest = digit;
    }
    *base = biggest + 1;
    return OK;
}

Status trim_zeros(const char *word, const char **digits, int *negative) {
    if (word == NULL || digits == NULL || negative == NULL || *word == '\0')
        return ERROR_INVALID_ARG;
    *negative = word[0] == '-';
    if (word[0] == '-' || word[0] == '+') word++;
    if (*word == '\0') return ERROR_INVALID_ARG;
    while (word[0] == '0' && word[1] != '\0') word++;
    if (word[0] == '0' && word[1] == '\0') *negative = 0;
    *digits = word;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], argv[2]) == 0) return report(ERROR_INVALID_ARG);
    FILE *in = fopen(argv[1], "r");
    FILE *out = in == NULL ? NULL : fopen(argv[2], "wx");
    Status st = in == NULL || out == NULL ? ERROR_FILE : OK;
    char *word = NULL;
    while (st == OK) {
        int has_word, base;
        st = read_word(in, &word, &has_word);
        if (st != OK || !has_word) break;
        st = min_base(word, &base);
        long long value;
        if (st == OK) st = parse_integer(word, base, &value);
        const char *clean;
        int negative;
        if (st == OK) st = trim_zeros(word, &clean, &negative);
        if (st == OK && fprintf(out, "%s%s %d %lld\n", negative ? "-" : "", clean, base, value) < 0)
            st = ERROR_FILE;
    }
    free(word);
    if (in != NULL && fclose(in) != 0) st = ERROR_FILE;
    if (out != NULL && fclose(out) != 0) st = ERROR_FILE;
    return report(st);
}
