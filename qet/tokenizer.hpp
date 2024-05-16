//
//  tokenizer.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef tokenizer_hpp
#define tokenizer_hpp

#include "gc.hpp"

namespace lox {
    
#define ENUMERATE_X_TOKEN \
    X(LEFT_PAREN)\
    X(RIGHT_PAREN)\
    X(LEFT_BRACE)\
    X(RIGHT_BRACE)\
    X(COMMA)\
    X(DOT)\
    X(MINUS)\
    X(PLUS)\
    X(SEMICOLON)\
    X(SLASH)\
    X(STAR)\
    X(BANG)\
    X(BANG_EQUAL)\
    X(EQUAL)\
    X(EQUAL_EQUAL)\
    X(GREATER)\
    X(GREATER_EQUAL)\
    X(LESS)\
    X(LESS_EQUAL)\
    X(IDENTIFIER)\
    X(STRING)\
    X(NUMBER)\
    X(AND)\
    X(CLASS)\
    X(ELSE)\
    X(FALSE)\
    X(FOR)\
    X(FUN)\
    X(IF)\
    X(NIL)\
    X(OR)\
    X(PRINT)\
    X(RETURN)\
    X(SUPER)\
    X(THIS)\
    X(TRUE)\
    X(VAR)\
    X(WHILE)\
    X(ERROR)\
    X(EOF)\

#define X(Z) TOKEN_##Z,
    enum TokenType { ENUMERATE_X_TOKEN };
#undef X
    
#define X(Z) [TOKEN_##Z] = "TOKEN_" #Z,
    constexpr const char* TokenTypeCString[] = { ENUMERATE_X_TOKEN };
#undef X
    
    struct Token {
        TokenType type;
        const char* start;
        int length;
        int line;
    };
    
    struct Tokenizer 
    : gc::Object {
        static Tokenizer* make(const char* first, const char* last);
        virtual Token next() = 0;
    };
        
} // namespace lox

#endif /* tokenizer_hpp */
