

#include "lexer.h"


static size_t fus_strnlen(const char *s, size_t maxlen){
    size_t len = 0;
    while(len < maxlen && s[len] != '\0')len++;
    return len;
}

static char *fus_strndup(const char *s1, size_t len){
    size_t s_len = fus_strnlen(s1, len);
    char *s2 = malloc(s_len + 1);
    if(s2 == NULL)return NULL;
    strncpy(s2, s1, len);
    s2[s_len] = '\0';
    return s2;
}



const int INITIAL_INDENTS_SIZE = 32;

static int fus_lexer_get_indent(fus_lexer_t *lexer);

void fus_lexer_cleanup(fus_lexer_t *lexer){
    free(lexer->indents);
}

int fus_lexer_init(fus_lexer_t *lexer, const char *text,
    const char *filename
){
    int err;

    lexer->debug = false;

    int indents_size = INITIAL_INDENTS_SIZE;
    int *indents = calloc(indents_size, sizeof(indents));
    if(indents == NULL)return 1;

    lexer->filename = filename;
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

    err = fus_lexer_get_indent(lexer);
    if(err)return err;
    err = fus_lexer_next(lexer);
    if(err)return err;
    return 0;
}

int fus_lexer_copy(fus_lexer_t *lexer, fus_lexer_t *lexer2){
    lexer->debug = lexer2->debug;
    lexer->filename = lexer2->filename;
    lexer->text = lexer2->text;
    lexer->text_len = lexer2->text_len;
    lexer->token = lexer2->token;
    lexer->token_len = lexer2->token_len;
    lexer->token_type = lexer2->token_type;
    lexer->pos = lexer2->pos;
    lexer->row = lexer2->row;
    lexer->col = lexer2->col;

    int indents_size = lexer2->indents_size;
    int *new_indents = malloc(sizeof(*new_indents) * indents_size);
    if(new_indents == NULL)return 1;
    for(int i = 0; i < indents_size; i++){
        new_indents[i] = lexer2->indents[i];}

    lexer->indent = lexer2->indent;
    lexer->indents_size = lexer2->indents_size;
    lexer->n_indents = lexer2->n_indents;
    lexer->indents = new_indents;
    lexer->returning_indents = lexer2->returning_indents;

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

void fus_lexer_info(fus_lexer_t *lexer, FILE *f){
    fprintf(f, "%s: row %i: col %i: ",
        lexer->filename,
        lexer->row + 1,
        lexer->col - lexer->token_len + 1);
}

void fus_lexer_err_info(fus_lexer_t *lexer){
    fprintf(stderr, "Lexer error: ");
    fus_lexer_info(lexer, stderr);
}

int fus_lexer_get_pos(fus_lexer_t *lexer){
    /* "public getter" */
    return lexer->pos;
}

void fus_lexer_set_pos(fus_lexer_t *lexer, int pos){
    /* "public setter" */
    lexer->pos = pos;
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

    while(1){
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
        if(c == '\0' || c == '\n'){
            if(c == '\n')fus_lexer_eat(lexer);

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

            if(c == '\0'){
                /* Reached end of file; report with NULL token */
                lexer->token = NULL;
                lexer->token_len = 0;
                lexer->token_type = FUS_LEXER_TOKEN_DONE;
                break;
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

bool fus_lexer_got(fus_lexer_t *lexer, const char *text){
    if(lexer->token == NULL || text == NULL){
        return lexer->token == text;
    }
    return
        lexer->token_len == strlen(text) &&
        strncmp(lexer->token, text, lexer->token_len) == 0
    ;
}

bool fus_lexer_got_name(fus_lexer_t *lexer){
    return lexer->token_type == FUS_LEXER_TOKEN_SYM;
}

bool fus_lexer_got_str(fus_lexer_t *lexer){
    return
        lexer->token_type == FUS_LEXER_TOKEN_STR ||
        lexer->token_type == FUS_LEXER_TOKEN_BLOCKSTR;
}

bool fus_lexer_got_int(fus_lexer_t *lexer){
    return lexer->token_type == FUS_LEXER_TOKEN_INT;
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
    return fus_lexer_next(lexer);
}

int fus_lexer_get_name(fus_lexer_t *lexer, char **name){
    if(!fus_lexer_got_name(lexer)){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected name, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *name = fus_strndup(lexer->token, lexer->token_len);
    if(*name == NULL)return 1;
    return fus_lexer_next(lexer);
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

        char *ss = fus_strndup(token+2, s_len);
        if(ss == NULL)return 1;

        *s = ss;
    }else{
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected str, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return fus_lexer_next(lexer);
}

int fus_lexer_get_chr(fus_lexer_t *lexer, char *c){
    int err;
    char *s;
    err = fus_lexer_get_str(lexer, &s);
    if(err)return err;
    if(strlen(s) != 1){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected chr, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *c = s[0];
    free(s);
    return 0;
}

int fus_lexer_get_int(fus_lexer_t *lexer, int *i){
    if(!fus_lexer_got_int(lexer)){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected int, but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    *i = atoi(lexer->token);
    return fus_lexer_next(lexer);
}

int fus_lexer_get_bool(fus_lexer_t *lexer, bool *b){
    if(fus_lexer_got(lexer, "y")){
        *b = true;
    }else if(fus_lexer_got(lexer, "n")){
        *b = false;
    }else{
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected bool (y or n), but got: ");
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return fus_lexer_next(lexer);
}

int fus_lexer_get_int_fancy(fus_lexer_t *lexer, int *i_ptr){
    int err;
    if(fus_lexer_got_int(lexer)){
        err = fus_lexer_get_int(lexer, i_ptr);
        if(err)goto err;
    }else{
        int i = 0;
        err = fus_lexer_get(lexer, "eval");
        if(err)goto err;
        err = fus_lexer_get(lexer, "(");
        if(err)goto err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            bool neg = false;
            if(fus_lexer_got(lexer, "+")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                neg = false;
            }else if(fus_lexer_got(lexer, "-")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                neg = true;
            }

            int add;
            err = fus_lexer_get_int(lexer, &add);
            if(err)goto err;

            if(fus_lexer_got(lexer, "*")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                int mul;
                err = fus_lexer_get_int(lexer, &mul);
                if(err)goto err;
                add *= mul;
            }

            i += neg? -add: add;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
        *i_ptr = i;
    }
    return 0;
err:
    fus_lexer_err_info(lexer);
    fprintf(stderr, "(...while parsing fancy int)\n");
    return err;
}

int fus_lexer_get_int_range(fus_lexer_t *lexer, int maxlen,
    int *i_ptr, int *len_ptr
){
    int err;

    int i = 0;
    int len = 1;

    if(fus_lexer_got(lexer, "*")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        len = maxlen;
    }else{
        err = fus_lexer_get_int(lexer, &i);
        if(err)return err;
        if(i < 0 || i > maxlen){
            fus_lexer_err_info(lexer);
            fprintf(stderr,
                "Expected int within 0..%i, got: %.*s\n",
                maxlen, lexer->token_len, lexer->token);
            return 2;}

        if(fus_lexer_got(lexer, ",")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get_int(lexer, &len);
            if(err)return err;
            if(len < 0 || i + len > maxlen){
                fus_lexer_err_info(lexer);
                fprintf(stderr,
                    "Expected int within 0..%i, got: %.*s\n",
                    maxlen-i, lexer->token_len, lexer->token);
                return 2;}
        }
    }

    *i_ptr = i;
    *len_ptr = len;
    return 0;
}

int fus_lexer_get_attr_int(fus_lexer_t *lexer, const char *attr, int *i,
    bool optional
){
    int err;
    if(fus_lexer_got(lexer, attr)){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_int(lexer, i);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }else if(!optional){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected int attribute \"%s\", but got: ", attr);
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return 0;
}

int fus_lexer_get_attr_bool(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional
){
    int err;
    if(fus_lexer_got(lexer, attr)){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_bool(lexer, b);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }else if(!optional){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected bool attribute \"%s\", but got: ", attr);
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return 0;
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

        if(fus_lexer_got(lexer, "(")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            depth++;
        }else if(fus_lexer_got(lexer, ")")){
            depth--;
            if(depth == 0)break;
            err = fus_lexer_next(lexer);
            if(err)return err;
        }else if(fus_lexer_done(lexer)){
            return fus_lexer_unexpected(lexer, NULL);
        }else{
            /* eat atoms silently */
            err = fus_lexer_next(lexer);
            if(err)return err;
        }
    }
    return 0;
}
