

#include "lexer.h"
#include "vars.h"


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



static int fus_lexer_get_indent(fus_lexer_t *lexer);

static void fus_lexer_free_frame_list(fus_lexer_frame_t *frame_list){
    while(frame_list){
        fus_lexer_frame_t *next = frame_list->next;
        free(frame_list);
        frame_list = next;
    }
}

void fus_lexer_cleanup(fus_lexer_t *lexer){
    vars_cleanup(&lexer->_vars);
    fus_lexer_free_frame_list(lexer->frame_list);
    fus_lexer_free_frame_list(lexer->free_frame_list);
}

static int _fus_lexer_init(fus_lexer_t *lexer, const char *text,
    const char *filename, vars_t *vars
){
    int err;

    vars_init(&lexer->_vars);
    lexer->vars = vars;

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

    lexer->returning_indents = 0;

    lexer->frame_list = NULL;
    lexer->free_frame_list = NULL;

    err = fus_lexer_get_indent(lexer);
    if(err)return err;

    err = fus_lexer_next(lexer);
    if(err)return err;
    return 0;
}

int fus_lexer_init(fus_lexer_t *lexer, const char *text,
    const char *filename
){
    return _fus_lexer_init(lexer, text, filename, NULL);
}

int fus_lexer_init_with_vars(fus_lexer_t *lexer, const char *text,
    const char *filename, vars_t *vars
){
    return _fus_lexer_init(lexer, text, filename,
        vars? vars: &lexer->_vars);
}

const char *fus_lexer_token_type_msg(fus_lexer_token_type_t token_type){
    static const char *msgs[FUS_LEXER_TOKEN_TYPES] = {
        FUS_LEXER_TOKEN_TYPE_MSGS
    };
    if(token_type < 0 || token_type >= FUS_LEXER_TOKEN_TYPES){
        return "unknown";
    }
    return msgs[token_type];
}

static int fus_lexer_depth(fus_lexer_t *lexer){
    int depth = 0;
    for(
        fus_lexer_frame_t *frame = lexer->frame_list;
        frame; frame = frame->next
    )depth++;
    return depth;
}

void fus_lexer_info(fus_lexer_t *lexer, FILE *f){
    fprintf(f, "[%s: row=%i col=%i pos=%i] ",
        lexer->filename,
        lexer->row + 1,
        lexer->col + 1,
        lexer->pos + 1);
}

void fus_lexer_err_info(fus_lexer_t *lexer){
    fprintf(stderr, "Lexer error: ");
    fus_lexer_info(lexer, stderr);
}

static void fus_lexer_fprint_frames(FILE *file, fus_lexer_t *lexer){
    /* For debugging */
    fprintf(file, "FRAMES:");
    for(
        fus_lexer_frame_t *frame = lexer->frame_list;
        frame; frame = frame->next
    )fprintf(file, " %p[%c](%i)", frame, frame->is_block? 'b': ' ',
        frame->indent);
    putc('\n', file);
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

static void fus_lexer_set_token(fus_lexer_t *lexer, const char *token){
    lexer->token = token;
    lexer->token_len = strlen(token);
}

static int fus_lexer_push_frame(fus_lexer_t *lexer,
    fus_lexer_frame_type_t type, bool is_block
){

    /* Get frame (from free_frame_list if available, else malloc) */
    fus_lexer_frame_t *frame = NULL;
    if(lexer->free_frame_list){
        frame = lexer->free_frame_list;
        lexer->free_frame_list = frame->next;
    }else{
        frame = calloc(1, sizeof(*frame));
        if(!frame)return 1;
    }

    /* Move frame onto frame_list */
    frame->next = lexer->frame_list;
    lexer->frame_list = frame;

    /* Set up frame fields */
    frame->type = type;
    frame->is_block = is_block;
    frame->indent = lexer->indent;

    return 0;
}

static int fus_lexer_pop_frame(fus_lexer_t *lexer,
    fus_lexer_frame_t **frame_ptr
){
    /* Can't pop from empty list */
    if(!lexer->frame_list){
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Tried to pop a frame, but frame stack is empty\n");
        return 2;
    }

    /* Get frame */
    fus_lexer_frame_t *frame = lexer->frame_list;

    /* Move frame from frame_list to free_frame_list */
    lexer->frame_list = frame->next;
    frame->next = lexer->free_frame_list;
    lexer->free_frame_list = frame;

    /* Remember to output a close paren... but only if this
    frame wasn't part of an $IF block! */
    if(frame->type != FUS_LEXER_FRAME_IF){
        lexer->returning_indents--;
    }

    /* Return the frame */
    *frame_ptr = frame;
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

static int fus_lexer_eat_comment(fus_lexer_t *lexer){
    /* Eat a comment, including leading '#' */
    fus_lexer_eat(lexer);
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '\n')break;
        fus_lexer_eat(lexer);
    }
    return 0;
}

