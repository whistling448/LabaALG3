// Запуск: .\task2.exe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

typedef enum {
    STATUS_OK,
    STATUS_END_OF_FILE,
    STATUS_NULL_POINTER,
    STATUS_INVALID_ARGUMENT,
    STATUS_INVALID_NUMBER,
    STATUS_OUT_OF_RANGE,
    STATUS_INVALID_TIME,
    STATUS_INVALID_INTERVAL,
    STATUS_DUPLICATE_ID,
    STATUS_INVALID_FORMAT,
    STATUS_MEMORY_ERROR,
    STATUS_READ_ERROR
} Status;

typedef struct {
    long long id;
    int start;
    int finish;
} Talk;

Status read_line(FILE *file, char **buffer, size_t *capacity) {
    const size_t start_capacity = 64;
    size_t length = 0;
    char *new_buffer;
    int c;
    if (file == NULL || buffer == NULL || capacity == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (*buffer == NULL) {
        *buffer = (char *)malloc(start_capacity);
        if (*buffer == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        *capacity = start_capacity;
    }
    c = fgetc(file);
    if (c == EOF) {
        (*buffer)[0] = '\0';
        return ferror(file) ? STATUS_READ_ERROR : STATUS_END_OF_FILE;
    }
    while (c != EOF && c != '\n') {
        if (length + 1 >= *capacity) {
            if (*capacity > SIZE_MAX / 2) {
                return STATUS_MEMORY_ERROR;
            }
            new_buffer = (char *)realloc(*buffer, *capacity * 2);
            if (new_buffer == NULL) {
                return STATUS_MEMORY_ERROR;
            }
            *buffer = new_buffer;
            *capacity *= 2;
        }
        (*buffer)[length] = (char)c;
        length++;
        c = fgetc(file);
    }
    if (ferror(file)) {
        return STATUS_READ_ERROR;
    }
    if (length > 0 && (*buffer)[length - 1] == '\r') {
        length--;
    }
    (*buffer)[length] = '\0';
    return STATUS_OK;
}

int is_blank(const char c) {
    return c == ' ' || c == '\t';
}

int split_tokens(char *line, char *tokens[], const int max_tokens) {
    int count = 0;
    char *p = line;
    while (*p != '\0') {
        while (is_blank(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (count == max_tokens) {
            return max_tokens + 1;
        }
        tokens[count] = p;
        count++;
        while (*p != '\0' && !is_blank(*p)) {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return count;
}

Status parse_positive(const char *str, long long *result) {
    long long value = 0;
    int digit;
    if (str == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (*str == '+') {
        str++;
    }
    if (*str == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    for (; *str != '\0'; str++) {
        if (*str < '0' || *str > '9') {
            return STATUS_INVALID_NUMBER;
        }
        digit = *str - '0';
        if (value > (LLONG_MAX - digit) / 10) {
            return STATUS_OUT_OF_RANGE;
        }
        value = value * 10 + digit;
    }
    if (value == 0) {
        return STATUS_OUT_OF_RANGE;
    }
    *result = value;
    return STATUS_OK;
}

Status parse_time(const char *str, int *minutes) {
    int hours;
    int mins;
    if (str == NULL || minutes == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (strlen(str) != 5 || str[2] != ':') {
        return STATUS_INVALID_TIME;
    }
    if (str[0] < '0' || str[0] > '9' || str[1] < '0' || str[1] > '9' ||
        str[3] < '0' || str[3] > '9' || str[4] < '0' || str[4] > '9') {
        return STATUS_INVALID_TIME;
    }
    hours = (str[0] - '0') * 10 + (str[1] - '0');
    mins = (str[3] - '0') * 10 + (str[4] - '0');
    if (hours > 23 || mins > 59) {
        return STATUS_INVALID_TIME;
    }
    *minutes = hours * 60 + mins;
    return STATUS_OK;
}

int compare_by_id(const void *first, const void *second) {
    const Talk *a = (const Talk *)first;
    const Talk *b = (const Talk *)second;
    return (a->id > b->id) - (a->id < b->id);
}

int compare_by_finish(const void *first, const void *second) {
    const Talk *a = (const Talk *)first;
    const Talk *b = (const Talk *)second;
    if (a->finish != b->finish) {
        return (a->finish > b->finish) - (a->finish < b->finish);
    }
    return (a->start > b->start) - (a->start < b->start);
}

Status find_duplicate_id(const Talk talks[], const size_t count, int *found, long long *duplicate) {
    Talk *sorted;
    size_t i;
    if (talks == NULL || found == NULL || duplicate == NULL) {
        return STATUS_NULL_POINTER;
    }
    *found = 0;
    if (count < 2) {
        return STATUS_OK;
    }
    if (count > SIZE_MAX / sizeof(Talk)) {
        return STATUS_MEMORY_ERROR;
    }
    sorted = (Talk *)malloc(count * sizeof(Talk));
    if (sorted == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    memcpy(sorted, talks, count * sizeof(Talk));
    qsort(sorted, count, sizeof(Talk), compare_by_id);
    for (i = 1; i < count; i++) {
        if (sorted[i].id == sorted[i - 1].id) {
            *found = 1;
            *duplicate = sorted[i].id;
            break;
        }
    }
    free(sorted);
    return STATUS_OK;
}

Status max_talks(const Talk talks[], const size_t count, const long long rooms, size_t *result) {
    const int minutes_per_day = 24 * 60;
    size_t free_rooms[24 * 60];
    Talk *sorted;
    size_t chosen = 0;
    size_t i;
    int duplicate_found;
    long long duplicate;
    int t;
    Status status;
    if (talks == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (rooms <= 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (i = 0; i < count; i++) {
        if (talks[i].id <= 0) {
            return STATUS_INVALID_ARGUMENT;
        }
        if (talks[i].start < 0 || talks[i].finish >= minutes_per_day) {
            return STATUS_INVALID_TIME;
        }
        if (talks[i].start >= talks[i].finish) {
            return STATUS_INVALID_INTERVAL;
        }
    }
    status = find_duplicate_id(talks, count, &duplicate_found, &duplicate);
    if (status != STATUS_OK) {
        return status;
    }
    if (duplicate_found) {
        return STATUS_DUPLICATE_ID;
    }
    if (count == 0) {
        *result = 0;
        return STATUS_OK;
    }
    if (count > SIZE_MAX / sizeof(Talk)) {
        return STATUS_MEMORY_ERROR;
    }
    sorted = (Talk *)malloc(count * sizeof(Talk));
    if (sorted == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    memcpy(sorted, talks, count * sizeof(Talk));
    qsort(sorted, count, sizeof(Talk), compare_by_finish);
    for (t = 0; t < minutes_per_day; t++) {
        free_rooms[t] = 0;
    }
    free_rooms[0] = (unsigned long long)rooms < (unsigned long long)count ? (size_t)rooms : count;
    for (i = 0; i < count; i++) {
        t = sorted[i].start;
        while (t >= 0 && free_rooms[t] == 0) {
            t--;
        }
        if (t >= 0) {
            free_rooms[t]--;
            free_rooms[sorted[i].finish]++;
            chosen++;
        }
    }
    free(sorted);
    *result = chosen;
    return STATUS_OK;
}

Status parse_header(char *line, long long *n, long long *k) {
    char *tokens[2];
    if (line == NULL || n == NULL || k == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (split_tokens(line, tokens, 2) != 2) {
        return STATUS_INVALID_FORMAT;
    }
    if (parse_positive(tokens[0], n) != STATUS_OK || parse_positive(tokens[1], k) != STATUS_OK) {
        return STATUS_INVALID_NUMBER;
    }
    return STATUS_OK;
}

Status parse_talk(char *line, Talk *talk) {
    char *tokens[3];
    Status status;
    if (line == NULL || talk == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (split_tokens(line, tokens, 3) != 3) {
        return STATUS_INVALID_FORMAT;
    }
    if (parse_positive(tokens[0], &talk->id) != STATUS_OK) {
        return STATUS_INVALID_NUMBER;
    }
    status = parse_time(tokens[1], &talk->start);
    if (status == STATUS_OK) {
        status = parse_time(tokens[2], &talk->finish);
    }
    if (status != STATUS_OK) {
        return status;
    }
    if (talk->start >= talk->finish) {
        return STATUS_INVALID_INTERVAL;
    }
    return STATUS_OK;
}

int is_empty_line(const char *line) {
    while (is_blank(*line)) {
        line++;
    }
    return *line == '\0';
}

Status read_next_line(FILE *file, char **buffer, size_t *capacity, unsigned long long *line_number) {
    Status status;
    do {
        status = read_line(file, buffer, capacity);
        if (status == STATUS_OK) {
            (*line_number)++;
        }
    } while (status == STATUS_OK && is_empty_line(*buffer));
    return status;
}

void print_read_error(const Status status, const unsigned long long line_number) {
    switch (status) {
        case STATUS_END_OF_FILE:
            fprintf(stderr, "Ошибка: ввод закончился раньше, чем были прочитаны все заявки\n");
            break;
        case STATUS_MEMORY_ERROR:
            fprintf(stderr, "Ошибка: не удалось выделить память\n");
            break;
        case STATUS_READ_ERROR:
            fprintf(stderr, "Ошибка чтения стандартного потока ввода\n");
            break;
        case STATUS_INVALID_FORMAT:
            fprintf(stderr, "Ошибка в строке %llu: неверное количество значений\n", line_number);
            break;
        case STATUS_INVALID_NUMBER:
            fprintf(stderr, "Ошибка в строке %llu: ожидалось положительное целое число\n", line_number);
            break;
        case STATUS_INVALID_TIME:
            fprintf(stderr, "Ошибка в строке %llu: время должно быть в формате HH:MM (часы 00-23, минуты 00-59)\n",
                    line_number);
            break;
        case STATUS_INVALID_INTERVAL:
            fprintf(stderr, "Ошибка в строке %llu: время начала должно быть строго меньше времени окончания\n",
                    line_number);
            break;
        default:
            fprintf(stderr, "Ошибка в строке %llu: некорректные данные\n", line_number);
            break;
    }
}

int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t capacity = 0;
    unsigned long long line_number = 0;
    long long n;
    long long k;
    Talk *talks = NULL;
    size_t i;
    size_t answer;
    int duplicate_found;
    long long duplicate;
    Status status;

    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Ошибка: программа не принимает аргументов, данные читаются из стандартного ввода\n");
        return 1;
    }
    status = read_next_line(stdin, &line, &capacity, &line_number);
    if (status == STATUS_OK) {
        status = parse_header(line, &n, &k);
    }
    if (status != STATUS_OK) {
        if (status == STATUS_END_OF_FILE) {
            fprintf(stderr, "Ошибка: ввод пуст, ожидалась строка \"n k\"\n");
        } else if (status == STATUS_INVALID_FORMAT || status == STATUS_INVALID_NUMBER) {
            fprintf(stderr, "Ошибка в строке %llu: первая строка должна содержать два положительных целых числа n и k\n",
                    line_number);
        } else {
            print_read_error(status, line_number);
        }
        free(line);
        return 1;
    }
    if ((unsigned long long)n > SIZE_MAX / sizeof(Talk)) {
        fprintf(stderr, "Ошибка: слишком много заявок\n");
        free(line);
        return 1;
    }
    talks = (Talk *)malloc((size_t)n * sizeof(Talk));
    if (talks == NULL) {
        fprintf(stderr, "Ошибка: не удалось выделить память для %lld заявок\n", n);
        free(line);
        return 1;
    }
    for (i = 0; i < (size_t)n; i++) {
        status = read_next_line(stdin, &line, &capacity, &line_number);
        if (status == STATUS_OK) {
            status = parse_talk(line, &talks[i]);
        }
        if (status != STATUS_OK) {
            print_read_error(status, line_number);
            free(line);
            free(talks);
            return 1;
        }
    }
    free(line);

    status = find_duplicate_id(talks, (size_t)n, &duplicate_found, &duplicate);
    if (status == STATUS_OK && duplicate_found) {
        fprintf(stderr, "Ошибка: идентификатор доклада %lld встречается более одного раза\n", duplicate);
        free(talks);
        return 1;
    }
    if (status == STATUS_OK) {
        status = max_talks(talks, (size_t)n, k, &answer);
    }
    free(talks);
    if (status != STATUS_OK) {
        if (status == STATUS_MEMORY_ERROR) {
            fprintf(stderr, "Ошибка: не удалось выделить память\n");
        } else {
            fprintf(stderr, "Ошибка: некорректные данные заявок\n");
        }
        return 1;
    }
    printf("%lu\n", (unsigned long)answer);
    return 0;
}
