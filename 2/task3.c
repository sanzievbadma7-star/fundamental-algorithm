#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE, ERROR_ESCAPE
} Status;

typedef struct {
    int line;
    int column;
} Position;

typedef struct {
    const char *path;
    Position *matches;
    int count;
} SearchResult;

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: string, position or collection out of range\n"); break;
        case ERROR_NO_MEMORY: fprintf(stderr, "Error: cannot allocate memory\n"); break;
        case ERROR_FILE: fprintf(stderr, "Error: cannot open, read, write or close file\n"); break;
        case ERROR_ESCAPE: fprintf(stderr, "Error: invalid escape sequence in pattern\n"); break;
    }
    return status;
}

Status string_length(const char *text, int *out) {
    if (text == NULL || out == NULL || *text == '\0') return ERROR_INVALID_ARG;
    int length = 0;
    while (text[length] != '\0') {
        if (length == INT_MAX - 1) return ERROR_OVERFLOW;
        length++;
    }
    *out = length;
    return OK;
}

Status append_match(SearchResult *result, int *capacity, const Position position) {
    if (result == NULL || capacity == NULL || result->count < 0 ||
        *capacity < result->count || position.line < 1 || position.column < 1 ||
        (*capacity > 0 && result->matches == NULL)) return ERROR_INVALID_ARG;
    if (result->count == *capacity) {
        if (*capacity > INT_MAX / 2) return ERROR_OVERFLOW;
        const int next = *capacity == 0 ? 16 : *capacity * 2;
        if ((size_t)next > SIZE_MAX / sizeof(Position)) return ERROR_OVERFLOW;
        Position *temp = realloc(result->matches, (size_t)next * sizeof(Position));
        if (temp == NULL) return ERROR_NO_MEMORY;
        result->matches = temp;
        *capacity = next;
    }
    result->matches[result->count++] = position;
    return OK;
}

Status read_text(FILE *in, unsigned char **out, int *length) {
    if (in == NULL || out == NULL || length == NULL) return ERROR_INVALID_ARG;
    int used = 0, capacity = 256;
    unsigned char *text = malloc(capacity);
    if (text == NULL) return ERROR_NO_MEMORY;
    Status st = OK;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (used == capacity) {
            if (capacity > INT_MAX / 2) { st = ERROR_OVERFLOW; break; }
            unsigned char *temp = realloc(text, capacity * 2);
            if (temp == NULL) { st = ERROR_NO_MEMORY; break; }
            text = temp;
            capacity *= 2;
        }
        text[used++] = c;
    }
    if (ferror(in)) st = ERROR_FILE;
    if (st != OK) { free(text); return st; }
    *out = text;
    *length = used;
    return OK;
}

Status find_matches(const unsigned char *text, const int size,
                    const char *pattern, SearchResult *result) {
    if (text == NULL || size < 0 || pattern == NULL || result == NULL ||
        result->matches != NULL || result->count != 0) return ERROR_INVALID_ARG;
    int length;
    Status st = string_length(pattern, &length);
    if (st != OK) return st;
    int last[UCHAR_MAX + 1];
    for (int i = 0; i <= UCHAR_MAX; i++) last[i] = -1;
    for (int i = 0; i < length; i++) last[(unsigned char)pattern[i]] = i;

    int offset = 0, capacity = 0;
    Position position = {1, 1};
    while (offset <= size - length) {
        int j = length - 1;
        while (j >= 0 && (unsigned char)pattern[j] == text[offset + j]) j--;
        int shift = 1;
        if (j < 0) {
            st = append_match(result, &capacity, position);
            if (st != OK) return st;
        } else {
            shift = j - last[text[offset + j]];
            if (shift < 1) shift = 1;
        }
        // После совпадения сдвигаемся на один байт, чтобы сохранить перекрытия.
        for (int i = 0; i < shift; i++) {
            if (text[offset++] == '\n') {
                if (position.line == INT_MAX) return ERROR_OVERFLOW;
                position.line++;
                position.column = 1;
            } else {
                if (position.column == INT_MAX) return ERROR_OVERFLOW;
                position.column++;
            }
        }
    }
    return OK;
}

