// Запуск: .\task4_2.exe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "string_lib.h"

typedef enum {
    STATUS_OK,
    STATUS_END_OF_FILE,
    STATUS_NULL_POINTER,
    STATUS_INVALID_INPUT,
    STATUS_NOT_FOUND,
    STATUS_DUPLICATE_ID,
    STATUS_ALREADY_DELIVERED,
    STATUS_TIME_ERROR,
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
    String city;
    String street;
    int house;
    String building;
    int apartment;
    String index;
} Address;

typedef struct {
    Address recipient;
    double weight;
    String id;
    String created;
    String delivery;
    DateTime created_time;
    DateTime delivery_time;
    int is_delivered;
} Mail;

typedef struct {
    Address *office;
    Mail *mails;
    size_t count;
    size_t capacity;
} Post;

typedef struct {
    char *buffer;
    size_t capacity;
} Input;

Status from_string_status(const StringStatus status) {
    if (status == STRING_OK) {
        return STATUS_OK;
    }
    if (status == STRING_MEMORY_ERROR || status == STRING_TOO_LONG) {
        return STATUS_MEMORY_ERROR;
    }
    return STATUS_NULL_POINTER;
}

Status read_line(FILE *file, Input *input) {
    const size_t start_capacity = 64;
    size_t length = 0;
    char *new_buffer;
    int c;
    if (file == NULL || input == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (input->buffer == NULL) {
        input->buffer = (char *)malloc(start_capacity);
        if (input->buffer == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        input->capacity = start_capacity;
    }
    c = fgetc(file);
    if (c == EOF) {
        input->buffer[0] = '\0';
        return ferror(file) ? STATUS_READ_ERROR : STATUS_END_OF_FILE;
    }
    while (c != EOF && c != '\n') {
        if (length + 1 >= input->capacity) {
            if (input->capacity > SIZE_MAX / 2) {
                return STATUS_MEMORY_ERROR;
            }
            new_buffer = (char *)realloc(input->buffer, input->capacity * 2);
            if (new_buffer == NULL) {
                return STATUS_MEMORY_ERROR;
            }
            input->buffer = new_buffer;
            input->capacity *= 2;
        }
        input->buffer[length] = (char)c;
        length++;
        c = fgetc(file);
    }
    if (ferror(file)) {
        return STATUS_READ_ERROR;
    }
    input->buffer[length] = '\0';
    return STATUS_OK;
}

char *trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    return text;
}

int is_digit_char(const char c) {
    return c >= '0' && c <= '9';
}

int is_digit_string(const char *text, const size_t length) {
    size_t i;
    if (strlen(text) != length) {
        return 0;
    }
    for (i = 0; i < length; i++) {
        if (!is_digit_char(text[i])) {
            return 0;
        }
    }
    return 1;
}

Status parse_natural(const char *text, int *result) {
    int value = 0;
    int digit;
    if (text == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (*text == '+') {
        text++;
    }
    if (*text == '\0') {
        return STATUS_INVALID_INPUT;
    }
    for (; *text != '\0'; text++) {
        if (!is_digit_char(*text)) {
            return STATUS_INVALID_INPUT;
        }
        digit = *text - '0';
        if (value > (INT_MAX - digit) / 10) {
            return STATUS_INVALID_INPUT;
        }
        value = value * 10 + digit;
    }
    if (value == 0) {
        return STATUS_INVALID_INPUT;
    }
    *result = value;
    return STATUS_OK;
}

Status parse_weight(const char *text, double *result) {
    const char *p = text;
    char *end;
    double value;
    int digits = 0;
    if (text == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (*p == '+') {
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
        return STATUS_INVALID_INPUT;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!is_digit_char(*p)) {
            return STATUS_INVALID_INPUT;
        }
        while (is_digit_char(*p)) {
            p++;
        }
    }
    if (*p != '\0') {
        return STATUS_INVALID_INPUT;
    }
    value = strtod(text, &end);
    if (*end != '\0' || !isfinite(value) || value < 0.0) {
        return STATUS_INVALID_INPUT;
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

Status parse_datetime(const char *text, DateTime *result) {
    const char pattern[] = "00:00:0000 00:00:00";
    size_t i;
    DateTime value;
    if (text == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (strlen(text) != sizeof(pattern) - 1) {
        return STATUS_INVALID_INPUT;
    }
    for (i = 0; i < sizeof(pattern) - 1; i++) {
        if (pattern[i] == '0' ? !is_digit_char(text[i]) : text[i] != pattern[i]) {
            return STATUS_INVALID_INPUT;
        }
    }
    value.day = two_digits(text);
    value.month = two_digits(text + 3);
    value.year = two_digits(text + 6) * 100 + two_digits(text + 8);
    value.hour = two_digits(text + 11);
    value.minute = two_digits(text + 14);
    value.second = two_digits(text + 17);
    if (value.year < 1 || value.month < 1 || value.month > 12 || value.day < 1 ||
        value.day > days_in_month(value.month, value.year) || value.hour > 23 || value.minute > 59 ||
        value.second > 59) {
        return STATUS_INVALID_INPUT;
    }
    *result = value;
    return STATUS_OK;
}

int compare_datetime(const DateTime *a, const DateTime *b) {
    const int left[6] = {a->year, a->month, a->day, a->hour, a->minute, a->second};
    const int right[6] = {b->year, b->month, b->day, b->hour, b->minute, b->second};
    int i;
    for (i = 0; i < 6; i++) {
        if (left[i] != right[i]) {
            return left[i] < right[i] ? -1 : 1;
        }
    }
    return 0;
}

Status get_current_datetime(DateTime *result) {
    time_t now;
    struct tm *local;
    if (result == NULL) {
        return STATUS_NULL_POINTER;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return STATUS_TIME_ERROR;
    }
    local = localtime(&now);
    if (local == NULL) {
        return STATUS_TIME_ERROR;
    }
    result->day = local->tm_mday;
    result->month = local->tm_mon + 1;
    result->year = local->tm_year + 1900;
    result->hour = local->tm_hour;
    result->minute = local->tm_min;
    result->second = local->tm_sec;
    return STATUS_OK;
}

void address_init(Address *address) {
    address->city.data = NULL;
    address->city.length = 0;
    address->street.data = NULL;
    address->street.length = 0;
    address->building.data = NULL;
    address->building.length = 0;
    address->index.data = NULL;
    address->index.length = 0;
    address->house = 0;
    address->apartment = 0;
}

void address_destroy(Address *address) {
    if (address == NULL) {
        return;
    }
    string_destroy(&address->city);
    string_destroy(&address->street);
    string_destroy(&address->building);
    string_destroy(&address->index);
}

void mail_init(Mail *mail) {
    address_init(&mail->recipient);
    mail->weight = 0.0;
    mail->id.data = NULL;
    mail->id.length = 0;
    mail->created.data = NULL;
    mail->created.length = 0;
    mail->delivery.data = NULL;
    mail->delivery.length = 0;
    mail->is_delivered = 0;
}

void mail_destroy(Mail *mail) {
    if (mail == NULL) {
        return;
    }
    address_destroy(&mail->recipient);
    string_destroy(&mail->id);
    string_destroy(&mail->created);
    string_destroy(&mail->delivery);
}

int compare_mails(const void *first, const void *second) {
    const Mail *a = (const Mail *)first;
    const Mail *b = (const Mail *)second;
    int result = 0;
    string_lexicographic_compare(&a->recipient.index, &b->recipient.index, &result);
    if (result == 0) {
        string_lexicographic_compare(&a->id, &b->id, &result);
    }
    return result;
}

int compare_by_creation(const void *first, const void *second) {
    const Mail *a = *(const Mail *const *)first;
    const Mail *b = *(const Mail *const *)second;
    int result = compare_datetime(&a->created_time, &b->created_time);
    if (result == 0) {
        string_lexicographic_compare(&a->id, &b->id, &result);
    }
    return result;
}

void post_init(Post *post, Address *office) {
    post->office = office;
    post->mails = NULL;
    post->count = 0;
    post->capacity = 0;
}

void post_destroy(Post *post) {
    size_t i;
    if (post == NULL) {
        return;
    }
    for (i = 0; i < post->count; i++) {
        mail_destroy(&post->mails[i]);
    }
    free(post->mails);
    post->mails = NULL;
    post->count = 0;
    post->capacity = 0;
    if (post->office != NULL) {
        address_destroy(post->office);
        free(post->office);
        post->office = NULL;
    }
}

Status post_find(const Post *post, const char *id, size_t *index) {
    size_t i;
    if (post == NULL || id == NULL || index == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (i = 0; i < post->count; i++) {
        if (strcmp(post->mails[i].id.data, id) == 0) {
            *index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status post_add(Post *post, Mail *mail) {
    Mail *new_mails;
    size_t new_capacity;
    size_t index;
    if (post == NULL || mail == NULL || mail->id.data == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (post_find(post, mail->id.data, &index) == STATUS_OK) {
        return STATUS_DUPLICATE_ID;
    }
    if (post->count == post->capacity) {
        if (post->capacity > SIZE_MAX / 2) {
            return STATUS_MEMORY_ERROR;
        }
        new_capacity = post->capacity == 0 ? 4 : post->capacity * 2;
        if (new_capacity > SIZE_MAX / sizeof(Mail)) {
            return STATUS_MEMORY_ERROR;
        }
        new_mails = (Mail *)realloc(post->mails, new_capacity * sizeof(Mail));
        if (new_mails == NULL) {
            return STATUS_MEMORY_ERROR;
        }
        post->mails = new_mails;
        post->capacity = new_capacity;
    }
    post->mails[post->count] = *mail;
    post->count++;
    mail_init(mail);
    qsort(post->mails, post->count, sizeof(Mail), compare_mails);
    return STATUS_OK;
}

Status post_remove(Post *post, const char *id) {
    size_t index;
    Status status;
    if (post == NULL || id == NULL) {
        return STATUS_NULL_POINTER;
    }
    status = post_find(post, id, &index);
    if (status != STATUS_OK) {
        return status;
    }
    mail_destroy(&post->mails[index]);
    memmove(&post->mails[index], &post->mails[index + 1], (post->count - index - 1) * sizeof(Mail));
    post->count--;
    return STATUS_OK;
}

Status post_mark_delivered(Post *post, const char *id) {
    size_t index;
    Status status;
    if (post == NULL || id == NULL) {
        return STATUS_NULL_POINTER;
    }
    status = post_find(post, id, &index);
    if (status != STATUS_OK) {
        return status;
    }
    if (post->mails[index].is_delivered) {
        return STATUS_ALREADY_DELIVERED;
    }
    post->mails[index].is_delivered = 1;
    return STATUS_OK;
}

int is_expired(const Mail *mail, const DateTime *now) {
    return !mail->is_delivered && compare_datetime(&mail->delivery_time, now) < 0;
}

Status post_select(const Post *post, const int delivered, const DateTime *now, const Mail ***result,
                   size_t *count) {
    const Mail **selected;
    size_t i;
    size_t found = 0;
    int match;
    if (post == NULL || now == NULL || result == NULL || count == NULL) {
        return STATUS_NULL_POINTER;
    }
    *result = NULL;
    *count = 0;
    if (post->count == 0) {
        return STATUS_OK;
    }
    if (post->count > SIZE_MAX / sizeof(const Mail *)) {
        return STATUS_MEMORY_ERROR;
    }
    selected = (const Mail **)malloc(post->count * sizeof(const Mail *));
    if (selected == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    for (i = 0; i < post->count; i++) {
        match = delivered ? post->mails[i].is_delivered : is_expired(&post->mails[i], now);
        if (match) {
            selected[found] = &post->mails[i];
            found++;
        }
    }
    qsort((void *)selected, found, sizeof(const Mail *), compare_by_creation);
    *result = selected;
    *count = found;
    return STATUS_OK;
}

void print_datetime(const DateTime *value) {
    printf("%02d:%02d:%04d %02d:%02d:%02d", value->day, value->month, value->year, value->hour, value->minute,
           value->second);
}

void print_address(const Address *address) {
    printf("г. %s, ул. %s, д. %d", address->city.data, address->street.data, address->house);
    if (address->building.length > 0) {
        printf(", корп. %s", address->building.data);
    }
    printf(", кв. %d, индекс %s", address->apartment, address->index.data);
}

void print_mail(const Mail *mail, const DateTime *now) {
    printf("Отправление %s\n", mail->id.data);
    printf("  Получатель: ");
    print_address(&mail->recipient);
    printf("\n  Вес: %.3f кг\n", mail->weight);
    printf("  Время создания: %s\n", mail->created.data);
    printf("  Время вручения: %s\n", mail->delivery.data);
    if (mail->is_delivered) {
        printf("  Статус: вручено\n");
    } else if (now != NULL && is_expired(mail, now)) {
        printf("  Статус: не вручено, срок доставки истёк\n");
    } else {
        printf("  Статус: не вручено\n");
    }
}

void print_mail_list(const Mail *const mails[], const size_t count, const DateTime *now, const char *empty) {
    size_t i;
    if (count == 0) {
        printf("%s\n", empty);
        return;
    }
    for (i = 0; i < count; i++) {
        print_mail(mails[i], now);
    }
}

Status ask(Input *input, const char *prompt, char **text) {
    Status status;
    printf("%s", prompt);
    fflush(stdout);
    status = read_line(stdin, input);
    if (status == STATUS_END_OF_FILE) {
        printf("\n");
    }
    if (status != STATUS_OK) {
        return status;
    }
    *text = trim(input->buffer);
    return STATUS_OK;
}

Status ask_text(Input *input, const char *prompt, const int allow_empty, String *result) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, prompt, &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (allow_empty || *text != '\0') {
            return from_string_status(string_create(result, text));
        }
        printf("Ошибка: значение не может быть пустым\n");
    }
}

Status ask_natural(Input *input, const char *prompt, int *result) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, prompt, &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (parse_natural(text, result) == STATUS_OK) {
            return STATUS_OK;
        }
        printf("Ошибка: нужно натуральное число от 1 до %d\n", INT_MAX);
    }
}

Status ask_digits(Input *input, const char *prompt, const size_t length, String *result) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, prompt, &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (is_digit_string(text, length)) {
            return from_string_status(string_create(result, text));
        }
        printf("Ошибка: нужно ровно %lu цифр\n", (unsigned long)length);
    }
}

Status ask_weight(Input *input, const char *prompt, double *result) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, prompt, &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (parse_weight(text, result) == STATUS_OK) {
            return STATUS_OK;
        }
        printf("Ошибка: вес должен быть неотрицательным вещественным числом, например 1.25\n");
    }
}

Status ask_datetime(Input *input, const char *prompt, String *text_result, DateTime *result) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, prompt, &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (parse_datetime(text, result) == STATUS_OK) {
            return from_string_status(string_create(text_result, text));
        }
        printf("Ошибка: нужен существующий момент времени в формате dd:MM:yyyy hh:mm:ss\n");
    }
}

Status ask_address(Input *input, Address *address) {
    Status status;
    address_init(address);
    status = ask_text(input, "  Город: ", 0, &address->city);
    if (status == STATUS_OK) {
        status = ask_text(input, "  Улица: ", 0, &address->street);
    }
    if (status == STATUS_OK) {
        status = ask_natural(input, "  Номер дома: ", &address->house);
    }
    if (status == STATUS_OK) {
        status = ask_text(input, "  Корпус (можно оставить пустым): ", 1, &address->building);
    }
    if (status == STATUS_OK) {
        status = ask_natural(input, "  Номер квартиры: ", &address->apartment);
    }
    if (status == STATUS_OK) {
        status = ask_digits(input, "  Индекс (6 цифр): ", 6, &address->index);
    }
    if (status != STATUS_OK) {
        address_destroy(address);
    }
    return status;
}

Status ask_mail(Input *input, const Post *post, Mail *mail) {
    size_t index;
    Status status;
    mail_init(mail);
    printf("Адрес получателя:\n");
    status = ask_address(input, &mail->recipient);
    if (status == STATUS_OK) {
        status = ask_weight(input, "Вес посылки (кг): ", &mail->weight);
    }
    while (status == STATUS_OK) {
        status = ask_digits(input, "Почтовый идентификатор (14 цифр): ", 14, &mail->id);
        if (status == STATUS_OK && post_find(post, mail->id.data, &index) == STATUS_OK) {
            printf("Ошибка: отправление с таким идентификатором уже есть\n");
            string_destroy(&mail->id);
            continue;
        }
        break;
    }
    if (status == STATUS_OK) {
        status = ask_datetime(input, "Время создания (dd:MM:yyyy hh:mm:ss): ", &mail->created, &mail->created_time);
    }
    while (status == STATUS_OK) {
        status = ask_datetime(input, "Время вручения (dd:MM:yyyy hh:mm:ss): ", &mail->delivery, &mail->delivery_time);
        if (status == STATUS_OK && compare_datetime(&mail->delivery_time, &mail->created_time) < 0) {
            printf("Ошибка: время вручения не может быть раньше времени создания\n");
            string_destroy(&mail->delivery);
            continue;
        }
        break;
    }
    if (status != STATUS_OK) {
        mail_destroy(mail);
    }
    return status;
}

void print_menu(void) {
    printf("\nМеню:\n");
    printf("  1 - добавить отправление\n");
    printf("  2 - удалить отправление по идентификатору\n");
    printf("  3 - найти отправление по идентификатору\n");
    printf("  4 - показать все отправления (по индексу и идентификатору)\n");
    printf("  5 - отметить отправление как врученное\n");
    printf("  6 - показать врученные отправления\n");
    printf("  7 - показать отправления с истёкшим сроком доставки\n");
    printf("  8 - показать адрес почтового отделения\n");
    printf("  0 - выход\n");
}

Status handle_add(Input *input, Post *post) {
    Mail mail;
    Status status = ask_mail(input, post, &mail);
    if (status != STATUS_OK) {
        return status;
    }
    status = post_add(post, &mail);
    if (status == STATUS_OK) {
        printf("Отправление добавлено\n");
    } else {
        mail_destroy(&mail);
        if (status == STATUS_DUPLICATE_ID) {
            printf("Ошибка: отправление с таким идентификатором уже есть\n");
            return STATUS_OK;
        }
    }
    return status;
}

Status ask_id(Input *input, char **id) {
    char *text;
    Status status;
    while (1) {
        status = ask(input, "Почтовый идентификатор (14 цифр): ", &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (is_digit_string(text, 14)) {
            *id = text;
            return STATUS_OK;
        }
        printf("Ошибка: нужно ровно 14 цифр\n");
    }
}

Status handle_by_id(Input *input, Post *post, const int action) {
    char *id;
    size_t index;
    DateTime now;
    Status status = ask_id(input, &id);
    if (status != STATUS_OK) {
        return status;
    }
    if (action == 2) {
        status = post_remove(post, id);
        if (status == STATUS_OK) {
            printf("Отправление удалено\n");
        }
    } else if (action == 3) {
        status = post_find(post, id, &index);
        if (status == STATUS_OK) {
            if (get_current_datetime(&now) == STATUS_OK) {
                print_mail(&post->mails[index], &now);
            } else {
                print_mail(&post->mails[index], NULL);
            }
        }
    } else {
        status = post_mark_delivered(post, id);
        if (status == STATUS_OK) {
            printf("Отправление отмечено как врученное\n");
        } else if (status == STATUS_ALREADY_DELIVERED) {
            printf("Отправление уже вручено\n");
            status = STATUS_OK;
        }
    }
    if (status == STATUS_NOT_FOUND) {
        printf("Отправление с идентификатором %s не найдено\n", id);
        status = STATUS_OK;
    }
    return status;
}

Status handle_select(const Post *post, const int delivered) {
    const Mail **selected;
    size_t count;
    DateTime now;
    Status status = get_current_datetime(&now);
    if (status != STATUS_OK) {
        printf("Ошибка: не удалось получить системное время\n");
        return STATUS_OK;
    }
    status = post_select(post, delivered, &now, &selected, &count);
    if (status != STATUS_OK) {
        return status;
    }
    printf("Текущее время: ");
    print_datetime(&now);
    printf("\n");
    print_mail_list(selected, count, &now, delivered ? "Врученных отправлений нет" : "Отправлений с истёкшим сроком нет");
    free((void *)selected);
    return STATUS_OK;
}

void handle_show_all(const Post *post) {
    size_t i;
    DateTime now;
    int has_time = get_current_datetime(&now) == STATUS_OK;
    if (post->count == 0) {
        printf("Отправлений нет\n");
        return;
    }
    for (i = 0; i < post->count; i++) {
        print_mail(&post->mails[i], has_time ? &now : NULL);
    }
}

Status run_menu(Input *input, Post *post) {
    char *text;
    int option;
    Status status;
    while (1) {
        print_menu();
        status = ask(input, "Выберите пункт: ", &text);
        if (status != STATUS_OK) {
            return status;
        }
        if (strlen(text) != 1 || text[0] < '0' || text[0] > '8') {
            printf("Ошибка: введите число от 0 до 8\n");
            continue;
        }
        option = text[0] - '0';
        switch (option) {
            case 0:
                return STATUS_OK;
            case 1:
                status = handle_add(input, post);
                break;
            case 2:
            case 3:
            case 5:
                status = handle_by_id(input, post, option);
                break;
            case 4:
                handle_show_all(post);
                break;
            case 6:
                status = handle_select(post, 1);
                break;
            case 7:
                status = handle_select(post, 0);
                break;
            case 8:
                printf("Почтовое отделение: ");
                print_address(post->office);
                printf("\n");
                break;
        }
        if (status != STATUS_OK) {
            return status;
        }
    }
}

int main(int argc, char *argv[]) {
    Input input = {NULL, 0};
    Post post;
    Address *office;
    Status status;

    (void)argv;
    if (argc != 1) {
        printf("Ошибка: программа не принимает аргументов\n");
        return 1;
    }
    office = (Address *)malloc(sizeof(Address));
    if (office == NULL) {
        printf("Ошибка: не удалось выделить память\n");
        return 1;
    }
    printf("Введите адрес текущего почтового отделения:\n");
    status = ask_address(&input, office);
    if (status != STATUS_OK) {
        free(office);
        free(input.buffer);
        if (status == STATUS_END_OF_FILE) {
            printf("Ввод завершён до ввода адреса отделения\n");
            return 0;
        }
        printf("Ошибка: %s\n", status == STATUS_MEMORY_ERROR ? "не удалось выделить память" : "ошибка ввода");
        return 1;
    }
    post_init(&post, office);
    status = run_menu(&input, &post);
    post_destroy(&post);
    free(input.buffer);
    if (status == STATUS_END_OF_FILE) {
        printf("Ввод завершён\n");
        return 0;
    }
    if (status != STATUS_OK) {
        printf("Ошибка: %s\n", status == STATUS_MEMORY_ERROR ? "не удалось выделить память" : "ошибка ввода");
        return 1;
    }
    return 0;
}