static int fus_lexer_eat_whitespace(fus_lexer_t *lexer){
    /* Eat all whitespace (including comments) up to next non-whitespace
    character or newline or end-of-file */
    int err;
    while(lexer->pos < lexer->text_len){
        char c = lexer->text[lexer->pos];
        if(c == '#'){
            err = fus_lexer_eat_comment(lexer);
            if(err)return err;
            break;
        }
        if(c == '\n' || c == '\0' || isgraph(c))break;
        if(c != ' '){
            fus_lexer_err_info(lexer); fprintf(stderr,
                "Indented with whitespace other than ' ' "
                "(#32): #%i\n", (int)c);
            return 2;
        }
        fus_lexer_eat(lexer);
    }
    return 0;
}

static int fus_lexer_get_indent(fus_lexer_t *lexer){
    /* Assumes lexer is at start of line. Eats whitespace until it
    finds non-whitespace, then sets lexer->indent.
    Lines consisting entirely of whitespace are simply consumed.
    On successful return, next char is NUL or non-whitespace. */
    int err;
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
        }else if(c == '#'){
            /* eat comment */
            err = fus_lexer_eat_comment(lexer);
            if(err)return err;
        }else if(isspace(c)){
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

static int fus_lexer_handle_whitespace(fus_lexer_t *lexer){
    int err;

    /* Eat whitespace up to first non-whitespace character or newline
    or end-of-file */
    err = fus_lexer_eat_whitespace(lexer);
    if(err)return err;

    char c = lexer->text[lexer->pos];
    if(c == '\0' || c == '\n'){
        /* Eat newline */
        if(c == '\n')fus_lexer_eat(lexer);

        /* Eat whitespace and set lexer->indent, will be 0 if c
        was '\0'. */
        err = fus_lexer_get_indent(lexer);
        if(err)return err;

        /* Pop all block frames whose indentation is higher than ours */
        while(1){
            /* Each time around the loop, a frame is popped, so
            lexer->frame_list will be the next frame */
            fus_lexer_frame_t *frame = lexer->frame_list;
            if(!frame)break;

            /* Validate */
            if(!frame->is_block){
                fus_lexer_err_info(lexer); fprintf(stderr,
                    "Parentheses must start & end on same line\n");
                return 2;
            }

            /* If frame is below current indent level, stop popping frames */
            if(frame->indent < lexer->indent)break;

            /* Pop frame */
            fus_lexer_frame_t *_frame;
            err = fus_lexer_pop_frame(lexer, &_frame);
            if(err)return err;

            /* Sanity check */
            if(frame != _frame){
                fprintf(stderr, "Failed sanity check: "
                    "frame != _frame (%p != %p)\n", frame, _frame);
                return 2;
            }
        }
    }
    return 0;
}

static int _fus_lexer_next(fus_lexer_t *lexer){
    /* Gets a single token. */
    int err;

    /* Handle whitespace (unless we already know we need to
    return some open/close parens) */
    if(!lexer->returning_indents){
        err = fus_lexer_handle_whitespace(lexer);
        if(err)return err;
    }

    /* Maybe output open/close parens */
    if(lexer->returning_indents > 0){
        fprintf(stderr,
            "Returning open parens this way isn't supported, "
            "because our implementation of $IF currently assumes "
            "that when an \"(\" is emitted, lexer->frame_list is "
            "a corresponding frame.\n");
        return 2;
        /*
        lexer->token_type = FUS_LEXER_TOKEN_OPEN;
        fus_lexer_set_token(lexer, "(");
        lexer->returning_indents--;
        return 0;
        */
    }else if(lexer->returning_indents < 0){
        lexer->token_type = FUS_LEXER_TOKEN_CLOSE;
        fus_lexer_set_token(lexer, ")");
        lexer->returning_indents++;
        return 0;
    }

    /* Decide what type of token we got based on next character */
    char c = lexer->text[lexer->pos];
    if(c == '\0'){
        /* Reached end of file; report with NULL token */
        lexer->token = NULL;
        lexer->token_len = 0;
        lexer->token_type = FUS_LEXER_TOKEN_DONE;
    }else if(c == ':'){
        if(lexer->frame_list && !lexer->frame_list->is_block){
            fus_lexer_err_info(lexer); fprintf(stderr,
                "Can't have a colon inside parentheses!\n");
            return 2;
        }
        fus_lexer_eat(lexer);
        fus_lexer_set_token(lexer, "(");
        lexer->token_type = FUS_LEXER_TOKEN_OPEN;
        err = fus_lexer_push_frame(lexer, FUS_LEXER_FRAME_NORMAL, true);
        if(err)return err;
    }else if(c == '('){
        fus_lexer_start_token(lexer);
        fus_lexer_eat(lexer);
        fus_lexer_end_token(lexer);
        lexer->token_type = FUS_LEXER_TOKEN_OPEN;
        err = fus_lexer_push_frame(lexer, FUS_LEXER_FRAME_NORMAL, false);
        if(err)return err;
    }else if(c == ')'){
        /* Pop frames up to and including the first non-block one, that is,
        the one corresponding to "(".
        For each popped frame, lexer->returning_indents is decremented.
        So given "(x: y: z)", when we hit the ')', we should pop 3 frames
        in total: that for the '(', *and* those for the 2 ':'. */
        while(1){
            fus_lexer_frame_t *frame;
            err = fus_lexer_pop_frame(lexer, &frame);
            if(err)return err;
            if(!frame){
                /* If frame is NULL, we allow loop to wrap around so pop_frame
                raises the error about popping from an empty stack. */
                continue;
            }
            if(frame->is_block){
                /* Block frame, that is, frame defined by ':' and indentation */
                continue;
            }
            /* If we make it here, frame is not a block, that is, it was defined
            by a '('. */
            break;
        }

        /* We may be returning "the same" ')' we just parsed... or it
        might be from an earlier ':'. */
        if(lexer->returning_indents < 0){
            fus_lexer_start_token(lexer);
            fus_lexer_eat(lexer);
            fus_lexer_end_token(lexer);
            lexer->token_type = FUS_LEXER_TOKEN_CLOSE;
            lexer->returning_indents++;
            return 0;
        }else{
            /* This should never happen, since that would mean we didn't pop
            any frames, which should have caused fus_lexer_pop_frame to complain
            about an empty stack */
            fus_lexer_err_info(lexer); fprintf(stderr,
                "Failed sanity check: no matching '('\n");
            return 2;
        }
    }else if(c == '_' || isalpha(c)){
        fus_lexer_parse_sym(lexer);
        lexer->token_type = FUS_LEXER_TOKEN_SYM;
    }else if(isdigit(c) || (
        c == '-' && isdigit(fus_lexer_peek(lexer))
    )){
        fus_lexer_parse_int(lexer);
        lexer->token_type = FUS_LEXER_TOKEN_INT;
    }else if(c == '"'){
        err = fus_lexer_parse_str(lexer);
        if(err)return err;
        lexer->token_type = FUS_LEXER_TOKEN_STR;
    }else if(c == ';' && fus_lexer_peek(lexer) == ';'){
        err = fus_lexer_parse_blockstr(lexer);
        if(err)return err;
        lexer->token_type = FUS_LEXER_TOKEN_BLOCKSTR;
    }else{
        fus_lexer_parse_op(lexer);
        lexer->token_type = FUS_LEXER_TOKEN_OP;
    }

    return 0;
}

int fus_lexer_next(fus_lexer_t *lexer){
    /* Gets a single token. Wrapper around _fus_lexer_next, which does
    the real work; this function implements a fancy "precompiler" syntax
    if lexer->vars != NULL */
    int err;
    while(1){
        err = _fus_lexer_next(lexer);
        if(err)return err;

        if(lexer->vars && fus_lexer_got(lexer, "$")){
            err = _fus_lexer_next(lexer);
            if(err)return err;
            if(fus_lexer_got(lexer, "SET_BOOL")){
                err = _fus_lexer_next(lexer);
                if(err)return err;

                char *name;
                err = fus_lexer_get_name(lexer, &name);
                if(err)return err;

                err = vars_set_bool(lexer->vars, name, true);
                if(err)return err;

                free(name);
            }else if(fus_lexer_got(lexer, "IF")){
                err = _fus_lexer_next(lexer);
                if(err)return err;

                bool not = false;
                if(fus_lexer_got(lexer, "!")){
                    err = _fus_lexer_next(lexer);
                    if(err)return err;
                    not = true;
                }

                char *name;
                err = fus_lexer_get_name(lexer, &name);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;

                bool cond = vars_get_bool(lexer->vars, name) ^ not;

                if(!cond){
                    /* Eat everything up to & including closing ")" */
                    err = fus_lexer_parse_silent(lexer);
                    if(err)return err;
                }else{
                    /* Mark this frame as belonging to an $IF,
                    so we don't emit ")" on leaving it. */
                    lexer->frame_list->type = FUS_LEXER_FRAME_IF;
                }

                free(name);
            }else{
                return fus_lexer_unexpected(lexer, NULL);
            }
            continue;
        }
        break;
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

        /* Maximum length of s is length of token without the surrounding
        '"' characters.
        (The actual length may be shorter if there are '\'-escaped
        characters in the string.) */
        int s_len = token_len - 2;

        char *ss0 = malloc(s_len + 1);
        if(ss0 == NULL)return 1;
        char *ss = ss0;

        for(int i = 1; i < token_len - 1; i++){
            char c = token[i];
            if(c == '\\'){
                i++;
                c = token[i];
                if(c == 'n'){
                    c = '\n';
                }
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

static int _fus_lexer_get_bool(fus_lexer_t *lexer, bool *b,
    const char *t, const char *f
){
    if(fus_lexer_got(lexer, t)){
        *b = true;
    }else if(fus_lexer_got(lexer, f)){
        *b = false;
    }else{
        fus_lexer_err_info(lexer); fprintf(stderr,
            "Expected bool (%s or %s), but got: ", t, f);
        fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
        return 2;
    }
    return fus_lexer_next(lexer);
}

int fus_lexer_get_bool(fus_lexer_t *lexer, bool *b){
    return _fus_lexer_get_bool(lexer, b, "t", "f");
}

int fus_lexer_get_yn(fus_lexer_t *lexer, bool *b){
    return _fus_lexer_get_bool(lexer, b, "y", "n");
}

int fus_lexer_get_yesno(fus_lexer_t *lexer, bool *b){
    return _fus_lexer_get_bool(lexer, b, "yes", "no");
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
            if(fus_lexer_got(lexer, "rand")){
                int lo, hi;
                err = fus_lexer_next(lexer);
                if(err)goto err;
                err = fus_lexer_get(lexer, "(");
                if(err)goto err;
                err = fus_lexer_get_int(lexer, &lo);
                if(err)goto err;
                err = fus_lexer_get(lexer, ",");
                if(err)goto err;
                err = fus_lexer_get_int(lexer, &hi);
                if(err)goto err;
                err = fus_lexer_get(lexer, ")");
                if(err)goto err;
                if(hi < lo){
                    /* swap */
                    int tmp = lo;
                    lo = hi;
                    hi = tmp;
                }
                int diff = hi - lo;
                add = (diff? rand() % (hi - lo): 0) + lo;
            }else{
                err = fus_lexer_get_int(lexer, &add);
                if(err)goto err;
            }

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

static int _fus_lexer_get_attr_bool(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional, const char *t, const char *f
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

int fus_lexer_get_attr_yn(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional
){
    return _fus_lexer_get_attr_bool(lexer, attr, b, optional, "y", "n");
}

int fus_lexer_get_attr_bool(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional
){
    return _fus_lexer_get_attr_bool(lexer, attr, b, optional, "t", "f");
}

int fus_lexer_unexpected(fus_lexer_t *lexer, const char *expected){
    fus_lexer_err_info(lexer);
    if(expected == NULL)fprintf(stderr, "Unexpected: ");
    else fprintf(stderr, "Expected %s, but got: ", expected);
    fus_lexer_show(lexer, stderr); fprintf(stderr, "\n");
    return 2;
}

int fus_lexer_parse_silent(fus_lexer_t *lexer){
    /* Expected to be called immediately following a "(" token.
    We eat everything up to and including the corresponding ")" token.
    NOTE: we use _fus_lexer_next so that all macros are skipped */
    int depth = 1;
    while(1){
        int err;

        if(fus_lexer_got(lexer, "(")){
            err = _fus_lexer_next(lexer);
            if(err)return err;
            depth++;
        }else if(fus_lexer_got(lexer, ")")){
            depth--;
            if(depth == 0)break;
            err = _fus_lexer_next(lexer);
            if(err)return err;
        }else if(fus_lexer_done(lexer)){
            return fus_lexer_unexpected(lexer, NULL);
        }else{
            /* eat atoms silently */
            err = _fus_lexer_next(lexer);
            if(err)return err;
        }
    }
    return 0;
}
