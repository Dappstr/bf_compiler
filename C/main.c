#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char *data;
    size_t count;
    size_t capacity;
} TokenVector;

#define GROW_CAPACITY(cap) \
    ((cap) < 8 ? 8 : (cap) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    ((type*) realloc((pointer), (newCount) * sizeof(type)))

char *get_file_contents(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    const size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *contents = (char*) malloc(size + 1);
    if (contents == NULL) {
        puts("Error allocating memory for files contents");
        fclose(file);
        exit(1);
    }
    fread(contents, size, 1, file);
    fclose(file);
    contents[size] = '\0';
    return contents;
}

void initTokenVector(TokenVector *vector) {
    vector->data = NULL;
    vector->count = 0;
    vector->capacity = 0;
}

void appendToken(TokenVector *vector, const char token) {
    if (vector->capacity == vector->count) {
        const int oldCap = vector->capacity;
        vector->capacity = GROW_CAPACITY(oldCap);
        vector->data = GROW_ARRAY(char, vector->data, oldCap, vector->capacity);
        if (vector->data == NULL) {
            printf("Error reallocating memory for token vector\n");
            exit(1);
        }
    }
    vector->data[vector->count] = token;
    ++vector->count;
}

void freeTokenVector(const TokenVector *vector) { free(vector->data); }

TokenVector lex(const char *src) {
    TokenVector tokens = {};
    for (size_t i = 0; i < strlen(src); i++) {
        if (isalpha(src[i] || isdigit(src[i])) || isspace(src[i])) {
            while (isalpha(src[i]) || isdigit(src[i])) {
                ++i;
            }
        }
        else if (src[i] == '+' || src[i] == '-' || src[i] == '*' || src[i] == '>' || src[i] == '<' || src[i] == '.' || src[i] == ',' || src[i] == '[' || src[i] == ']') {
            appendToken(&tokens, src[i]);
        } else {
            printf("Error! Unrecognized character: %c\n", src[i]);
            exit(1);
        }
    }
    return tokens;
}

void printTokenVector(const TokenVector *vector) {
    for (size_t i = 0; i < vector->count; i++) {
        printf("%zu: %c\n",i, vector->data[i]);
    }
    putchar('\n');
}

int main(const int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    char *contents = get_file_contents(argv[1]);
    TokenVector tokens = lex(contents);
    printf("Got tokens:\n");
    printTokenVector(&tokens);
    freeTokenVector(&tokens);
    free(contents);
    return 0;
}