Status search_file(const char *pattern, const char *path, SearchResult *out) {
    if (pattern == NULL || *pattern == '\0' || path == NULL ||
        *path == '\0' || out == NULL) return ERROR_INVALID_ARG;
    FILE *in = fopen(path, "rb");
    if (in == NULL) return ERROR_FILE;
    unsigned char *text = NULL;
    int length = 0;
    Status st = read_text(in, &text, &length);
    if (fclose(in) == EOF && st == OK) st = ERROR_FILE;
    SearchResult result = {path, NULL, 0};
    if (st == OK) st = find_matches(text, length, pattern, &result);
    free(text);
    if (st != OK) { free(result.matches); return st; }
    *out = result;
    return OK;
}

Status free_results(SearchResult *results, const int count) {
    if (count < 0 || (count > 0 && results == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) free(results[i].matches);
    free(results);
    return OK;
}

Status search_files(const char *pattern, const int file_count, SearchResult **out, ...) {
    if (pattern == NULL || *pattern == '\0' || file_count <= 0 || out == NULL)
        return ERROR_INVALID_ARG;
    if ((size_t)file_count > SIZE_MAX / sizeof(SearchResult)) return ERROR_OVERFLOW;
    SearchResult *results = calloc(file_count, sizeof(SearchResult));
    if (results == NULL) return ERROR_NO_MEMORY;
    va_list args;
    va_start(args, out);
    Status st = OK;
    for (int i = 0; i < file_count && st == OK; i++) {
        const char *path = va_arg(args, const char *);
        st = search_file(pattern, path, &results[i]);
    }
    va_end(args);
    if (st != OK) { free_results(results, file_count); return st; }
    *out = results;
    return OK;
}

Status print_results(const SearchResult *results, const int count) {
    if (count < 0 || (count > 0 && results == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) {
        const SearchResult *result = &results[i];
        if (result->path == NULL || result->count < 0 ||
            (result->count > 0 && result->matches == NULL)) return ERROR_INVALID_ARG;
        if (printf("%s:\n", result->path) < 0) return ERROR_FILE;
        if (result->count == 0 && puts("  no matches") == EOF) return ERROR_FILE;
        for (int j = 0; j < result->count; j++) {
            if (printf("  line %d, column %d\n", result->matches[j].line,
                       result->matches[j].column) < 0) return ERROR_FILE;
        }
    }
    return OK;
}

Status unescape_pattern(const char *in, char **out) {
    if (in == NULL || out == NULL) return ERROR_INVALID_ARG;
    int length;
    Status st = string_length(in, &length);
    if (st != OK) return st;

    char *res = malloc((size_t)length + 1);
    if (res == NULL) return ERROR_NO_MEMORY;

    int used = 0;
    for (int i = 0; i < length; i++) {
        if (in[i] != '\\') {
            res[used++] = in[i];
            continue;
        }
        if (i + 1 >= length) { free(res); return ERROR_ESCAPE; }
        switch (in[++i]) {
            case 'n':  res[used++] = '\n'; break;
            case 't':  res[used++] = '\t'; break;
            case 'r':  res[used++] = '\r'; break;
            case 'a':  res[used++] = '\a'; break;
            case 'b':  res[used++] = '\b'; break;
            case 'f':  res[used++] = '\f'; break;
            case 'v':  res[used++] = '\v'; break;
            case '\\': res[used++] = '\\'; break;
            case '"':  res[used++] = '"';  break;
            case '\'': res[used++] = '\''; break;
            default:
                free(res);
                return ERROR_ESCAPE;
        }
    }
    res[used] = '\0';
    *out = res;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return report(ERROR_INVALID_ARG);

    char *pattern = NULL;
    Status st = unescape_pattern(argv[1], &pattern);
    if (st != OK) return report(st);

    int i = 2;
    while (i < argc && st == OK) {
        SearchResult *results = NULL;
        const int count = argc - i >= 2 ? 2 : 1;
        if (count == 2)
            st = search_files(pattern, 2, &results, (const char *)argv[i], (const char *)argv[i + 1]);
        else
            st = search_files(pattern, 1, &results, (const char *)argv[i]);
        if (st == OK) st = print_results(results, count);
        if (results != NULL) free_results(results, count);
        i += count;
    }
    free(pattern);
    if (st == OK && fflush(stdout) == EOF) st = ERROR_FILE;
    return report(st);
}
