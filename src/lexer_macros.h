
#define INIT int err;
#define DO(X) err = (X); if(err)return err;
#define GOT(S) fus_lexer_got(lexer, S)
#define GET(S) DO(fus_lexer_get(lexer, S))
#define OPEN GET("(")
#define CLOSE GET(")")
#define NEXT DO(fus_lexer_next(lexer))
#define PARSE_SILENT DO(fus_lexer_parse_silent(lexer))
#define DONE fus_lexer_done(lexer)
#define GOT_CHR fus_lexer_got_chr(lexer)
#define GOT_NAME fus_lexer_got_name(lexer)
#define GOT_STR fus_lexer_got_str(lexer)
#define GOT_INT fus_lexer_got_int(lexer)
#define GET_NAME(P) DO(fus_lexer_get_name(lexer, (&P)))
#define GET_CHR(P) DO(fus_lexer_get_chr(lexer, (&P)))
#define GET_STR(P) DO(fus_lexer_get_str(lexer, (&P)))
#define GET_NAME_OR_STR(P) DO(fus_lexer_get_name_or_str(lexer, (&P)))
#define GET_INT(P) DO(fus_lexer_get_int_fancy(lexer, (&P)))
#define GET_BOOL(P) DO(fus_lexer_get_bool(lexer, (&P)))
#define GET_YN(P) DO(fus_lexer_get_yn(lexer, (&P)))
#define GET_YESNO(P) DO(fus_lexer_get_yesno(lexer, (&P)))
#define GET_ATTR_INT(ATTR, P, OPT) DO(fus_lexer_get_attr_int(lexer, (ATTR), (&P), (OPT)))
#define GET_ATTR_STR(ATTR, P, OPT) DO(fus_lexer_get_attr_str(lexer, (ATTR), (&P), (OPT)))
#define GET_ATTR_BOOL(ATTR, P, OPT) DO(fus_lexer_get_attr_bool(lexer, (ATTR), (&P), (OPT)))
#define GET_ATTR_YN(ATTR, P, OPT) DO(fus_lexer_get_attr_yn(lexer, (ATTR), (&P), (OPT)))
#define GET_ATTR_YESNO(ATTR, P, OPT) DO(fus_lexer_get_attr_yesno(lexer, (ATTR), (&P), (OPT)))
#define GET_INT_FANCY(P) DO(fus_lexer_get_int_fancy(lexer, (&P)))
#define UNEXPECTED(S) fus_lexer_unexpected(lexer, S)


/* NOTE: the following require #include "geom_lexer_utils.h" */
#define GET_VEC(SPACE, VEC) DO(fus_lexer_get_vec(lexer, SPACE, VEC))
#define GET_TRF(SPACE, TRF) DO(fus_lexer_get_trf(lexer, SPACE, &(TRF)))


#define _GET_CACHED(P, _GET_EXPR, STORE) { \
    char *__str; \
    _GET_EXPR /* Assigns to __str */ \
    const char *__const_str = stringstore_get_donate((STORE), __str); \
    if(!__const_str)return 1; \
    (P) = __const_str; \
}
#define GET_STR_CACHED(P, STORE) \
    _GET_CACHED(P, GET_STR        (__str), STORE)
#define GET_NAME_CACHED(P, STORE) \
    _GET_CACHED(P, GET_NAME       (__str), STORE)
#define GET_NAME_OR_STR_CACHED(P, STORE) \
    _GET_CACHED(P, GET_NAME_OR_STR(__str), STORE)
#define GET_ATTR_STR_CACHED(ATTR, P, OPT, STORE) \
    _GET_CACHED(P, GET_ATTR_STR(ATTR, __str, OPT), STORE)
