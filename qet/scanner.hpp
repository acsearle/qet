//
//  scanner.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef scanner_hpp
#define scanner_hpp

#define E \
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
enum TokenType { E };
#undef X

#define X(Z) [TOKEN_##Z] = "TOKEN_" #Z,
constexpr const char* TokenTypeCString[] = { E };
#undef X

#undef E

struct Token {
    TokenType type;
    const char* start;
    int length;
    int line;
};

void initScanner(const char* source);
Token scanToken();

#endif /* scanner_hpp */
