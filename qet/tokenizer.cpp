//
//  tokenizer.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cstdio>
#include <cstring>

#include "common.hpp"
#include "tokenizer.hpp"

namespace lox {
    
    struct ConcreteScanner : Tokenizer {
        const char* first;   // <-- start of source code
        const char* last;    // <-- end of source code

        const char* start;   // <-- start of current token
        const char* current; // <-- position inside current token
        
        
        int line;
        
        explicit ConcreteScanner(const char* first, const char* last);
        
        bool isAtEnd() const;
        char advance();
        char peek() const;
        char peekNext() const;
        bool match(char expected);
        Token makeToken(TokenType type) const;
        Token errorToken(const char* message) const;
        void skipWhitespace();
        TokenType checkKeyword(int start, int length, const char* rest, TokenType type) const;
        TokenType identifierType() const;
        Token identifier();
        Token string();
        Token number();
        virtual Token next();

    };
    
    // Tokenizer tokenizer;
    
    ConcreteScanner::ConcreteScanner(const char* first, const char* last) {
        this->first = first;
        this->last = last;
        start = first;
        current = first;
        line = 1;
    }
    
    static bool isAlpha(char c) {
        return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_';
    }
    
    static bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }
    
    bool ConcreteScanner::isAtEnd() const {
        return *current == '\0';
    }
    
    char ConcreteScanner::advance() {
        return *current++;
    }
    
    char ConcreteScanner::peek() const {
        return *current;
    }
    
    char ConcreteScanner::peekNext() const {
        if (isAtEnd())
            return '\0';
        return *(current + 1);
    }
    
    bool ConcreteScanner::match(char expected) {
        if (isAtEnd()) return false;
        if (*current != expected) return false;
        current++;
        return true;
    }
    
    Token ConcreteScanner::makeToken(TokenType type) const {
        Token token;
        token.type = type;
        token.start = start;
        token.length = (int)(current - start);
        token.line = line;
        return token;
    }
    
    Token ConcreteScanner::errorToken(const char* message) const {
        Token token;
        token.type = TOKEN_ERROR;
        token.start = message;
        token.length = (int)strlen(message);
        token.line = line;
        return token;
    }
    
    void ConcreteScanner::skipWhitespace() {
        for (;;) {
            char c = peek();
            switch (c) {
                case ' ':
                case '\r':
                case '\t':
                    advance();
                    break;
                case '\n':
                    line++;
                    advance();
                    break;
                case '/':
                    if (peekNext() != '/')
                        return;
                    while (peek() != '\n' && !isAtEnd())
                        advance();
                    break;
                default:
                    return;
            }
        }
    }
    
    
    TokenType ConcreteScanner::checkKeyword(int start, int length, const char* rest, TokenType type) const {
        if (current - this->start == start + length &&
            memcmp(this->start + start, rest, length) == 0) {
            return type;
        }
        return TOKEN_IDENTIFIER;
    }
    
    TokenType ConcreteScanner::identifierType() const {
        switch (start[0]) {
            case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
            case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
            case 'e': return checkKeyword(1, 2, "lse", TOKEN_ELSE);
            case 'f':
                if (current - start > 1) {
                    switch (start[1]) {
                        case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                        case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                        case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                    }
                }
                break;
            case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
            case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
            case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
            case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
            case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
            case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
            case 't':
                if (current - start > 1) {
                    switch (start[1]) {
                        case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                        case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                    }
                }
                break;
            case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
            case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
        }
        return TOKEN_IDENTIFIER;
    }
    
    Token ConcreteScanner::identifier() {
        while (isAlpha(peek()) || isDigit(peek())) advance();
        return makeToken(identifierType());
    }
    
    Token ConcreteScanner::string() {
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++;
            advance();
        }
        if (isAtEnd())
            return errorToken("Unterminated string.");
        
        // The closing quote.
        advance();
        return makeToken(TOKEN_STRING);
    }
    
    Token ConcreteScanner::number() {
        while (isDigit(peek())) advance();
        if (peek() == '.' && isDigit(peekNext())) {
            // Consume the '.'.
            advance();
            while (isDigit(peek())) advance();
        }
        return makeToken(TOKEN_NUMBER);
    }
    
    Token ConcreteScanner::next() {
        skipWhitespace();
        start = current;
        if (isAtEnd()) return makeToken(TOKEN_EOF);
        
        char c = advance();
        if (isAlpha(c)) return identifier();
        if (isDigit(c)) return number();
        
        switch (c) {
            case '(': return makeToken(TOKEN_LEFT_PAREN);
            case ')': return makeToken(TOKEN_RIGHT_PAREN);
            case '{': return makeToken(TOKEN_LEFT_BRACE);
            case '}': return makeToken(TOKEN_RIGHT_BRACE);
            case ';': return makeToken(TOKEN_SEMICOLON);
            case ',': return makeToken(TOKEN_COMMA);
            case '.': return makeToken(TOKEN_DOT);
            case '-': return makeToken(TOKEN_MINUS);
            case '+': return makeToken(TOKEN_PLUS);
            case '/': return makeToken(TOKEN_SLASH);
            case '*': return makeToken(TOKEN_STAR);
            case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
            case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
            case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
            case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
            case '"': return string();
                
                
        }
        
        return errorToken("Unexpected character.");
    }
    
    Tokenizer* Tokenizer::make(const char* first, const char* last) {
        return new ConcreteScanner(first, last);
    }
    
} // namespace lox
