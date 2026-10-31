#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;

typedef struct {
    char *name;
    int size;
    long long importance;
} FileInfo;

typedef struct {
    FileInfo *files;
    int count;
    int total_size;
    long long total_importance;
} Backup;

Status report(const Status st) {
    if (st == ERROR_INVALID_ARG) fprintf(stderr, "Error: invalid input or argument\n");
    else if (st == ERROR_OVERFLOW) fprintf(stderr, "Error: number or table too large\n");
    else if (st == ERROR_NO_MEMORY) fprintf(stderr, "Error: cannot allocate memory\n");
    else if (st == ERROR_FILE) fprintf(stderr, "Error: input or output failure\n");
    return st;
}

Status read_fields(FILE *in, char **line, char **fields, const int count) {
    if (in == NULL || line == NULL || fields == NULL || count <= 0) return ERROR_INVALID_ARG;
    int used = 0, capacity = 64, c;
    char *text = malloc(capacity);
    if (text == NULL) return ERROR_NO_MEMORY;
    Status st = OK;
    while ((c = fgetc(in)) != EOF && c != '\n') {
        if (c == '\0') { st = ERROR_INVALID_ARG; break; }
        if (used == capacity - 1) {
            if (capacity > INT_MAX / 2) { st = ERROR_OVERFLOW; break; }
            char *temp = realloc(text, capacity * 2);
            if (temp == NULL) { st = ERROR_NO_MEMORY; break; }
            text = temp; capacity *= 2;
        }
        text[used++] = c;
    }
    if (ferror(in)) st = ERROR_FILE;
    text[used] = '\0';
    char *token = strtok(text, " \t\r\v\f");
    for (int i = 0; i < count && st == OK; i++) {
        if (token == NULL) st = ERROR_INVALID_ARG;
        else { fields[i] = token; token = strtok(NULL, " \t\r\v\f"); }
    }
    if (token != NULL) st = ERROR_INVALID_ARG;
    if (st != OK) { free(text); return st; }
    *line = text;
    return OK;
}

Status parse_nonnegative(const char *text, const long long limit, long long *out) {
    if (text == NULL || out == NULL || limit < 0) return ERROR_INVALID_ARG;
    if (*text == '+') text++;
    if (*text == '\0') return ERROR_INVALID_ARG;
    long long value = 0;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') return ERROR_INVALID_ARG;
        const int digit = *text - '0';
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10))
            return ERROR_OVERFLOW;
        value = value * 10 + digit;
        text++;
    }
    *out = value;
    return OK;
}

