
#define INIT int err;
#define DO(X) err = (X); if(err)return err;
#define GOT(S) fus_lexer_got(lexer, S)
#define GET(S) DO(fus_lexer_get(lexer, S))
#define NEXT DO(fus_lexer_next(lexer))
#define DONE fus_lexer_done(lexer)
#define GET_NAME(P) DO(fus_lexer_get_name(lexer, P))
#define GET_STR(P) DO(fus_lexer_get_str(lexer, P))
#define GET_INT(P) DO(fus_lexer_get_int(lexer, P))
#define UNEXPECTED(S) fus_lexer_unexpected(lexer, S)
