// Запуск: .\task6.exe tests\stop_a.txt tests\stop_b.txt tests\stop_c.txt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef enum {
    STATUS_OK,
    STATUS_END_OF_FILE,
    STATUS_NULL_POINTER,
    STATUS_INVALID_ARGUMENT,
    STATUS_INVALID_COORDINATES,
    STATUS_INVALID_FORMAT,
    STATUS_INVALID_TIME,
    STATUS_INVALID_MARKER,
    STATUS_DEPARTURE_BEFORE_ARRIVAL,
    STATUS_OVERLAP,
    STATUS_BROKEN_ROUTE,
    STATUS_NO_COORDINATES,
    STATUS_EMPTY,
    STATUS_OPEN_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_READ_ERROR
} Status;

typedef struct {
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;
} DateTime;

typedef struct {
    char *vehicle;
    double x;
    double y;
    DateTime arrival_time;
    DateTime departure_time;
    long long arrival;
    long long departure;
    char marker;
} Record;

typedef struct {
    Record *items;
    size_t count;
    size_t capacity;
} RecordArray;

typedef struct StopNode {
    double x;
    double y;
    DateTime arrival_time;
    DateTime departure_time;
    long long arrival;
    long long departure;
    char marker;
    struct StopNode *next;
} StopNode;

typedef struct RouteNode {
    char *vehicle;
    StopNode *stops;
    size_t stop_count;
    struct RouteNode *next;
} RouteNode;

typedef struct {
    RouteNode *head;
} Storage;

typedef struct {
    const char *vehicle;
    double routes;
    double path;
    double longest_route;
    double shortest_route;
    double longest_stop;
    double shortest_stop;
    double idle;
} VehicleStats;

typedef enum {
    CRITERION_ROUTES,
    CRITERION_PATH,
    CRITERION_ROUTE,
    CRITERION_STOP,
    CRITERION_IDLE
} Criterion;

typedef struct {
    char *buffer;
    size_t capacity;
} LineBuffer;

