// Запуск: .\task1.exe 255 4
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_INVALID_BASE,
    STATUS_BUFFER_TOO_SMALL,
    STATUS_INVALID_NUMBER,
    STATUS_OUT_OF_RANGE
} Status;

unsigned long long bitwise_add(unsigned long long a, unsigned long long b) {
    unsigned long long carry;
    while (b != 0) {
        carry = (a & b) << 1;
        a = a ^ b;
        b = carry;
    }
    return a;
}

size_t increment(const size_t value) {
    return (size_t)bitwise_add(value, 1ULL);
}

size_t decrement(const size_t value) {
    return (size_t)bitwise_add(value, ~0ULL);
}

Status to_power_of_two_base(const long long number, const int r, char *out, const size_t size) {
    const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
    char digits[64];
    unsigned long long magnitude = (unsigned long long)number;
    unsigned long long mask;
    size_t count = 0;
    size_t position = 0;
    size_t needed;
    int is_negative = number < 0;
    if (out == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (r < 1 || r > 5) {
        return STATUS_INVALID_BASE;
    }
    if (is_negative) {
        magnitude = bitwise_add(~magnitude, 1ULL);
    }
    mask = ~(~0ULL << r);
    do {
        digits[count] = alphabet[magnitude & mask];
        count = increment(count);
        magnitude = magnitude >> r;
    } while (magnitude != 0);
    needed = increment(count);
    if (is_negative) {
        needed = increment(needed);
    }
    if (needed > size) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    if (is_negative) {
        out[position] = '-';
        position = increment(position);
    }
    while (count != 0) {
        count = decrement(count);
        out[position] = digits[count];
        position = increment(position);
    }
    out[position] = '\0';
    return STATUS_OK;
}

Status parse_long_long(const char *str, long long *result) {
    const char *p;
    long long value = 0;
    int negative = 0;
    int digit;
    if (str == NULL || result == NULL) {
        return STATUS_NULL_POINTER;
    }
    p = str;
    if (*p == '-' || *p == '+') {
        negative = *p == '-';
        p++;
    }
    if (*p == '\0') {
        return STATUS_INVALID_NUMBER;
    }
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return STATUS_INVALID_NUMBER;
        }
        digit = *p - '0';
        if (negative) {
            if (value < (LLONG_MIN + digit) / 10) {
                return STATUS_OUT_OF_RANGE;
            }
            value = value * 10 - digit;
        } else {
            if (value > (LLONG_MAX - digit) / 10) {
                return STATUS_OUT_OF_RANGE;
            }
            value = value * 10 + digit;
        }
    }
    *result = value;
    return STATUS_OK;
}

void print_conversion(const long long number, const int r) {
    char buffer[70];
    Status status = to_power_of_two_base(number, r, buffer, sizeof(buffer));
    switch (status) {
        case STATUS_OK:
            printf("  r = %d (основание %2d): %s\n", r, 1 << r, buffer);
            break;
        case STATUS_INVALID_BASE:
            printf("  r = %d: ошибка - r должно быть от 1 до 5\n", r);
            break;
        case STATUS_BUFFER_TOO_SMALL:
            printf("  r = %d: ошибка - буфер слишком мал\n", r);
            break;
        default:
            printf("  r = %d: внутренняя ошибка\n", r);
            break;
    }
}

void print_all_bases(const long long number) {
    int r;
    printf("Число %lld:\n", number);
    for (r = 1; r <= 5; r++) {
        print_conversion(number, r);
    }
}

void run_demo(void) {
    const long long numbers[] = {0, 1, 7, 10, 31, 32, 255, -255, 1024, 123456789, LLONG_MAX, LLONG_MIN};
    const size_t count = sizeof(numbers) / sizeof(numbers[0]);
    char small[3];
    size_t i;
    for (i = 0; i < count; i++) {
        print_all_bases(numbers[i]);
    }
    printf("Ошибочные вызовы:\n");
    print_conversion(10, 0);
    print_conversion(10, 6);
    print_conversion(10, -1);
    if (to_power_of_two_base(255, 1, small, sizeof(small)) == STATUS_BUFFER_TOO_SMALL) {
        printf("  255 при r = 1 в буфер из 3 символов: ошибка - буфер слишком мал\n");
    }
    if (to_power_of_two_base(255, 1, NULL, 10) == STATUS_NULL_POINTER) {
        printf("  передан NULL вместо буфера: ошибка - нулевой указатель\n");
    }
}

void print_usage(void) {
    printf("Использование:\n");
    printf("  task1.exe                - демонстрация на наборе чисел\n");
    printf("  task1.exe <число>        - перевод числа во все системы 2^r, r = 1..5\n");
    printf("  task1.exe <число> <r>    - перевод числа в систему с основанием 2^r\n");
}

int main(int argc, char *argv[]) {
    long long number;
    long long r;
    Status status;
    if (argc == 1) {
        run_demo();
        return 0;
    }
    if (argc > 3) {
        printf("Ошибка: слишком много аргументов\n");
        print_usage();
        return 1;
    }
    status = parse_long_long(argv[1], &number);
    if (status == STATUS_OUT_OF_RANGE) {
        printf("Ошибка: число должно быть в диапазоне [%lld, %lld]\n", LLONG_MIN, LLONG_MAX);
        return 1;
    }
    if (status != STATUS_OK) {
        printf("Ошибка: \"%s\" не является целым десятичным числом\n", argv[1]);
        print_usage();
        return 1;
    }
    if (argc == 2) {
        print_all_bases(number);
        return 0;
    }
    status = parse_long_long(argv[2], &r);
    if (status != STATUS_OK || r < 1 || r > 5) {
        printf("Ошибка: r должно быть целым числом от 1 до 5\n");
        return 1;
    }
    printf("Число %lld:\n", number);
    print_conversion(number, (int)r);
    return 0;
}
