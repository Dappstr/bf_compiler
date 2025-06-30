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
        const size_t oldCap = vector->capacity;
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

void compute_jumps(const OpVector *vector) {
    size_t stack[1024];
    int top = -1;

    for (size_t i = 0; i < vector->count; i++) {
        if (vector->data[i].token == '[') {
            stack[++top] = i;
        } else if (vector->data[i].token == ']') {
            if (top < 0) {
                printf("Error: unmatched ']' at position %zu\n", i);
                exit(1);
            }
            const size_t match = stack[top--];
            vector->data[match].jump = i;
            vector->data[i].jump = match;
        }
    }
    if (top >= 0) {
        printf("Error: unmatched '[' at position %zu\n", stack[top]);
        exit(1);
    }
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

    fprintf(output, "adrp x21, _tape@PAGE\n");
    fprintf(output, "add x21, x21, _tape@PAGEOFF\n");

    for (size_t i = 0; i < vector->count; i++) {
        // TODO
        switch (vector->data[i].token) {
            case '>':
                fprintf(output, "add x21, x21, #1\n");
                break;

            case '<':
                fprintf(output, "sub x21, x21, #1\n");
                break;

            case '.':
                fprintf(output, "mov x0, #1\n");        // fd
                fprintf(output, "mov x1, x21\n");       // cell
                fprintf(output, "mov x2, #1\n");        // length
                fprintf(output, "bl _write\n");
                break;

            case ',':
                fprintf(output, "mov x0, #0\n");
                fprintf(output, "mov x1, x21\n");
                fprintf(output, "mov x2, #1\n");
                fprintf(output, "bl _read\n");
                break;

            case '+':
                break;

            case '-':
                break;

            case '[':
                fprintf(output, "loop_start_%zu\n", i);
                fprintf(output, "ldrb w0, [x21]\n");
                fprintf(output, "cmp w0, #0\n");
                fprintf(output, "beq loop_end%zu\n", vector->data[i].jump);
                break;

            case ']':
                fprintf(output, "b loop_start_%zu:\n", vector->data[i].jump);
                fprintf(output, "loop_end_%zu:\n", i);
                break;

            default:
                fprintf(stderr, "Error! An unrecognized token detected pass lexer: %c", vector->data[i].token);
                exit(1);
        }
    }

    fprintf(output, "mov x0, #0\nbl _exit\n");

    fprintf(output, ".section __DATA, __bss\n");
    fprintf(output, ".balign 16\n");
    fprintf(output, ".globl _tape\n");
    fprintf(output, "_tape:\n");
    fprintf(output, ".skip 30000\n");
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
    compute_jumps(&tokens);
    generateCode(&tokens);

    freeTokenVector(&tokens);
    free(contents);

    return EXIT_SUCCESS;
}