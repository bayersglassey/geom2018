

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
    lexer->token_type = FUS_LEXER_TOKEN_DONE;
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

void fus_lexer_dump(fus_lexer_t *lexer, FILE *f){
    fprintf(f, "lexer: %p\n", lexer);
    if(lexer == NULL)return;
    fprintf(f, "  text = ...\n");
    fprintf(f, "  pos = %i\n", lexer->pos);
    fprintf(f, "  row = %i\n", lexer->row);
    fprintf(f, "  col = %i\n", lexer->col);
    fprintf(f, "  indent = %i\n", lexer->indent);
    fprintf(f, "  indents_size = %i\n", lexer->indents_size);
    fprintf(f, "  n_indents = %i\n", lexer->n_indents);
    fprintf(f, "  indents:\n");
    for(int i = 0; i < lexer->n_indents; i++){
        fprintf(f, "    %i\n", lexer->indents[i]);
    }
    fprintf(f, "  returning_indents = %i\n", lexer->returning_indents);
}

void fus_lexer_err_info(fus_lexer_t *lexer){
    fprintf(stderr, "Lexer error: @(row=%i, col=%i, pos=%i): ",
        lexer->row + 1,
        lexer->col - lexer->token_len + 1,
        lexer->pos - lexer->token_len);
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
    if(c == '\n'){
        lexer->row++;
        lexer->col = 0;
    }else{
        lexer->col++;
    }
    return c;
}

static int fus_lexer_get_indent(fus_lexer_t *lexer){
    int indent = 0;
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == ' '){
            indent++;
            fus_lexer_eat(lexer);
        }else if(c == '\n'){
            /* blank lines don't count towards indentation --
            just reset the indentation and restart on next line */
            indent = 0;
            fus_lexer_eat(lexer);
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
        fus_lexer_eat(lexer);
    }
}

static void fus_lexer_eat_comment(fus_lexer_t *lexer){
    /* eat leading '#' */
    fus_lexer_eat(lexer);

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\n')break;
        fus_lexer_eat(lexer);
    }
}

static void fus_lexer_parse_sym(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c != '_' && !isalnum(c))break;
        fus_lexer_eat(lexer);
    }
    fus_lexer_end_token(lexer);
}

static void fus_lexer_parse_int(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);

    /* eat leading '-' if present */
    if(lexer->text[lexer->pos] == '-')fus_lexer_eat(lexer);

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(!isdigit(c))break;
        fus_lexer_eat(lexer);
    }
    fus_lexer_end_token(lexer);
}

static void fus_lexer_parse_op(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '(' || c == ')' || c == ':'
            || !isgraph(c) || isalnum(c))break;
        fus_lexer_eat(lexer);
    }
    fus_lexer_end_token(lexer);
}

static int fus_lexer_parse_str(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);

    /* Include leading '"' */
    fus_lexer_eat(lexer);

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\0'){
            goto err_eof;
        }else if(c == '\n'){
            goto err_eol;
        }else if(c == '"'){
            fus_lexer_eat(lexer);
            break;
        }else if(c == '\\'){
            fus_lexer_eat(lexer);
            char c = lexer->text[lexer->pos];
            if(c == '\0'){
                goto err_eof;
            }else if(c == '\n'){
                goto err_eol;
            }
        }
        fus_lexer_eat(lexer);
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

