

#include "util.h"
#include "lexer.h"



const int INITIAL_INDENTS_SIZE = 32;

static int fus_lexer_get_indent(fus_lexer_t *lexer);

int fus_lexer_init(fus_lexer_t *lexer, const char *text){
    lexer->debug = false;

    int indents_size = INITIAL_INDENTS_SIZE;
    int *indents = calloc(indents_size, sizeof(indents));
    if(indents == NULL)return 1;

    lexer->text = text;
    lexer->text_len = strlen(text);
    lexer->token = NULL;
    lexer->token_len = 0;
    lexer->pos = 0;
    lexer->row = 0;
    lexer->col = 0;
    lexer->indent = 0;
    lexer->indents_size = indents_size;
    lexer->n_indents = 0;
    lexer->indents = indents;
    lexer->returning_indents = 0;

    int err = fus_lexer_get_indent(lexer);
    if(err)return err;
    return 0;
}

static void fus_lexer_dump(FILE *f, fus_lexer_t *lexer){
    printf("lexer: %p\n", lexer);
    if(lexer == NULL)return;
    printf("  text = ...\n");
    printf("  pos = %i\n", lexer->pos);
    printf("  row = %i\n", lexer->row);
    printf("  col = %i\n", lexer->col);
    printf("  indent = %i\n", lexer->indent);
    printf("  indents_size = %i\n", lexer->indents_size);
    printf("  n_indents = %i\n", lexer->n_indents);
    printf("  indents:\n");
    for(int i = 0; i < lexer->n_indents; i++){
        printf("    %i\n", lexer->indents[i]);
    }
    printf("  returning_indents = %i\n", lexer->returning_indents);
}

static void fus_lexer_start_token(fus_lexer_t *lexer){
    lexer->token = lexer->text + lexer->pos;
    lexer->token_len = 0;
}

static void fus_lexer_end_token(fus_lexer_t *lexer){
    int token_startpos = lexer->token - lexer->text;
    lexer->token_len = lexer->pos - token_startpos;
}

static void fus_lexer_set_token(
    fus_lexer_t *lexer,
    const char *token
){
    lexer->token = token;
    lexer->token_len = strlen(token);
}

static int fus_lexer_push_indent(
    fus_lexer_t *lexer,
    int indent
){
    if(lexer->n_indents >= lexer->indents_size){
        int indents_size = lexer->indents_size;
        int new_indents_size = indents_size * 2;
        int *new_indents = realloc(lexer->indents,
            new_indents_size * sizeof(new_indents));
        if(new_indents == NULL)return 1;
        memset(new_indents + indents_size, 0, indents_size);
        lexer->indents = new_indents;
        lexer->indents_size = new_indents_size;
    }

    lexer->n_indents++;
    lexer->indents[lexer->n_indents-1] = indent;
    return 0;
}

static void fus_lexer_err_info(fus_lexer_t *lexer){
    fprintf(stderr, "%s: @(row=%i, col=%i, pos=%i): ",
        __FILE__, lexer->row, lexer->col, lexer->pos);
}

static int fus_lexer_pop_indent(fus_lexer_t *lexer){
    if(lexer->n_indents == 0){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Tried to pop an indent, but indents stack is empty\n");
        return 2;
    }
    lexer->n_indents--;
    return 0;
}


static char fus_lexer_peek(fus_lexer_t *lexer){
    return lexer->text[lexer->pos + 1];
}

static char fus_lexer_eat(fus_lexer_t *lexer){
    char c = lexer->text[lexer->pos];
    lexer->pos++;
    return c;
}

static int fus_lexer_get_indent(fus_lexer_t *lexer){
    int indent = 0;
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == ' '){
            indent++;
            lexer->pos++;
        }else if(c == '\n'){
            /* blank lines don't count towards indentation --
            just reset the indentation and restart on next line */
            indent = 0;
            lexer->pos++;
        }else if(c != '\0' && isspace(c)){
            fus_lexer_err_info(lexer); fprintf(stderr,
                "Indented with whitespace other than ' ' "
                "(#32): #%i\n", (int)c);
            return 2;
        }else{
            break;
        }
    }
    lexer->indent = indent;
    return 0;
}

static void fus_lexer_eat_whitespace(fus_lexer_t *lexer){
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\0' || isgraph(c))break;
        lexer->pos++;
    }
}

static void fus_lexer_eat_comment(fus_lexer_t *lexer){
    /* eat leading '#' */
    lexer->pos++;

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\n')break;
        lexer->pos++;
    }
}

static void fus_lexer_get_sym(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c != '_' && !isalnum(c))break;
        lexer->pos++;
    }
    fus_lexer_end_token(lexer);
}