Status free_files(FileInfo *files, const int count) {
    if (count < 0 || (count > 0 && files == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) free(files[i].name);
    free(files);
    return OK;
}

Status read_files(FILE *in, FileInfo **out, int *count, int *capacity) {
    if (in == NULL || out == NULL || count == NULL || capacity == NULL) return ERROR_INVALID_ARG;
    char *line = NULL, *fields[3];
    Status st = read_fields(in, &line, fields, 2);
    long long n = 0, space = 0;
    if (st == OK) st = parse_nonnegative(fields[0], INT_MAX - 1, &n);
    if (st == OK) st = parse_nonnegative(fields[1], INT_MAX - 1, &space);
    free(line);
    if (st != OK) return st;
    if (n == 0) return ERROR_INVALID_ARG;
    if ((size_t)n > SIZE_MAX / sizeof(FileInfo)) return ERROR_OVERFLOW;
    FileInfo *files = calloc((size_t)n, sizeof(FileInfo));
    if (files == NULL) return ERROR_NO_MEMORY;
    for (int i = 0; i < n && st == OK; i++) {
        line = NULL;
        st = read_fields(in, &line, fields, 3);
        long long size = 0, importance = 0;
        if (st == OK) st = parse_nonnegative(fields[1], INT_MAX, &size);
        if (st == OK) st = parse_nonnegative(fields[2], LLONG_MAX, &importance);
        if (st == OK && size == 0) st = ERROR_INVALID_ARG;
        if (st == OK) {
            for (int j = 0; j < i; j++)
                if (strcmp(files[j].name, fields[0]) == 0) st = ERROR_INVALID_ARG;
        }
        if (st == OK) {
            files[i].name = malloc(strlen(fields[0]) + 1);
            if (files[i].name == NULL) st = ERROR_NO_MEMORY;
            else {
                strcpy(files[i].name, fields[0]);
                files[i].size = (int)size;
                files[i].importance = importance;
            }
        }
        free(line);
    }
    if (st == OK) {
        int c;
        while ((c = fgetc(in)) != EOF) if (!isspace((unsigned char)c)) { st = ERROR_INVALID_ARG; break; }
        if (ferror(in)) st = ERROR_FILE;
    }
    if (st != OK) { free_files(files, (int)n); return st; }
    *out = files; *count = (int)n; *capacity = (int)space;
    return OK;
}

Status choose_backup(const FileInfo *files, const int count, const int capacity, Backup *out) {
    if (files == NULL || out == NULL || count <= 0 || capacity < 0) return ERROR_INVALID_ARG;
    if (count == INT_MAX || capacity == INT_MAX) return ERROR_OVERFLOW;
    int space = capacity;
    int sum = 0;
    for (int i = 0; i < count; i++) {
        if (files[i].name == NULL || *files[i].name == '\0' || files[i].size <= 0 ||
            files[i].importance < 0) return ERROR_INVALID_ARG;
        if (sum < capacity) sum += files[i].size > capacity - sum ? capacity - sum : files[i].size;
    }
    if (sum < space) space = sum;
    const size_t columns = (size_t)space + 1;
    if ((size_t)(count + 1) > SIZE_MAX / columns / sizeof(long long) ||
        (size_t)count > SIZE_MAX / sizeof(FileInfo)) return ERROR_OVERFLOW;
    long long *dp = calloc((size_t)(count + 1) * columns, sizeof(long long));
    if (dp == NULL) return ERROR_NO_MEMORY;
    Status st = OK;
    for (int i = 1; i <= count && st == OK; i++) {
        for (int c = 0; c <= space; c++) {
            const long long without = dp[(size_t)(i - 1) * columns + c];
            long long best = without;
            if (files[i - 1].size <= c) {
                const long long previous = dp[(size_t)(i - 1) * columns + c - files[i - 1].size];
                if (files[i - 1].importance > LLONG_MAX - previous) { st = ERROR_OVERFLOW; break; }
                const long long with = previous + files[i - 1].importance;
                if (with > best) best = with;
            }
            dp[(size_t)i * columns + c] = best;
        }
    }
    FileInfo *selected = NULL;
    if (st == OK) {
        selected = malloc((size_t)count * sizeof(FileInfo));
        if (selected == NULL) st = ERROR_NO_MEMORY;
    }
    if (st != OK) { free(dp); return st; }
    Backup result = {selected, 0, 0, dp[(size_t)count * columns + space]};
    int remaining = space;
    for (int i = count; i > 0; i--) {
        if (dp[(size_t)i * columns + remaining] != dp[(size_t)(i - 1) * columns + remaining]) {
            result.files[result.count++] = files[i - 1];
            result.total_size += files[i - 1].size;
            remaining -= files[i - 1].size;
        }
    }
    for (int i = 0; i < result.count / 2; i++) {
        const FileInfo temp = selected[i];
        selected[i] = selected[result.count - 1 - i];
        selected[result.count - 1 - i] = temp;
    }
    free(dp);
    *out = result;
    return OK;
}

Status print_backup(const Backup *backup) {
    if (backup == NULL || backup->count < 0 ||
        (backup->count > 0 && backup->files == NULL)) return ERROR_INVALID_ARG;
    if (printf("TOTAL_SIZE %d\nTOTAL_IMPORTANCE %lld\nFILES %d\n", backup->total_size,
               backup->total_importance, backup->count) < 0) return ERROR_FILE;
    for (int i = 0; i < backup->count; i++) if (puts(backup->files[i].name) == EOF) return ERROR_FILE;
    return fflush(stdout) == EOF ? ERROR_FILE : OK;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;
    FileInfo *files = NULL;
    int count = 0, capacity = 0;
    Backup backup = {NULL, 0, 0, 0};
    Status st = read_files(stdin, &files, &count, &capacity);
    if (st == OK) st = choose_backup(files, count, capacity, &backup);
    if (st == OK) st = print_backup(&backup);
    free(backup.files);
    free_files(files, count);
    return report(st);
}
