
#define INIT int err;
#define DO(X) err = (X); if(err)return err;
#define GOT(S) fus_lexer_got(lexer, S)
#define GET(S) DO(fus_lexer_get(lexer, S))
#define NEXT DO(fus_lexer_next(lexer))
#define DONE fus_lexer_done(lexer)
#define GOT_NAME fus_lexer_got_name(lexer)
#define GOT_STR fus_lexer_got_str(lexer)
#define GOT_INT fus_lexer_got_int(lexer)
#define GET_NAME(P) DO(fus_lexer_get_name(lexer, (&P)))
#define GET_STR(P) DO(fus_lexer_get_str(lexer, (&P)))
#define GET_INT(P) DO(fus_lexer_get_int(lexer, (&P)))
#define GET_YESNO(P) DO(fus_lexer_get_yesno(lexer, (&P)))
#define GET_BOOL(P) DO(fus_lexer_get_bool(lexer, (&P)))
#define GET_INT_FANCY(P) DO(fus_lexer_get_int_fancy(lexer, (&P)))
#define GET_VEC(SPACE, VEC) DO(fus_lexer_get_vec(lexer, SPACE, VEC))
#define UNEXPECTED(S) fus_lexer_unexpected(lexer, S)
