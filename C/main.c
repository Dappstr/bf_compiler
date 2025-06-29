

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    char token;
    size_t jump;
} Op;

typedef struct {
    Op *data;
    size_t count;
    size_t capacity;
} OpVector;

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

void initTokenVector(OpVector *vector) {
    vector->data = NULL;
    vector->count = 0;
    vector->capacity = 0;
}

void appendOp(OpVector *vector, const Op op) {
    if (vector->capacity == vector->count) {
        const int oldCap = vector->capacity;
        vector->capacity = GROW_CAPACITY(oldCap);
        vector->data = GROW_ARRAY(Op, vector->data, oldCap, vector->capacity);
        if (vector->data == NULL) {
            printf("Error reallocating memory for token vector\n");
            exit(1);
        }
    }
    vector->data[vector->count++] = op;
}

void freeTokenVector(const OpVector *vector) { free(vector->data); }

OpVector lex(const char *src) {
    OpVector ops = {};
    for (size_t i = 0; i < strlen(src); i++) {
        if (isalpha(src[i]) || isdigit(src[i]) || isspace(src[i])) {
            ++i;
        }
        else if (src[i] == '+' || src[i] == '-' || src[i] == '>' || src[i] == '<' ||
                src[i] == '.' || src[i] == ',' || src[i] == '[' || src[i] == ']') {
            const Op op = { .token = src[i], .jump = 0 };
            appendOp(&ops, op);
        } else {
            printf("Error! Unrecognized character: %c\n", src[i]);
            exit(1);
        }
    }
    return ops;
}

void printTokenVector(const OpVector *vector) {
    for (size_t i = 0; i < vector->count; i++) {
        printf("%zu: %c\n",i, vector->data[i].token);
    }
    putchar('\n');
}

void run_command(const char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed!");
        exit(1);
    } else if (pid == 0) {
        execvp(argv[0], (char* const*) argv);
        perror("execvp failed!");
        exit(1);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed!");
            exit(1);
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Command failed: %s\n", argv[0]);
            exit(1);
        }
    }
}

void generateCode(const OpVector *vector) {
    // TODO
    FILE *output = fopen("output.s", "w");
    if (output == NULL) {
        printf("Error opening output file\n");
        exit(1);
    }
    fprintf(output, ".section __TEXT, __text\n");
    fprintf(output, ".align 2\n");
    fprintf(output, ".globl _start\n");
    fprintf(output, ".extern _exit\n");
    fprintf(output, "_start:\n");
    fprintf(output, "mov x0, #0\nbl _exit\n");
    fclose(output);

    const char *clang_args[] = {
        "clang",
        "-arch", "arm64",
        "-nostartfiles",
        "-Wl,-e,_start",
        "-o", "out",
        "output.s",
        NULL
    };
    run_command(clang_args);

    const char *rm_args[] = { "rm", "output.s", NULL };
    run_command(rm_args);
}

int main(const int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    char *contents = get_file_contents(argv[1]);
    const OpVector tokens = lex(contents);
    // printf("Got tokens:\n");
    printTokenVector(&tokens);

    generateCode(&tokens);

    freeTokenVector(&tokens);
    free(contents);

    return 0;
}