static int fus_lexer_parse_blockstr(fus_lexer_t *lexer){
    fus_lexer_start_token(lexer);

    /* Include leading ";;" */
    fus_lexer_eat(lexer);
    fus_lexer_eat(lexer);

    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\0'){
            break;
        }else if(c == '\n'){
            break;
        }
        fus_lexer_eat(lexer);
    }
    fus_lexer_end_token(lexer);
    return 0;
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
            lexer->token_type = FUS_LEXER_TOKEN_DONE;
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
            lexer->token_type = c == '('? FUS_LEXER_TOKEN_OPEN: FUS_LEXER_TOKEN_CLOSE;
            break;
        }else if(c == '_' || isalpha(c)){
            fus_lexer_parse_sym(lexer);
            lexer->token_type = FUS_LEXER_TOKEN_SYM;
            break;
        }else if(isdigit(c) || (
            c == '-' && isdigit(fus_lexer_peek(lexer))
        )){
            fus_lexer_parse_int(lexer);
            lexer->token_type = FUS_LEXER_TOKEN_INT;
            break;
        }else if(c == '#'){
            fus_lexer_eat_comment(lexer);
        }else if(c == '"'){
            err = fus_lexer_parse_str(lexer);
            if(err)return err;
            lexer->token_type = FUS_LEXER_TOKEN_STR;
            break;
        }else if(c == ';' && fus_lexer_peek(lexer) == ';'){
            err = fus_lexer_parse_blockstr(lexer);
            if(err)return err;
            lexer->token_type = FUS_LEXER_TOKEN_BLOCKSTR;
            break;
        }else{
            fus_lexer_parse_op(lexer);
            lexer->token_type = FUS_LEXER_TOKEN_OP;
            break;
        }
    }

    if(lexer->returning_indents > 0){
        lexer->token_type = FUS_LEXER_TOKEN_OPEN;
        fus_lexer_set_token(lexer, "(");
        lexer->returning_indents--;
    }
    if(lexer->returning_indents < 0){
        lexer->token_type = FUS_LEXER_TOKEN_CLOSE;
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
        fprintf(f, "end of input");
    }else{
        fprintf(f, "\"%.*s\"", lexer->token_len, lexer->token);
    }
}

int fus_lexer_get(fus_lexer_t *lexer, const char *text){
    if(!fus_lexer_got(lexer, text)){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected \"%s\", but got: ", text);
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return 0;
}

int fus_lexer_get_name(fus_lexer_t *lexer, char **name){
    if(lexer->token_type != FUS_LEXER_TOKEN_SYM){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected name, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *name = strndup(lexer->token, lexer->token_len);
    if(*name == NULL)return 1;
    return 0;
}

int fus_lexer_get_str(fus_lexer_t *lexer, char **s){
    if(lexer->token_type == FUS_LEXER_TOKEN_STR){
        const char *token = lexer->token;
        int token_len = lexer->token_len;

        /* Length of s is length of token without the surrounding
        '"' characters */
        int s_len = token_len - 2;

        char *ss0 = malloc(s_len + 1);
        if(ss0 == NULL)return 1;
        char *ss = ss0;

        for(int i = 1; i < token_len - 1; i++){
            char c = token[i];
            if(c == '\\'){
                i++;
                c = token[i];
            }
            *ss = c;
            ss++;
        }

        *ss = '\0';
        *s = ss0;
    }else if(lexer->token_type == FUS_LEXER_TOKEN_BLOCKSTR){
        const char *token = lexer->token;
        int token_len = lexer->token_len;

        /* Length of s is length of token without the leading ";;" */
        int s_len = token_len - 2;

        char *ss = strndup(token+2, s_len+1);
        if(ss == NULL)return 1;

        *s = ss;
    }else{
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected str, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return 0;
}

int fus_lexer_get_int(fus_lexer_t *lexer, int *i){
    if(lexer->token_type != FUS_LEXER_TOKEN_INT){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected int, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *i = atoi(lexer->token);
    return 0;
}

int fus_lexer_expect(fus_lexer_t *lexer, const char *text){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get(lexer, text);
}

int fus_lexer_expect_name(fus_lexer_t *lexer, char **name){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_name(lexer, name);
}

int fus_lexer_expect_str(fus_lexer_t *lexer, char **s){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_str(lexer, s);
}

int fus_lexer_expect_int(fus_lexer_t *lexer, int *i){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_int(lexer, i);
}

int fus_lexer_unexpected(fus_lexer_t *lexer, const char *expected){
    fus_lexer_err_info(lexer);
    if(expected == NULL)fprintf(stderr, "Unexpected: ");
    else fprintf(stderr, "Expected %s, but got: ", expected);
    fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
    return 2;
}

int fus_lexer_parse_silent(fus_lexer_t *lexer){
    int depth = 1;
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, "(")){
            depth++;
        }else if(fus_lexer_got(lexer, ")")){
            depth--;
            if(depth == 0){
                break;
            }
        }else if(fus_lexer_done(lexer)){
            return fus_lexer_unexpected(lexer, NULL);
        }else{
            /* eat atoms silently */
        }
    }
    return 0;
}