Status read_line(FILE *file, LineBuffer *line) {
    const size_t start_capacity = 64;
    size_t length = 0;
    char *new_buffer;
    int c;
    if (file == NULL || line == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (line->buffer == NULL) {
        line->buffer = (char *)malloc(start_capacity);
        if (line->buffer == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        line->capacity = start_capacity;
    }
    c = fgetc(file);
    if (c == EOF) {
        line->buffer[0] = '\0';
        return ferror(file) ? STATUS_READ_ERROR : STATUS_END_OF_FILE;
    }
    while (c != EOF && c != '\n') {
        if (length + 1 >= line->capacity) {
            if (line->capacity > SIZE_MAX / 2) {
                return STATUS_MEMORY_ERROR;
            }
            new_buffer = (char *)realloc(line->buffer, line->capacity * 2);
            if (new_buffer == NULL) {
                return STATUS_MEMORY_ERROR;
            }
            line->buffer = new_buffer;
            line->capacity *= 2;
        }
        line->buffer[length] = (char)c;
        length++;
        c = fgetc(file);
    }
    if (ferror(file)) {
        return STATUS_READ_ERROR;
    }
    if (length > 0 && line->buffer[length - 1] == '\r') {
        length--;
    }
    line->buffer[length] = '\0';
    return STATUS_OK;
}

int is_blank(const char c) {
    return c == ' ' || c == '\t';
}

char *trim(char *text) {
    char *end;
    while (is_blank(*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && is_blank(end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
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

int is_digit_char(const char c) {
    return c >= '0' && c <= '9';
}

Status parse_double(const char *text, double *result) {
    const char *p = text;
    char *end;
    double value;
    int digits = 0;
    if (text == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (*p == '+' || *p == '-') {
        p++;
    }
    for (; is_digit_char(*p); p++) {
        digits++;
    }
    if (*p == '.') {
        for (p++; is_digit_char(*p); p++) {
            digits++;
        }
    }
    if (digits == 0) {
        return STATUS_INVALID_COORDINATES;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!is_digit_char(*p)) {
            return STATUS_INVALID_COORDINATES;
        }
        while (is_digit_char(*p)) {
            p++;
        }
    }
    if (*p != '\0') {
        return STATUS_INVALID_COORDINATES;
    }
    value = strtod(text, &end);
    if (*end != '\0' || !isfinite(value)) {
        return STATUS_INVALID_COORDINATES;
    }
    *result = value + 0.0;
    return STATUS_OK;
}

int is_leap_year(const int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(const int month, const int year) {
    const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

int two_digits(const char *text) {
    return (text[0] - '0') * 10 + (text[1] - '0');
}

int matches_pattern(const char *text, const char *pattern) {
    size_t i;
    if (strlen(text) != strlen(pattern)) {
        return 0;
    }
    for (i = 0; pattern[i] != '\0'; i++) {
        if (pattern[i] == '0' ? !is_digit_char(text[i]) : text[i] != pattern[i]) {
            return 0;
        }
    }
    return 1;
}

long long days_from_civil(int year, const int month, const int day) {
    long long era;
    long long yoe;
    long long doy;
    long long doe;
    year -= month <= 2;
    era = year / 400;
    yoe = year - era * 400;
    doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe;
}

Status parse_datetime(const char *date, const char *clock, DateTime *result, long long *seconds) {
    DateTime value;
    if (date == NULL || clock == NULL || result == NULL || seconds == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (!matches_pattern(date, "00.00.0000") || !matches_pattern(clock, "00:00:00")) {
        return STATUS_INVALID_TIME;
    }
    value.day = two_digits(date);
    value.month = two_digits(date + 3);
    value.year = two_digits(date + 6) * 100 + two_digits(date + 8);
    value.hour = two_digits(clock);
    value.minute = two_digits(clock + 3);
    value.second = two_digits(clock + 6);
    if (value.year < 1 || value.month < 1 || value.month > 12 || value.day < 1 ||
        value.day > days_in_month(value.month, value.year) || value.hour > 23 || value.minute > 59 ||
        value.second > 59) {
        return STATUS_INVALID_TIME;
    }
    *result = value;
    *seconds = days_from_civil(value.year, value.month, value.day) * 86400LL + value.hour * 3600LL +
               value.minute * 60LL + value.second;
    return STATUS_OK;
}

char *copy_text(const char *text) {
    const size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

Status parse_record(char *line, const double x, const double y, Record *record) {
    char *tokens[6];
    Status status;
    if (line == NULL || record == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (split_tokens(line, tokens, 6) != 6) {
        return STATUS_INVALID_FORMAT;
    }
    status = parse_datetime(tokens[1], tokens[2], &record->arrival_time, &record->arrival);
    if (status == STATUS_OK) {
        status = parse_datetime(tokens[3], tokens[4], &record->departure_time, &record->departure);
    }
    if (status != STATUS_OK) {
        return status;
    }
    if (strlen(tokens[5]) != 1 || (tokens[5][0] != 'S' && tokens[5][0] != 'M' && tokens[5][0] != 'E')) {
        return STATUS_INVALID_MARKER;
    }
    if (record->departure < record->arrival) {
        return STATUS_DEPARTURE_BEFORE_ARRIVAL;
    }
    record->marker = tokens[5][0];
    record->x = x;
    record->y = y;
    record->vehicle = copy_text(tokens[0]);
    if (record->vehicle == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    return STATUS_OK;
}

Status records_append(RecordArray *records, const Record *record) {
    Record *new_items;
    size_t new_capacity;
    if (records == NULL || record == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (records->count == records->capacity) {
        if (records->capacity > SIZE_MAX / 2) {
            return STATUS_MEMORY_ERROR;
        }
        new_capacity = records->capacity == 0 ? 16 : records->capacity * 2;
        if (new_capacity > SIZE_MAX / sizeof(Record)) {
            return STATUS_MEMORY_ERROR;
        }
        new_items = (Record *)realloc(records->items, new_capacity * sizeof(Record));
        if (new_items == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        records->items = new_items;
        records->capacity = new_capacity;
    }
    records->items[records->count] = *record;
    records->count++;
    return STATUS_OK;
}

void records_destroy(RecordArray *records) {
    size_t i;
    if (records == NULL) {
        return;
    }
    for (i = 0; i < records->count; i++) {
        free(records->items[i].vehicle);
    }
    free(records->items);
    records->items = NULL;
    records->count = 0;
    records->capacity = 0;
}

int is_empty_line(const char *line) {
    while (is_blank(*line)) {
        line++;
    }
    return *line == '\0';
}

Status load_file(const char *path, RecordArray *records, unsigned long long *line_number) {
    LineBuffer line = {NULL, 0};
    char *tokens[2];
    double x = 0.0;
    double y = 0.0;
    int has_coordinates = 0;
    Record record;
    FILE *file;
    Status status;
    if (path == NULL || records == NULL || line_number == NULL) {
        return STATUS_NULL_POINTER;
    }
    *line_number = 0;
    file = fopen(path, "r");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }
    while ((status = read_line(file, &line)) == STATUS_OK) {
        (*line_number)++;
        if (is_empty_line(line.buffer)) {
            continue;
        }
        if (!has_coordinates) {
            if (split_tokens(line.buffer, tokens, 2) != 2 || parse_double(tokens[0], &x) != STATUS_OK ||
                parse_double(tokens[1], &y) != STATUS_OK) {
                status = STATUS_INVALID_COORDINATES;
                break;
            }
            has_coordinates = 1;
            continue;
        }
        status = parse_record(line.buffer, x, y, &record);
        if (status != STATUS_OK) {
            break;
        }
        status = records_append(records, &record);
        if (status != STATUS_OK) {
            free(record.vehicle);
            break;
        }
    }
    free(line.buffer);
    fclose(file);
    if (status == STATUS_END_OF_FILE) {
        status = has_coordinates ? STATUS_OK : STATUS_NO_COORDINATES;
    }
    return status;
}

int compare_records(const void *first, const void *second) {
    const Record *a = (const Record *)first;
    const Record *b = (const Record *)second;
    int result = strcmp(a->vehicle, b->vehicle);
    if (result != 0) {
        return result;
    }
    if (a->arrival != b->arrival) {
        return a->arrival < b->arrival ? -1 : 1;
    }
    return (a->departure > b->departure) - (a->departure < b->departure);
}

void storage_destroy(Storage *storage) {
    RouteNode *route;
    RouteNode *next_route;
    StopNode *stop;
    StopNode *next_stop;
    if (storage == NULL) {
        return;
    }
    route = storage->head;
    while (route != NULL) {
        next_route = route->next;
        stop = route->stops;
        while (stop != NULL) {
            next_stop = stop->next;
            free(stop);
            stop = next_stop;
        }
        free(route->vehicle);
        free(route);
        route = next_route;
    }
    storage->head = NULL;
}

Status append_stop(RouteNode *route, StopNode **tail, const Record *record) {
    StopNode *stop;
    if (route == NULL || tail == NULL || record == NULL) {
        return STATUS_NULL_POINTER;
    }
    stop = (StopNode *)malloc(sizeof(StopNode));
    if (stop == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    stop->x = record->x;
    stop->y = record->y;
    stop->arrival_time = record->arrival_time;
    stop->departure_time = record->departure_time;
    stop->arrival = record->arrival;
    stop->departure = record->departure;
    stop->marker = record->marker;
    stop->next = NULL;
    if (*tail == NULL) {
        route->stops = stop;
    } else {
        (*tail)->next = stop;
    }
    *tail = stop;
    route->stop_count++;
    return STATUS_OK;
}

Status new_route(Storage *storage, RouteNode **route_tail, const char *vehicle, RouteNode **route) {
    RouteNode *node = (RouteNode *)malloc(sizeof(RouteNode));
    if (node == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    node->vehicle = copy_text(vehicle);
    if (node->vehicle == NULL) {
        free(node);
        return STATUS_MEMORY_ERROR;
    }
    node->stops = NULL;
    node->stop_count = 0;
    node->next = NULL;
    if (*route_tail == NULL) {
        storage->head = node;
    } else {
        (*route_tail)->next = node;
    }
    *route_tail = node;
    *route = node;
    return STATUS_OK;
}

Status build_storage(RecordArray *records, Storage *storage, const char **bad_vehicle) {
    RouteNode *route_tail = NULL;
    RouteNode *open_route = NULL;
    StopNode *stop_tail = NULL;
    size_t i;
    Status status = STATUS_OK;
    if (records == NULL || storage == NULL || bad_vehicle == NULL) {
        return STATUS_NULL_POINTER;
    }
    storage->head = NULL;
    *bad_vehicle = NULL;
    qsort(records->items, records->count, sizeof(Record), compare_records);
    for (i = 0; i < records->count && status == STATUS_OK; i++) {
        const Record *record = &records->items[i];
        const int same_vehicle = i > 0 && strcmp(records->items[i - 1].vehicle, record->vehicle) == 0;
        if (!same_vehicle && open_route != NULL) {
            *bad_vehicle = records->items[i - 1].vehicle;
            status = STATUS_BROKEN_ROUTE;
            break;
        }
        if (same_vehicle && record->arrival < records->items[i - 1].departure) {
            *bad_vehicle = record->vehicle;
            status = STATUS_OVERLAP;
            break;
        }
        if ((record->marker == 'S') == (open_route != NULL)) {
            *bad_vehicle = record->vehicle;
            status = STATUS_BROKEN_ROUTE;
            break;
        }
        if (record->marker == 'S') {
            status = new_route(storage, &route_tail, record->vehicle, &open_route);
            stop_tail = NULL;
        }
        if (status == STATUS_OK) {
            status = append_stop(open_route, &stop_tail, record);
        }
        if (record->marker == 'E') {
            open_route = NULL;
        }
    }
    if (status == STATUS_OK && open_route != NULL) {
        *bad_vehicle = records->items[records->count - 1].vehicle;
        status = STATUS_BROKEN_ROUTE;
    }
    if (status == STATUS_OK && storage->head == NULL) {
        status = STATUS_EMPTY;
    }
    if (status != STATUS_OK) {
        storage_destroy(storage);
    }
    return status;
}

Status route_length(const RouteNode *route, double *result) {
    const StopNode *stop;
    double length = 0.0;
    if (route == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (stop = route->stops; stop != NULL && stop->next != NULL; stop = stop->next) {
        double segment = hypot(stop->next->x - stop->x, stop->next->y - stop->y);
        if (!isfinite(segment) || !isfinite(length + segment)) {
            return STATUS_INVALID_COORDINATES;
        }
        length += segment;
    }
    *result = length;
    return STATUS_OK;
}

Status compute_statistics(const Storage *storage, VehicleStats **result, size_t *count) {
    const RouteNode *route;
    const StopNode *stop;
    VehicleStats *stats;
    VehicleStats *current = NULL;
    size_t routes = 0;
    size_t n = 0;
    double length;
    double duration;
    if (storage == NULL || result == NULL || count == NULL) {
        return STATUS_NULL_POINTER;
    }
    *result = NULL;
    *count = 0;
    if (storage->head == NULL) {
        return STATUS_EMPTY;
    }
    for (route = storage->head; route != NULL; route = route->next) {
        if (routes == SIZE_MAX) {
            return STATUS_MEMORY_ERROR;
        }
        routes++;
    }
    if (routes > SIZE_MAX / sizeof(VehicleStats)) {
        return STATUS_MEMORY_ERROR;
    }
    stats = (VehicleStats *)malloc(routes * sizeof(VehicleStats));
    if (stats == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    for (route = storage->head; route != NULL; route = route->next) {
        if (current == NULL || strcmp(current->vehicle, route->vehicle) != 0) {
            current = &stats[n];
            n++;
            current->vehicle = route->vehicle;
            current->routes = 0.0;
            current->path = 0.0;
            current->idle = 0.0;
            current->longest_route = -1.0;
            current->shortest_route = -1.0;
            current->longest_stop = -1.0;
            current->shortest_stop = -1.0;
        }
        if (route_length(route, &length) != STATUS_OK) {
            free(stats);
            return STATUS_INVALID_COORDINATES;
        }
        current->routes += 1.0;
        current->path += length;
        if (!isfinite(current->path)) {
            free(stats);
            return STATUS_INVALID_COORDINATES;
        }
        if (current->longest_route < 0.0 || length > current->longest_route) {
            current->longest_route = length;
        }
        if (current->shortest_route < 0.0 || length < current->shortest_route) {
            current->shortest_route = length;
        }
        for (stop = route->stops; stop != NULL; stop = stop->next) {
            duration = (double)(stop->departure - stop->arrival);
            current->idle += duration;
            if (current->longest_stop < 0.0 || duration > current->longest_stop) {
                current->longest_stop = duration;
            }
            if (current->shortest_stop < 0.0 || duration < current->shortest_stop) {
                current->shortest_stop = duration;
            }
        }
    }
    *result = stats;
    *count = n;
    return STATUS_OK;
}

double criterion_value(const VehicleStats *stats, const Criterion criterion, const int find_max) {
    switch (criterion) {
        case CRITERION_ROUTES:
            return stats->routes;
        case CRITERION_PATH:
            return stats->path;
        case CRITERION_ROUTE:
            return find_max ? stats->longest_route : stats->shortest_route;
        case CRITERION_STOP:
            return find_max ? stats->longest_stop : stats->shortest_stop;
        case CRITERION_IDLE:
            return stats->idle;
    }
    return 0.0;
}

Status select_vehicles(const VehicleStats stats[], const size_t count, const Criterion criterion, const int find_max,
                       const double eps, size_t **indices, size_t *found, double *best) {
    size_t *selected;
    size_t i;
    size_t n = 0;
    double value;
    if (stats == NULL || indices == NULL || found == NULL || best == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (count == 0) {
        return STATUS_EMPTY;
    }
    if (!isfinite(eps) || eps < 0.0) {
        return STATUS_INVALID_ARGUMENT;
    }
    *best = criterion_value(&stats[0], criterion, find_max);
    for (i = 1; i < count; i++) {
        value = criterion_value(&stats[i], criterion, find_max);
        if (find_max ? value > *best + eps : value < *best - eps) {
            *best = value;
        }
    }
    if (count > SIZE_MAX / sizeof(size_t)) {
        return STATUS_MEMORY_ERROR;
    }
    selected = (size_t *)malloc(count * sizeof(size_t));
    if (selected == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    for (i = 0; i < count; i++) {
        if (fabs(criterion_value(&stats[i], criterion, find_max) - *best) <= eps) {
            selected[n] = i;
            n++;
        }
    }
    *indices = selected;
    *found = n;
    return STATUS_OK;
}

void print_duration(const double seconds) {
    const long long total = (long long)seconds;
    printf("%lld ч %02lld мин %02lld с", total / 3600, total / 60 % 60, total % 60);
}

void print_datetime(const DateTime *value) {
    printf("%02d.%02d.%04d %02d:%02d:%02d", value->day, value->month, value->year, value->hour, value->minute,
           value->second);
}

Status print_storage(const Storage *storage) {
    const RouteNode *route;
    const StopNode *stop;
    size_t number = 0;
    if (storage == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (route = storage->head; route != NULL; route = route->next) {
        double length;
        Status status = route_length(route, &length);
        if (status != STATUS_OK) {
            return status;
        }
        number++;
        printf("Маршрут %lu, транспорт %s, остановок %lu, длина %.3f:\n", (unsigned long)number, route->vehicle,
               (unsigned long)route->stop_count, length);
        for (stop = route->stops; stop != NULL; stop = stop->next) {
            printf("  %c (%g, %g) прибытие ", stop->marker, stop->x, stop->y);
            print_datetime(&stop->arrival_time);
            printf(", отправление ");
            print_datetime(&stop->departure_time);
            printf("\n");
        }
    }
    return STATUS_OK;
}

void print_selection(const VehicleStats stats[], const size_t indices[], const size_t found, const Criterion criterion,
                     const double best) {
    size_t i;
    printf("Транспорт:");
    for (i = 0; i < found; i++) {
        printf("%s %s", i == 0 ? "" : ",", stats[indices[i]].vehicle);
    }
    printf("\nЗначение: ");
    if (criterion == CRITERION_ROUTES) {
        printf("%.0f маршрут(ов)\n", best);
    } else if (criterion == CRITERION_PATH || criterion == CRITERION_ROUTE) {
        printf("%.3f\n", best);
    } else {
        print_duration(best);
        printf("\n");
    }
}

void print_menu(void) {
    printf("\nМеню:\n");
    printf("  1 - транспорт, проехавший больше всех маршрутов\n");
    printf("  2 - транспорт, проехавший меньше всех маршрутов\n");
    printf("  3 - транспорт, проехавший самый длинный путь (сумма всех маршрутов)\n");
    printf("  4 - транспорт, проехавший самый короткий путь (сумма всех маршрутов)\n");
    printf("  5 - транспорт с самым длинным маршрутом\n");
    printf("  6 - транспорт с самым коротким маршрутом\n");
    printf("  7 - транспорт с самой долгой остановкой\n");
    printf("  8 - транспорт с самой короткой остановкой\n");
    printf("  9 - транспорт с самым большим временем простоя\n");
    printf("  10 - показать все маршруты\n");
    printf("  0 - выход\n");
}

Status run_menu(const Storage *storage, const VehicleStats stats[], const size_t count) {
    const Criterion criteria[9] = {CRITERION_ROUTES, CRITERION_ROUTES, CRITERION_PATH, CRITERION_PATH, CRITERION_ROUTE,
                                   CRITERION_ROUTE, CRITERION_STOP, CRITERION_STOP, CRITERION_IDLE};
    const double eps = 1e-9;
    LineBuffer input = {NULL, 0};
    size_t *indices;
    size_t found;
    double best;
    char *text;
    int option;
    Status status;
    while (1) {
        print_menu();
        printf("Выберите пункт: ");
        fflush(stdout);
        status = read_line(stdin, &input);
        if (status != STATUS_OK) {
            if (status == STATUS_END_OF_FILE) {
                printf("\n");
            }
            break;
        }
        text = trim(input.buffer);
        if (strcmp(text, "10") == 0) {
            status = print_storage(storage);
            if (status != STATUS_OK) {
                break;
            }
            continue;
        }
        if (strlen(text) != 1 || text[0] < '0' || text[0] > '9') {
            printf("Ошибка: введите число от 0 до 10\n");
            continue;
        }
        option = text[0] - '0';
        if (option == 0) {
            break;
        }
        status = select_vehicles(stats, count, criteria[option - 1], option % 2 == 1 || option == 9, eps, &indices,
                                 &found, &best);
        if (status != STATUS_OK) {
            break;
        }
        print_selection(stats, indices, found, criteria[option - 1], best);
        free(indices);
    }
    free(input.buffer);
    return status == STATUS_END_OF_FILE ? STATUS_OK : status;
}

void print_load_error(const Status status, const char *path, const unsigned long long line) {
    switch (status) {
        case STATUS_OPEN_ERROR:
            printf("Ошибка: не удалось открыть файл %s\n", path);
            break;
        case STATUS_NO_COORDINATES:
            printf("Ошибка: в файле %s нет координат остановки\n", path);
            break;
        case STATUS_INVALID_COORDINATES:
            printf("Ошибка: %s, строка %llu: первая строка должна содержать две координаты\n", path, line);
            break;
        case STATUS_INVALID_FORMAT:
            printf("Ошибка: %s, строка %llu: нужно 6 значений: номер дата_прибытия время_прибытия "
                   "дата_отправления время_отправления маркер\n", path, line);
            break;
        case STATUS_INVALID_TIME:
            printf("Ошибка: %s, строка %llu: время должно быть существующим, в формате dd.MM.yyyy hh:mm:ss\n", path,
                   line);
            break;
        case STATUS_INVALID_MARKER:
            printf("Ошибка: %s, строка %llu: маркер должен быть S (начальная), M (промежуточная) или E (конечная)\n",
                   path, line);
            break;
        case STATUS_DEPARTURE_BEFORE_ARRIVAL:
            printf("Ошибка: %s, строка %llu: время отправления раньше времени прибытия\n", path, line);
            break;
        case STATUS_MEMORY_ERROR:
            printf("Ошибка: не удалось выделить память\n");
            break;
        default:
            printf("Ошибка чтения файла %s\n", path);
            break;
    }
}

int main(int argc, char *argv[]) {
    RecordArray records = {NULL, 0, 0};
    Storage storage = {NULL};
    VehicleStats *stats = NULL;
    size_t count = 0;
    unsigned long long line;
    const char *bad_vehicle;
    int i;
    Status status = STATUS_OK;

    if (argc < 2) {
        printf("Использование: task6.exe <файл остановки 1> [<файл остановки 2> ...]\n");
        return 1;
    }
    for (i = 1; i < argc && status == STATUS_OK; i++) {
        status = load_file(argv[i], &records, &line);
        if (status != STATUS_OK) {
            print_load_error(status, argv[i], line);
        }
    }
    if (status != STATUS_OK) {
        records_destroy(&records);
        return 1;
    }
    status = build_storage(&records, &storage, &bad_vehicle);
    if (status != STATUS_OK) {
        if (status == STATUS_EMPTY) {
            printf("Ошибка: в файлах нет ни одной записи о транспорте\n");
        } else if (status == STATUS_OVERLAP) {
            printf("Ошибка: транспорт %s находится на двух остановках одновременно\n", bad_vehicle);
        } else if (status == STATUS_BROKEN_ROUTE) {
            printf("Ошибка: у транспорта %s нарушена последовательность остановок "
                   "(маршрут должен начинаться с S и заканчиваться E)\n", bad_vehicle);
        } else {
            printf("Ошибка: не удалось выделить память\n");
        }
        records_destroy(&records);
        return 1;
    }
    records_destroy(&records);
    status = compute_statistics(&storage, &stats, &count);
    if (status == STATUS_OK) {
        printf("Загружено транспортных средств: %lu\n", (unsigned long)count);
        status = run_menu(&storage, stats, count);
    }
    free(stats);
    storage_destroy(&storage);
    if (status != STATUS_OK) {
        printf("Ошибка: %s\n", status == STATUS_MEMORY_ERROR ? "не удалось выделить память" : "ошибка ввода");
        return 1;
    }
    return 0;
}
