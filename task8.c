// Запуск: .\task8.exe tests\poly.txt
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <time.h>
typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO, ERR_EOF, ERR_NOT_FOUND, ERR_DOMAIN } Status;
static void report_error(const Status st) {
    switch (st) {
        case ERR_ARG: fprintf(stderr, "Ошибка: неверная инструкция или многочлен. Учитывается регистр букв; инструкция должна заканчиваться точкой с запятой, коэффициенты и неотрицательные степени должны быть целыми.\n"); break;
        case ERR_RANGE: fprintf(stderr, "Ошибка: значение или размер памяти вне допустимого диапазона.\n"); break;
        case ERR_MEM: fprintf(stderr, "Ошибка: не удалось выделить память.\n"); break;
        case ERR_IO: fprintf(stderr, "Ошибка открытия, чтения, записи или закрытия потока.\n"); break;
        case ERR_EOF: fprintf(stderr, "Ошибка: ввод закончился раньше ожидаемого.\n"); break;
        case ERR_NOT_FOUND: fprintf(stderr, "Ошибка: запись не найдена.\n"); break;
        case ERR_DOMAIN: fprintf(stderr, "Ошибка: для Div и Mod делитель должен быть ненулевым, а коэффициенты частного — целыми.\n"); break;
        case OK: break;
    }
}
static Status parse_real(const char *const text, long double *const result) {
    if (!text || !result || !*text) return ERR_ARG;
    const char *p = text;
    if (*p == '+' || *p == '-') ++p;
    bool digit = false, dot = false, nonzero = false;
    for (; *p && *p != 'e' && *p != 'E'; ++p) {
        if (*p == '.' && !dot) { dot = true; continue; }
        if (*p < '0' || *p > '9') return ERR_ARG;
        digit = true; if (*p != '0') nonzero = true;
    }
    if (!digit) return ERR_ARG;
    if (*p) {
        ++p; if (*p == '+' || *p == '-') ++p;
        if (!*p) return ERR_ARG;
        for (; *p; ++p) if (*p < '0' || *p > '9') return ERR_ARG;
    }
    char *end;
    const long double value = strtold(text, &end);
    if (*end || isnan(value) || fabsl(value) == HUGE_VALL || (value == 0 && nonzero)) return ERR_RANGE;
    *result = value; return OK;
}
typedef struct Term { long long coefficient; unsigned power; struct Term *next; } Term;
static Status checked_sum(const long long a, const long long b, const bool subtract, long long *const result) {
    if (!result) return ERR_ARG;
    if (subtract) {
        if ((b > 0 && a < LLONG_MIN+b) || (b < 0 && a > LLONG_MAX+b)) return ERR_RANGE;
        *result = a-b;
    } else {
        if ((b > 0 && a > LLONG_MAX-b) || (b < 0 && a < LLONG_MIN-b)) return ERR_RANGE;
        *result = a+b;
    }
    return OK;
}
static Status checked_product(const long long a, const long long b, long long *const result) {
    if (!result) return ERR_ARG;
    if (a > 0) {
        if ((b > 0 && a > LLONG_MAX/b) || (b < 0 && b < LLONG_MIN/a)) return ERR_RANGE;
    } else if (a < 0) {
        if ((b > 0 && a < LLONG_MIN/b) || (b < 0 && a < LLONG_MAX/b)) return ERR_RANGE;
    }
    *result = a*b; return OK;
}
static void free_poly(Term *poly) {
    while (poly) { Term *next = poly->next; free(poly); poly = next; }
}
static Status add_term(Term **const poly, const long long coefficient, const unsigned power, const bool subtract) {
    if (!poly) return ERR_ARG;
    if (!coefficient) return OK;
    Term **position = poly;
    while (*position && (*position)->power > power) position = &(*position)->next;
    const bool exists = *position && (*position)->power == power;
    long long value;
    Status st = checked_sum(exists ? (*position)->coefficient : 0,coefficient,subtract,&value);
    if (st != OK) return st;
    if (exists) {
        if (!value) { Term *old = *position; *position = old->next; free(old); }
        else (*position)->coefficient = value;
    } else if (value) {
        Term *created = (Term *)malloc(sizeof(*created));
        if (!created) return ERR_MEM;
        *created = (Term){value,power,*position}; *position = created;
    }
    return OK;
}
static Status poly_add(const Term *a, const Term *b, const bool subtract, Term **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Status st = OK;
    for (; a && st == OK; a = a->next) st = add_term(result,a->coefficient,a->power,false);
    for (; b && st == OK; b = b->next) st = add_term(result,b->coefficient,b->power,subtract);
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status poly_mult(const Term *const a, const Term *const b, Term **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Status st = OK;
    for (const Term *x = a; x && st == OK; x = x->next) for (const Term *y = b; y && st == OK; y = y->next) {
        long long coefficient;
        if (x->power > UINT_MAX-y->power) { st = ERR_RANGE; break; }
        st = checked_product(x->coefficient,y->coefficient,&coefficient);
        if (st == OK) st = add_term(result,coefficient,x->power+y->power,false);
    }
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status poly_divmod(const Term *const a, const Term *const b, Term **const quotient, Term **const remainder) {
    if (!quotient || !remainder || quotient == remainder) return ERR_ARG;
    *quotient = NULL; *remainder = NULL;
    if (!b) return ERR_DOMAIN;
    Status st = poly_add(a,NULL,false,remainder);
    while (st == OK && *remainder && (*remainder)->power >= b->power) {
        const long long lead = (*remainder)->coefficient;
        if (lead == LLONG_MIN && b->coefficient == -1) { st = ERR_RANGE; break; }
        if (lead%b->coefficient) { st = ERR_DOMAIN; break; }
        const long long coefficient = lead/b->coefficient;
        const unsigned power = (*remainder)->power-b->power;
        st = add_term(quotient,coefficient,power,false);
        for (const Term *term = b; term && st == OK; term = term->next) {
            long long product;
            if (power > UINT_MAX-term->power) { st = ERR_RANGE; break; }
            st = checked_product(coefficient,term->coefficient,&product);
            if (st == OK) st = add_term(remainder,product,power+term->power,true);
        }
    }
    if (st != OK) { free_poly(*quotient); free_poly(*remainder); *quotient = NULL; *remainder = NULL; }
    return st;
}
static Status poly_diff(const Term *poly, Term **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Status st = OK;
    for (; poly && st == OK; poly = poly->next) if (poly->power) {
        long long coefficient; st = checked_product(poly->coefficient,(long long)poly->power,&coefficient);
        if (st == OK) st = add_term(result,coefficient,poly->power-1,false);
    }
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status poly_eval(const Term *poly, const long double x, long double *const result) {
    if (!result || !isfinite(x)) return ERR_ARG;
    long double value = 0;
    for (; poly; poly = poly->next) {
        value += poly->coefficient*powl(x,poly->power);
        if (!isfinite(value)) return ERR_RANGE;
    }
    *result = value; return OK;
}
static Status poly_power(const Term *const poly, unsigned power, Term **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Term *factor = NULL;
    Status st = add_term(result,1,0,false);
    if (st == OK) st = poly_add(poly,NULL,false,&factor);
    while (power && st == OK) {
        Term *next = NULL;
        if (power & 1u) {
            st = poly_mult(*result,factor,&next);
            if (st == OK) { free_poly(*result); *result = next; }
        }
        power >>= 1;
        if (power && st == OK) {
            st = poly_mult(factor,factor,&next);
            if (st == OK) { free_poly(factor); factor = next; }
        }
    }
    free_poly(factor);
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status poly_compose(const Term *outer, const Term *const inner, Term **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Status st = OK;
    for (; outer && st == OK; outer = outer->next) {
        Term *power = NULL; st = poly_power(inner,outer->power,&power);
        for (const Term *term = power; term && st == OK; term = term->next) {
            long long coefficient; st = checked_product(outer->coefficient,term->coefficient,&coefficient);
            if (st == OK) st = add_term(result,coefficient,term->power,false);
        }
        free_poly(power);
    }
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status parse_poly(const char *text, Term **const result) {
    if (!text || !*text || !result) return ERR_ARG;
    *result = NULL; Status st = OK; bool first = true;
    while (*text && st == OK) {
        bool negative = false;
        if (*text == '+' || *text == '-') { negative = *text == '-'; ++text; }
        else if (!first) { st = ERR_ARG; break; }
        first = false; const bool digits = isdigit((unsigned char)*text) != 0;
        long long coefficient = 0; const long long limit = negative ? LLONG_MIN : -LLONG_MAX;
        while (isdigit((unsigned char)*text)) {
            const int digit = *text++-'0';
            if (coefficient < (limit+digit)/10) { st = ERR_RANGE; break; }
            coefficient = coefficient*10-digit;
        }
        if (st != OK) break;
        coefficient = digits ? (negative ? coefficient : -coefficient) : (negative ? -1 : 1);
        unsigned power = 0;
        if (*text == 'x') {
            ++text; power = 1;
            if (*text == '^') {
                ++text; power = 0;
                if (!isdigit((unsigned char)*text)) { st = ERR_ARG; break; }
                while (isdigit((unsigned char)*text)) {
                    const unsigned digit = (unsigned)(*text++-'0');
                    if (power > (UINT_MAX-digit)/10) { st = ERR_RANGE; break; }
                    power = power*10+digit;
                }
            }
        } else if (!digits) { st = ERR_ARG; break; }
        if (st == OK && *text && *text != '+' && *text != '-') st = ERR_ARG;
        if (st == OK) st = add_term(result,coefficient,power,false);
    }
    if (st != OK) { free_poly(*result); *result = NULL; }
    return st;
}
static Status print_poly(FILE *const out, const Term *poly) {
    if (!out) return ERR_ARG;
    if (!poly) return fputs("0\n",out) == EOF ? ERR_IO : OK;
    bool first = true;
    for (; poly; poly = poly->next) {
        if (poly->coefficient < 0) { if (fputc('-',out) == EOF) return ERR_IO; }
        else if (!first && fputc('+',out) == EOF) return ERR_IO;
        const unsigned long long magnitude = poly->coefficient < 0 ? 0ULL-(unsigned long long)poly->coefficient : (unsigned long long)poly->coefficient;
        if ((!poly->power || magnitude != 1) && fprintf(out,"%llu",magnitude) < 0) return ERR_IO;
        if (poly->power && fputc('x',out) == EOF) return ERR_IO;
        if (poly->power > 1 && fprintf(out,"^%u",poly->power) < 0) return ERR_IO;
        first = false;
    }
    return fputc('\n',out) == EOF ? ERR_IO : OK;
}
static Status execute(char *const command, Term **const accumulator, FILE *const out) {
    if (!command || !accumulator || !out) return ERR_ARG;
    char *open = strchr(command,'('), *close = strrchr(command,')');
    if (!open || !close || close[1] || close < open) return ERR_ARG;
    *open = '\0'; *close = '\0'; char *arguments = open+1, *comma = strchr(arguments,',');
    if (comma) { *comma = '\0'; if (strchr(comma+1,',')) return ERR_ARG; }
    Term *a = NULL, *b = NULL, *result = NULL; Status st = OK;
    if (!strcmp(command,"Eval")) {
        long double x, value;
        if (comma) st = parse_poly(arguments,&a);
        if (st == OK) st = parse_real(comma ? comma+1 : arguments,&x);
        if (st == OK) st = poly_eval(comma ? a : *accumulator,x,&value);
        if (st == OK && fprintf(out,"%.*Lg\n",DECIMAL_DIG,value) < 0) st = ERR_IO;
        free_poly(a); return st;
    }
    if (!strcmp(command,"Diff")) {
        if (comma) return ERR_ARG;
        if (*arguments) st = parse_poly(arguments,&a);
        if (st == OK) st = poly_diff(*arguments ? a : *accumulator,&result);
    } else {
        if (!*arguments || (comma && !comma[1])) return ERR_ARG;
        if (comma) st = parse_poly(arguments,&a);
        if (st == OK) st = parse_poly(comma ? comma+1 : arguments,&b);
        const Term *left = comma ? a : *accumulator;
        if (st == OK) {
            if (!strcmp(command,"Add")) st = poly_add(left,b,false,&result);
            else if (!strcmp(command,"Sub")) st = poly_add(left,b,true,&result);
            else if (!strcmp(command,"Mult")) st = poly_mult(left,b,&result);
            else if (!strcmp(command,"Cmps")) st = poly_compose(left,b,&result);
            else if (!strcmp(command,"Div") || !strcmp(command,"Mod")) {
                Term *quotient = NULL, *remainder = NULL;
                st = poly_divmod(left,b,&quotient,&remainder);
                if (!strcmp(command,"Div")) { result = quotient; free_poly(remainder); }
                else { result = remainder; free_poly(quotient); }
            } else st = ERR_ARG;
        }
    }
    free_poly(a); free_poly(b);
    if (st != OK) { free_poly(result); return st; }
    free_poly(*accumulator); *accumulator = result;
    return print_poly(out,result);
}
static Status process_file(FILE *const in, FILE *const out) {
    if (!in || !out) return ERR_ARG;
    Term *accumulator = NULL; char *command = NULL; size_t used = 0, capacity = 0, instruction = 1;
    bool block = false, line_comment = false; Status st = OK; int c;
    while (st == OK && (c = fgetc(in)) != EOF) {
        if (line_comment) { if (c == '\n') line_comment = false; continue; }
        if (block) {
            if (c == '[') { st = ERR_ARG; break; }
            if (c == ']') block = false;
            continue;
        }
        if (c == '%') { line_comment = true; continue; }
        if (c == '[') { block = true; continue; }
        if (isspace((unsigned char)c)) continue;
        if (c == ';') {
            if (!used) { st = ERR_ARG; break; }
            command[used] = '\0';
            st = execute(command,&accumulator,out); used = 0;
            if (st == OK) ++instruction;
        } else {
            if (c == 0 || c == ']') { st = ERR_ARG; break; }
            if (used+1 >= capacity) {
                if (capacity > SIZE_MAX/2) { st = ERR_RANGE; break; }
                const size_t next_capacity = capacity ? capacity*2 : 128;
                char *next = (char *)realloc(command,next_capacity);
                if (!next) { st = ERR_MEM; break; }
                command = next; capacity = next_capacity;
            }
            command[used++] = (char)c;
        }
    }
    if (ferror(in)) st = ERR_IO;
    if (st == OK && (block || used)) st = ERR_ARG;
    if (st != OK) fprintf(stderr,"Инструкция %zu: не удалось прочитать или выполнить команду.\n",instruction);
    free(command); free_poly(accumulator); return st;
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr,"Использование: task8.exe <файл инструкций>\n"); return 1; }
    FILE *in = fopen(argv[1],"r");
    if (!in) { report_error(ERR_IO); return 1; }
    Status st = process_file(in,stdout);
    if (fclose(in) == EOF) st = ERR_IO;
    const int output_error = ferror(stdout);
    if (fclose(stdout) == EOF || output_error) st = ERR_IO;
    if (st != OK) report_error(st);
    return st != OK;
}
