// Запуск: .\task3.exe tests\huff_abra.txt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define BYTE_VALUES 256
#define MAX_NODES (2 * BYTE_VALUES - 1)

typedef enum {
    STATUS_OK,
    STATUS_NULL_POINTER,
    STATUS_INVALID_ARGUMENT,
    STATUS_HEAP_EMPTY,
    STATUS_HEAP_FULL,
    STATUS_OVERFLOW,
    STATUS_MEMORY_ERROR,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR
} Status;

typedef struct {
    unsigned long long weight;
    int byte;
    int left;
    int right;
} Node;

typedef struct {
    int *items;
    int size;
    int capacity;
    const Node *nodes;
} MinHeap;

typedef struct {
    Node nodes[MAX_NODES];
    int node_count;
    int root;
    char codes[BYTE_VALUES][BYTE_VALUES];
    int code_lengths[BYTE_VALUES];
} Huffman;

typedef struct {
    unsigned long long original_bytes;
    unsigned long long encoded_bits;
    unsigned long long encoded_bytes;
    double savings;
} Statistics;

Status count_frequencies(const char *path, unsigned long long frequencies[]) {
    unsigned char buffer[1024];
    size_t read_count;
    size_t i;
    int b;
    FILE *file;
    if (path == NULL || frequencies == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (b = 0; b < BYTE_VALUES; b++) {
        frequencies[b] = 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }
    while ((read_count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (i = 0; i < read_count; i++) {
            frequencies[buffer[i]]++;
        }
    }
    if (ferror(file)) {
        fclose(file);
        return STATUS_READ_ERROR;
    }
    if (fclose(file) != 0) {
        return STATUS_READ_ERROR;
    }
    return STATUS_OK;
}

Status heap_create(MinHeap *heap, const int capacity, const Node *nodes) {
    if (heap == NULL || nodes == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (capacity <= 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    heap->items = (int *)malloc((size_t)capacity * sizeof(int));
    if (heap->items == NULL) {
        return STATUS_MEMORY_ERROR;
    }
    heap->size = 0;
    heap->capacity = capacity;
    heap->nodes = nodes;
    return STATUS_OK;
}

void heap_destroy(MinHeap *heap) {
    if (heap == NULL) {
        return;
    }
    free(heap->items);
    heap->items = NULL;
    heap->size = 0;
    heap->capacity = 0;
}

int heap_less(const MinHeap *heap, const int i, const int j) {
    const Node *a = &heap->nodes[heap->items[i]];
    const Node *b = &heap->nodes[heap->items[j]];
    if (a->weight != b->weight) {
        return a->weight < b->weight;
    }
    return heap->items[i] < heap->items[j];
}

void heap_swap(MinHeap *heap, const int i, const int j) {
    int tmp = heap->items[i];
    heap->items[i] = heap->items[j];
    heap->items[j] = tmp;
}

Status heap_push(MinHeap *heap, const int node) {
    int i;
    int parent;
    if (heap == NULL || heap->items == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (heap->size == heap->capacity) {
        return STATUS_HEAP_FULL;
    }
    i = heap->size;
    heap->items[i] = node;
    heap->size++;
    while (i > 0) {
        parent = (i - 1) / 2;
        if (!heap_less(heap, i, parent)) {
            break;
        }
        heap_swap(heap, i, parent);
        i = parent;
    }
    return STATUS_OK;
}

Status heap_pop(MinHeap *heap, int *node) {
    int i = 0;
    int left;
    int right;
    int smallest;
    if (heap == NULL || heap->items == NULL || node == NULL) {
        return STATUS_NULL_POINTER;
    }
    if (heap->size == 0) {
        return STATUS_HEAP_EMPTY;
    }
    *node = heap->items[0];
    heap->size--;
    heap->items[0] = heap->items[heap->size];
    while (1) {
        left = 2 * i + 1;
        right = 2 * i + 2;
        smallest = i;
        if (left < heap->size && heap_less(heap, left, smallest)) {
            smallest = left;
        }
        if (right < heap->size && heap_less(heap, right, smallest)) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        heap_swap(heap, i, smallest);
        i = smallest;
    }
    return STATUS_OK;
}

void assign_codes(Huffman *tree, const int node, char *path, const int depth) {
    const Node *current = &tree->nodes[node];
    if (current->left == -1) {
        memcpy(tree->codes[current->byte], path, (size_t)depth);
        tree->codes[current->byte][depth] = '\0';
        tree->code_lengths[current->byte] = depth;
        return;
    }
    path[depth] = '0';
    assign_codes(tree, current->left, path, depth + 1);
    path[depth] = '1';
    assign_codes(tree, current->right, path, depth + 1);
}

Status build_huffman(const unsigned long long frequencies[], Huffman *tree) {
    MinHeap heap;
    char path[BYTE_VALUES];
    int first;
    int second;
    int b;
    Status status = STATUS_OK;
    if (frequencies == NULL || tree == NULL) {
        return STATUS_NULL_POINTER;
    }
    tree->node_count = 0;
    tree->root = -1;
    for (b = 0; b < BYTE_VALUES; b++) {
        tree->code_lengths[b] = 0;
        tree->codes[b][0] = '\0';
    }
    status = heap_create(&heap, BYTE_VALUES, tree->nodes);
    if (status != STATUS_OK) {
        return status;
    }
    for (b = 0; b < BYTE_VALUES && status == STATUS_OK; b++) {
        if (frequencies[b] > 0) {
            tree->nodes[tree->node_count].weight = frequencies[b];
            tree->nodes[tree->node_count].byte = b;
            tree->nodes[tree->node_count].left = -1;
            tree->nodes[tree->node_count].right = -1;
            status = heap_push(&heap, tree->node_count);
            tree->node_count++;
        }
    }
    while (status == STATUS_OK && heap.size > 1) {
        status = heap_pop(&heap, &first);
        if (status == STATUS_OK) {
            status = heap_pop(&heap, &second);
        }
        if (status == STATUS_OK && tree->nodes[first].weight > ULLONG_MAX - tree->nodes[second].weight) {
            status = STATUS_OVERFLOW;
        }
        if (status == STATUS_OK) {
            tree->nodes[tree->node_count].weight = tree->nodes[first].weight + tree->nodes[second].weight;
            tree->nodes[tree->node_count].byte = -1;
            tree->nodes[tree->node_count].left = first;
            tree->nodes[tree->node_count].right = second;
            status = heap_push(&heap, tree->node_count);
            tree->node_count++;
        }
    }
    if (status == STATUS_OK && heap.size == 1) {
        status = heap_pop(&heap, &tree->root);
    }
    heap_destroy(&heap);
    if (status != STATUS_OK || tree->root == -1) {
        return status;
    }
    if (tree->nodes[tree->root].left == -1) {
        b = tree->nodes[tree->root].byte;
        tree->codes[b][0] = '0';
        tree->codes[b][1] = '\0';
        tree->code_lengths[b] = 1;
    } else {
        assign_codes(tree, tree->root, path, 0);
    }
    return STATUS_OK;
}

Status compute_statistics(const unsigned long long frequencies[], const Huffman *tree, Statistics *stats) {
    unsigned long long bits = 0;
    unsigned long long total = 0;
    unsigned long long part;
    int b;
    if (frequencies == NULL || tree == NULL || stats == NULL) {
        return STATUS_NULL_POINTER;
    }
    for (b = 0; b < BYTE_VALUES; b++) {
        if (frequencies[b] == 0) {
            continue;
        }
        if (tree->code_lengths[b] <= 0) {
            return STATUS_INVALID_ARGUMENT;
        }
        if (frequencies[b] > ULLONG_MAX / (unsigned long long)tree->code_lengths[b]) {
            return STATUS_OVERFLOW;
        }
        part = frequencies[b] * (unsigned long long)tree->code_lengths[b];
        if (bits > ULLONG_MAX - part || total > ULLONG_MAX - frequencies[b]) {
            return STATUS_OVERFLOW;
        }
        bits += part;
        total += frequencies[b];
    }
    stats->original_bytes = total;
    stats->encoded_bits = bits;
    stats->encoded_bytes = bits / 8 + (bits % 8 != 0);
    stats->savings = total == 0 ? 0.0 : 100.0 * (1.0 - (double)stats->encoded_bytes / (double)total);
    return STATUS_OK;
}

void print_table(const unsigned long long frequencies[], const Huffman *tree) {
    int b;
    printf("Байт   Частота              Код\n");
    for (b = 0; b < BYTE_VALUES; b++) {
        if (frequencies[b] > 0) {
            printf("%-6d %-20llu %s\n", b, frequencies[b], tree->codes[b]);
        }
    }
}

void print_statistics(const Statistics *stats) {
    printf("Размер исходного файла: %llu байт\n", stats->original_bytes);
    printf("Размер закодированных данных: %llu бит (%llu байт)\n", stats->encoded_bits, stats->encoded_bytes);
    printf("Экономия: %.2f%%\n", stats->savings);
}

int main(int argc, char *argv[]) {
    unsigned long long frequencies[BYTE_VALUES];
    Huffman *tree;
    Statistics stats;
    Status status;

    if (argc != 2) {
        printf("Использование: task3.exe <путь к файлу>\n");
        return 1;
    }
    status = count_frequencies(argv[1], frequencies);
    if (status == STATUS_OPEN_ERROR) {
        printf("Ошибка: не удалось открыть файл %s\n", argv[1]);
        return 1;
    }
    if (status != STATUS_OK) {
        printf("Ошибка чтения файла %s\n", argv[1]);
        return 1;
    }
    tree = (Huffman *)malloc(sizeof(Huffman));
    if (tree == NULL) {
        printf("Ошибка: не удалось выделить память\n");
        return 1;
    }
    status = build_huffman(frequencies, tree);
    if (status == STATUS_OK) {
        status = compute_statistics(frequencies, tree, &stats);
    }
    if (status != STATUS_OK) {
        if (status == STATUS_MEMORY_ERROR) {
            printf("Ошибка: не удалось выделить память\n");
        } else if (status == STATUS_OVERFLOW) {
            printf("Ошибка: файл слишком большой для подсчёта размера\n");
        } else {
            printf("Ошибка: не удалось построить дерево Хаффмана\n");
        }
        free(tree);
        return 1;
    }
    printf("Файл: %s\n", argv[1]);
    print_table(frequencies, tree);
    print_statistics(&stats);
    free(tree);
    return 0;
}