static void fus_lexer_get_int(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);

    /* eat leading '-' if present */
    if(lexer->text[lexer->pos] == '-')lexer->pos++;

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(!isdigit(c))break;
        lexer->pos++;
    }
    fus_lexer_end_token(lexer);
}

static void fus_lexer_get_op(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '(' || c == ')' || c == ':'
            || !isgraph(c) || isalnum(c))break;
        lexer->pos++;
    }
    fus_lexer_end_token(lexer);
}

static int fus_lexer_get_str(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);

    /* Include leading '"' */
    lexer->pos++;

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\0'){
            goto err_eof;
        }else if(c == '\n'){
            goto err_eol;
        }else if(c == '"'){
            lexer->pos++;
            break;
        }else if(c == '\\'){
            lexer->pos++;
            char c = lexer->text[lexer->pos];
            if(c == '\0'){
                goto err_eof;
            }else if(c == '\n'){
                goto err_eol;
            }
        }
        lexer->pos++;
    }
    fus_lexer_end_token(lexer);
    return 0;
err_eol:
    fus_lexer_err_info(lexer); fprintf(stderr,
        "Reached newline while parsing str\n");
    return 2;
err_eof:
    fus_lexer_err_info(lexer); fprintf(stderr,
        "Reached end of text while parsing str\n");
    return 2;
}

int fus_lexer_next(fus_lexer_t *lexer){
    int err;
    while(1){
        /* return "(" or ")" token based on indents? */
        if(lexer->returning_indents != 0)break;

        char c = lexer->text[lexer->pos];
        if(c == '\0'){
            /* Reached end of file; report with NULL token */
            lexer->token = NULL;
            lexer->token_len = 0;
            break;
        }else if(c == '\n'){
            fus_lexer_eat(lexer);
            err = fus_lexer_get_indent(lexer);
            if(err)return err;
            int new_indent = lexer->indent;
            while(lexer->n_indents > 0){
                int indent = lexer->indents[lexer->n_indents-1];
                if(new_indent <= indent){
                    err = fus_lexer_pop_indent(lexer);
                    if(err)return err;
                    lexer->returning_indents--;
                }else{
                    break;
                }
            }
        }else if(isspace(c)){
            fus_lexer_eat_whitespace(lexer);
        }else if(c == ':'){
            fus_lexer_eat(lexer);
            lexer->returning_indents++;
            fus_lexer_push_indent(lexer, lexer->indent);
            break;
        }else if(c == '(' || c == ')'){
            fus_lexer_start_token(lexer);
            fus_lexer_eat(lexer);
            fus_lexer_end_token(lexer);
            break;
        }else if(c == '_' || isalpha(c)){
            fus_lexer_get_sym(lexer);
            break;
        }else if(isdigit(c) || (
            c == '-' && isdigit(fus_lexer_peek(lexer))
        )){
            fus_lexer_get_int(lexer);
            break;
        }else if(c == '#'){
            fus_lexer_eat_comment(lexer);
        }else if(c == '"'){
            err = fus_lexer_get_str(lexer);
            if(err)return err;
            break;
        }else{
            fus_lexer_get_op(lexer);
            break;
        }
    }

    if(lexer->returning_indents > 0){
        fus_lexer_set_token(lexer, "(");
        lexer->returning_indents--;
    }
    if(lexer->returning_indents < 0){
        fus_lexer_set_token(lexer, ")");
        lexer->returning_indents++;
    }

    return 0;
}

bool fus_lexer_done(fus_lexer_t *lexer){
    return lexer->token == NULL;
}

int fus_lexer_got(fus_lexer_t *lexer, const char *text){
    if(lexer->token == NULL || text == NULL){
        return lexer->token == text;
    }
    return
        lexer->token_len == strlen(text) &&
        strncmp(lexer->token, text, lexer->token_len) == 0
    ;
}

void fus_lexer_show(fus_lexer_t *lexer, FILE *f){
    if(lexer->token == NULL){
        fprintf(f, "NULL");
    }else{
        fprintf(f, "\"%.*s\"", lexer->token_len, lexer->token);
    }
}

int fus_lexer_get_name(fus_lexer_t *lexer, char **name){
    if(lexer->token == NULL ||
        (lexer->token[0] != '_' && !isalpha(lexer->token[0]))
    ){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected name, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *name = strndup(lexer->token, lexer->token_len);
    if(*name == NULL)return 1;
    return 0;
}

int fus_lexer_expect(fus_lexer_t *lexer, const char *text){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    if(!fus_lexer_got(lexer, text)){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected \"%s\", but got: ", text);
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return 0;
}

int fus_lexer_unexpected(fus_lexer_t *lexer){
    fus_lexer_err_info(lexer); fprintf(stderr, "Unexpected: ");
    fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
    return 2;
}

