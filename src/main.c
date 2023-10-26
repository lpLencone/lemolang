#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// utils.{h|c}
#define eprint(fmt, ...) fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__)
#define panic() \
    do { \
        eprint("%s:%d in %s: %s\n", __FILE__, __LINE__, __func__, strerror(errno)); \
        exit(1); \
    } while (0)
#define panic_if(cond) do { if (cond) panic(); } while (0)

char *read_to_string(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    panic_if(fp == NULL);
    
    char *contents = NULL;
    char buf[1024];
    size_t bytes = 0;
    size_t total = 0;
    while ((bytes = fread(buf, 1, 1024, fp)) != 0) {
        total += bytes;
        contents = realloc(contents, total);
        memcpy(contents + (total - bytes), buf, bytes);
    }
    return contents;
}

/////// da.h

#ifndef DA_INIT_CAPACITY
#  define DA_INIT_CAPACITY  8
#endif // DA_INIT_CAPACITY

#define TYPESIZE(da) sizeof(*(da)->data)

#define da_var(da, t) \
    struct { \
        size_t count; \
        size_t capacity; \
        t *data; \
    } da

#define da_Type(da, t) \
    typedef da_var(da, t)

#define da_insert_n(da, d, n, at) \
    do { \
        assert((da) != NULL && d != NULL); \
        assert(at <= (da)->count); \
        \
        if ((da)->capacity == 0) { \
            (da)->capacity = DA_INIT_CAPACITY; \
            (da)->data = calloc(DA_INIT_CAPACITY, TYPESIZE(da)); \
        } \
        \
        while ((da)->capacity < (da)->count + n) { \
            (da)->capacity *= 2; \
            (da)->data = realloc((da)->data, (da)->capacity * TYPESIZE(da)); \
            assert((da)->data != NULL); \
        } \
        \
        memmove( \
            (da)->data + (at + n), \
            (da)->data + at, \
            ((da)->count - at) * TYPESIZE(da) \
        ); \
        memcpy((da)->data + at, d, (n * TYPESIZE(da))); \
        \
        (da)->count += n; \
    } while (0)

#define da_insert(da, d, at) da_insert_n(da, d, 1, at)
#define da_append_n(da, d, n) da_insert_n(da, d, n, (da)->count)
#define da_append(da, d) da_append_n(da, d, 1)

#define da_end(da) \
    do { \
        assert(da != NULL && (da)->data != NULL); \
        free((da)->data); \
        (da)->data = NULL; \
        (da)->count = 0; \
        (da)->capacity = 0; \
    } while (0)

/////// lexer.c

typedef struct Token {
    enum {
        TokenType_Num = 0,
        TokenType_Op,
        TokenType_Comment,
    } type;
    union {
        long long num;
        enum {
            TokenOp_Push = 0,
            TokenOp_Add,
            TokenOp_Duplicate,
            TokenOp_Dump,
            TokenOp_RShift,
            TokenOp_LShift,
        } op;
    }; 
} Token;

da_Type(Da_Token, Token);

Da_Token lex(const char *source, const char *delimiters)
{
    Da_Token datoken = {0};

    const char *end = source;
    while (*end != '\0') {
        const char *begin = end + strspn(end, delimiters);
        end = begin + strcspn(begin, delimiters);
        size_t len = end - begin;

        if (strncmp(begin, "//", len) == 0) {
            end += strcspn(end, "\n");
            continue;
        } 
        
        Token *tok = malloc(sizeof(Token));

        if (isdigit(*begin)) {
            tok->type = TokenType_Num;
            const char *tokp = begin;
            while (isdigit(*tokp)) {
                tokp++;
            }
            if (begin + len != tokp) {
                eprint("Invalid token %.*s\n", (int) len, begin);
                exit(1);
            }
            tok->num = strtoll(begin, NULL, 10);
            da_append(&datoken, tok);
            continue;
        }

        tok->type = TokenType_Op;
        if (strncmp(begin, "push", len) == 0) {
            tok->op = TokenOp_Push;
        } else if (strncmp(begin, "duplicate", len) == 0) {
            tok->op = TokenOp_Duplicate;
        } else if (strncmp(begin, "dump", len) == 0) {
            tok->op = TokenOp_Dump;
        } else if (strncmp(begin, "add", len) == 0) {
            tok->op = TokenOp_Add;
        } else if (strncmp(begin, "lshift", len) == 0) {
            tok->op = TokenOp_LShift;
        } else if (strncmp(begin, "rshift", len) == 0) {
            tok->op = TokenOp_RShift;
        } else {
            eprint("Invalid token %.*s\n", (int) len, begin);
            exit(1);
        }
        da_append(&datoken, tok);
    }

    return datoken;
}

///////// main.c

void interpret(Da_Token datoken)
{
    long long stack[100] = {0};
    size_t count = 0;

    for (size_t i = 0; i < datoken.count; i++) {
        Token tok = datoken.data[i];
        
        assert(tok.type == TokenType_Op);
        switch(tok.op) {
            case TokenOp_Push:
                assert(i + 1 < datoken.count);
                stack[count++] = datoken.data[i + 1].num;
                i++;
                break;
            case TokenOp_Duplicate:
                assert(count > 0);
                stack[count] = stack[count - 1];
                count++;
                break;
            case TokenOp_Dump:
                assert(count > 0);
                printf("%lld\n", stack[--count]);
                break;
            case TokenOp_Add:
                assert(count > 1);
                stack[count - 2] = stack[count - 2] + stack[count - 1];
                count--;
                break;
            case TokenOp_LShift:
                assert(count > 0);
                stack[count - 1] = stack[count - 1] << 1;
                break;
            case TokenOp_RShift:
                assert(count > 0);
                stack[count - 1] = stack[count - 1] >> 1;
                break;
            default:
                assert(0 && "What?");
        }
    }
}

void usage(char *argv[])
{
    eprint("Usage: %s <source.lemo>\n", argv[0]);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv);
    }

    char *contents = read_to_string(argv[1]);

    Da_Token datoken = lex(contents, " \n");
    interpret(datoken);
    
    free(contents);
    da_end(&datoken);

    return 0;
}
