//
//  compiler.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common.hpp"
#include "compiler.hpp"
#include "opcodes.hpp"
#include "memory.hpp"
#include "scanner.hpp"

#ifdef DEBUG_PRINT_CODE
#include "debug.hpp"
#endif

namespace {
    
    enum Precedence {
        PREC_NONE,
        PREC_ASSIGNMENT,  // =
        PREC_OR,          // or
        PREC_AND,         // and
        PREC_EQUALITY,    // == !=
        PREC_COMPARISON,  // < > <= >=
        PREC_TERM,        // + -
        PREC_FACTOR,      // * /
        PREC_UNARY,       // ! -
        PREC_CALL,        // . ()
        PREC_PRIMARY,
    };
    
    struct Parser {
        Token current;
        Token previous;
        bool hadError;
        bool panicMode;
        
        void errorAt(Token* token, const char* message);
        void error(const char* message);
        void errorAtCurrent(const char* message);
        
        void advance();
        void consume(TokenType type, const char* message);
        bool check(TokenType type);
        bool match(TokenType type);

        
    };
    
    struct Compiler;
    
    std::vector<Compiler*> compilers_that_are_roots;
        
    using ParseFn = void (Compiler::*)(bool canAssign);
    
    struct ParseRule {
        ParseFn prefix;
        ParseFn infix;
        Precedence precedence;
    };
    
    struct Local {
        Token name;
        int depth;
        bool isCaptured;
    };
    
    struct Upvalue {
        uint8_t index;
        bool isLocal;
    };
    
    enum FunctionType {
        TYPE_FUNCTION,
        TYPE_INITIALIZER,
        TYPE_METHOD,
        TYPE_SCRIPT,
    };
    
    struct Compiler {
        
        Compiler* enclosing;
        ObjectFunction* function;
        FunctionType type;
        
        Local locals[UINT8_COUNT];
        int localCount;
        Upvalue upvalues[UINT8_COUNT];
        int scopeDepth;
        
        Chunk* chunk();
        
        void emitByte(uint8_t byte);
        void emitBytes(uint8_t byte1, uint8_t byte2);
        void emitLoop(ptrdiff_t loopStart);
        ptrdiff_t emitJump(uint8_t instruction);
        void emitReturn();
        uint8_t makeConstant(Value value);
        void emitConstant(Value value);
        void patchJump(ptrdiff_t offset);
        
        void beginScope();
        void endScope();

        uint8_t identifierConstant(Token* name);
        int resolveLocal(Token* name);
        int addUpvalue(uint8_t index, bool isLocal);
        int resolveUpvalue(Token* name);

        void addLocal(Token name);
        void declareVariable();
        uint8_t parseVariable(const char* errorMessage);
        void markInitialized();
        void defineVariable(uint8_t global);
        
        
        void namedVariable(Token name, bool canAssign);
                
        uint8_t argumentList();

        void and_(bool canAssign);
        void binary(bool canAssign);
        void call(bool canAssign);
        void dot(bool canAssign);
        void literal(bool canAssign);
        void grouping(bool canAssign);
        void number(bool canAssign);
        void or_(bool canAssign);
        void string(bool canAssign);
        void super_(bool canAssign);
        void this_(bool canAssign);
        void unary(bool canAssign);
        void variable(bool canAssign);
        
        void expression();
        void block();
        void functionDefinition(FunctionType);
        void funDeclaration();
        void statement();
        void method();
        void declaration();
        void parsePrecedence(Precedence precedence);
        void classDeclaration();
        void varDeclaration();
        void expressionStatement();
        void forStatement();
        void ifStatement();
        void printStatement();
        void returnStatement();
        void whileStatement();


    };
    
    struct ClassCompiler {
        ClassCompiler* enclosing;
        bool hasSuperclass;
    };
    
    Parser parser;
    Compiler* current = NULL;
    ClassCompiler* currentClass = NULL;
    
    
    
#pragma mark Parser

    void Parser::errorAt(Token* token, const char* message) {
        if (panicMode) return;
        panicMode = true;
        fprintf(stderr, "[line %d] Error", token->line);
        if (token->type == TOKEN_EOF) {
            fprintf(stderr, " at end");
        } else if (token->type == TOKEN_ERROR) {
            // Nothing.
        } else {
            fprintf(stderr, " at '%.*s'", token->length, token->start);
        }
        fprintf(stderr, ": %s\n", message);
        hadError = true;
    }
    
    void Parser::error(const char* message) {
        errorAt(&previous, message);
    }
    
    void Parser::errorAtCurrent(const char* message) {
        errorAt(&current, message);
    }
    
    void Parser::advance() {
        previous = current;
        for (;;) {
            current = scanToken();
            if (current.type != TOKEN_ERROR) break;
            
            errorAtCurrent(current.start);
        }
    }
    
    void Parser::consume(TokenType type, const char* message) {
        if (current.type == type) {
            advance();
            return;
        }
        errorAtCurrent(message);
    }
    
    bool Parser::check(TokenType type) {
        return current.type == type;
    }
    
    bool Parser::match(TokenType type) {
        if (!check(type)) return false;
        advance();
        return true;
    }
    
    
    
#pragma mark Compiler
    
    Chunk* Compiler::chunk() {
        return &function->chunk;
    }
    
    void Compiler::emitByte(uint8_t byte) {
        chunk()->write(byte, parser.previous.line);
    }
    
    void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
        emitByte(byte1);
        emitByte(byte2);
    }
    
    void Compiler::emitLoop(ptrdiff_t loopStart) {
        emitByte(OPCODE_LOOP);
        ptrdiff_t offset = chunk()->code.size() - loopStart + 2;
        if (offset > UINT16_MAX) parser.error("Loop body too large.");
        // TODO: Big-endian
        emitByte((offset >> 8) & 0xff);
        emitByte(offset & 0xff);
    }
    
    ptrdiff_t Compiler::emitJump(uint8_t instruction) {
        emitByte(instruction);
        emitByte(0xff); // <-- padding will be patched later
        emitByte(0xff);
        return chunk()->code.size() - 2;
    }
    
    void Compiler::emitReturn() {
        if (type == TYPE_INITIALIZER) {
            emitBytes(OPCODE_GET_LOCAL, 0);
        } else {
            emitByte(OPCODE_NIL);
        }
        emitByte(OPCODE_RETURN);
    }
    
    uint8_t Compiler::makeConstant(Value value) {
        ptrdiff_t constant = chunk()->add_constant(value);
        if (constant > UINT8_MAX) {
            parser.error("Too many constants in one chunk.");
            return 0;
        }
        
        return (uint8_t)constant;
    }
    
    void Compiler::emitConstant(Value value) {
        emitBytes(OPCODE_CONSTANT, makeConstant(value));
    }
    
    void Compiler::patchJump(ptrdiff_t offset) {
        // -2 to adjust for the bytecode for the jump offset itself
        ptrdiff_t jump = chunk()->code.size() - offset - 2;
        if (jump > UINT16_MAX) {
            parser.error("Too much code to jump over.");
        }
        // this is big-endian for some stupid reason
        chunk()->code[offset] = (jump >> 8) & 0xff;
        chunk()->code[offset + 1] = jump & 0xff;
    }
    
    void initCompiler(Compiler* compiler, FunctionType type) {
        compiler->enclosing = current;
        compiler->function = NULL;
        compiler->type = type;
        compiler->localCount = 0;
        compiler->scopeDepth = 0;
        compiler->function = new(0) ObjectFunction();
        current = compiler;
        if (type != TYPE_SCRIPT) {
            compiler->function->name = copyString(parser.previous.start,
                                                 parser.previous.length);
        }
        
        Local* local = &compiler->locals[compiler->localCount++];
        local->depth = 0;
        local->isCaptured = false;
        if (type != TYPE_FUNCTION) {
            local->name.start = "this";
            local->name.length = 4;
        } else {
            local->name.start = "";
            local->name.length = 0;
        }
    }
    
    ObjectFunction* endCompiler(Compiler* compiler) {
        assert(current == compiler);
        compiler->emitReturn();
        ObjectFunction* function = compiler->function;
        
#ifdef DEBUG_PRINT_CODE
        if (!parser.hadError) {
            disassembleChunk(compiler->chunk(), function->name != NULL ? function->name->chars : "<script>");
        }
#endif
        current = current->enclosing;
        return function;
    }
    
    void Compiler::beginScope() {
        scopeDepth++;
    }
    
    void Compiler::endScope() {
        scopeDepth--;
        while (localCount > 0 &&
               locals[localCount - 1].depth > scopeDepth) {
            if (locals[ localCount - 1].isCaptured) {
                emitByte(OPCODE_CLOSE_UPVALUE);
            } else {
                emitByte(OPCODE_POP);
            }
            localCount--;
        }
    }
    
    ParseRule* getRule(TokenType type);
    
    uint8_t Compiler::identifierConstant(Token* name) {
        return makeConstant(Value(copyString(name->start, name->length)));
    }
    
    bool identifiersEqual(Token* a, Token* b) {
        if (a->length != b->length)
            return false;
        return memcmp(a->start, b->start, a->length) == 0;
    }
    
    int Compiler::resolveLocal(Token* name) {
        for (int i = localCount - 1; i >= 0; i--) {
            Local* local = &locals[i];
            if (identifiersEqual(name, &local->name)) {
                if (local->depth == -1) {
                    parser.error("Can't read local variable in its own initializer");
                }
                return i;
            }
        }
        
        return -1;
    }
    
    int Compiler::addUpvalue(uint8_t index, bool isLocal) {
        int upvalueCount = function->upvalueCount;
        
        for (int i = 0; i != upvalueCount; ++i) {
            Upvalue* upvalue = &upvalues[i];
            if (upvalue->index == index && upvalue->isLocal == isLocal) {
                return i;
            }
        }
        
        if (upvalueCount == UINT8_COUNT) {
            parser.error("Too many closure variables in function.");
            return 0;
        }
        
        upvalues[upvalueCount].isLocal = isLocal;
        upvalues[upvalueCount].index = index;
        return function->upvalueCount++;
    }
    
    int Compiler::resolveUpvalue(Token* name) {
        if (enclosing == NULL) return -1;
        
        int local = enclosing->resolveLocal(name);
        if (local != -1) {
            enclosing->locals[local].isCaptured = true;
            return addUpvalue((uint8_t)local, true);
        }
        
        int upvalue = enclosing->resolveUpvalue(name);
        if (upvalue != -1) {
            return addUpvalue((uint8_t)upvalue, false);
        }
        
        return -1;
    }
    
    void Compiler::addLocal(Token name) {
        if (localCount == UINT8_COUNT) {
            parser.error("Too many local variables in function.");
            return;
        }
        Local* local = &locals[ localCount++];
        local->name = name;
        local->depth = -1; // Sentinel value for uninitialized variables.
        local->isCaptured = false;
    }
    
    void Compiler::declareVariable() {
        if (scopeDepth == 0)
            return;
        Token* name = &parser.previous;
        for (int i = localCount - 1; i >= 0; i--) {
            Local* local = &locals[i];
            if (local->depth != -1 && local->depth < scopeDepth) {
                break;
            }
            if (identifiersEqual(name, &local->name)) {
                parser.error("Already a variable with this name in this scope.");
            }
        }
        addLocal(*name);
    }
    
    uint8_t Compiler::parseVariable(const char* errorMessage) {
        parser.consume(TOKEN_IDENTIFIER, errorMessage);
        declareVariable();
        if (scopeDepth > 0) return 0;
        return identifierConstant(&parser.previous);
    }
    
    // not GC mark
    void Compiler::markInitialized() {
        if (scopeDepth == 0) return;
        locals[localCount - 1].depth =
        scopeDepth;
    }
    
    void Compiler::defineVariable(uint8_t global) {
        if (scopeDepth > 0) {
            markInitialized();
            return;
        }
        emitBytes(OPCODE_DEFINE_GLOBAL, global);
    }
    
    uint8_t Compiler::argumentList() {
        uint8_t argCount = 0;
        if (!parser.check(TOKEN_RIGHT_PAREN)) {
            do {
                expression();
                argCount++;
                if (argCount == 255) {
                    parser.error("Can't have more than 255 arguments.");
                }
            } while (parser.match(TOKEN_COMMA));
        }
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after rguments.");
        return argCount;
    }
    
    void Compiler::and_(bool canAssign) {
        ptrdiff_t endJump = emitJump(OPCODE_JUMP_IF_FALSE);
        emitByte(OPCODE_POP);
        parsePrecedence(PREC_AND);
        patchJump(endJump);
    }
    
    
    void Compiler::binary(bool canAssign) {
        TokenType operatorType = parser.previous.type;
        ParseRule* rule = getRule(operatorType);
        parsePrecedence((Precedence)(rule->precedence + 1));
        
        switch (operatorType) {
            case TOKEN_BANG_EQUAL: emitBytes(OPCODE_EQUAL, OPCODE_NOT); break;
            case TOKEN_EQUAL_EQUAL: emitByte(OPCODE_EQUAL); break;
            case TOKEN_GREATER: emitByte(OPCODE_GREATER); break;
            case TOKEN_GREATER_EQUAL: emitBytes(OPCODE_LESS, OPCODE_NOT); break;
            case TOKEN_LESS: emitByte(OPCODE_LESS); break;
            case TOKEN_LESS_EQUAL: emitBytes(OPCODE_GREATER, OPCODE_NOT); break;
            case TOKEN_PLUS: emitByte(OPCODE_ADD); break;
            case TOKEN_MINUS: emitByte(OPCODE_SUBTRACT); break;
            case TOKEN_STAR: emitByte(OPCODE_MULTIPLY); break;
            case TOKEN_SLASH: emitByte(OPCODE_DIVIDE); break;
            default: return; // Unreachable.
        }
    }
    
    void Compiler::call(bool canAssign) {
        uint8_t argCount = argumentList();
        emitBytes(OPCODE_CALL, argCount);
    }
    
    void Compiler::dot(bool canAssign) {
        parser.consume(TOKEN_IDENTIFIER, "Expect property name after '.'");
        uint8_t name = identifierConstant(&parser.previous);
        
        if (canAssign && parser.match(TOKEN_EQUAL)) {
            expression();
            emitBytes(OPCODE_SET_PROPERTY, name);
        } else if (parser.match(TOKEN_LEFT_PAREN)) {
            uint8_t argCount = argumentList();
            emitBytes(OPCODE_INVOKE, name);
            emitByte(argCount);
        } else {
            emitBytes(OPCODE_GET_PROPERTY, name);
        }
    }
    
    void Compiler::literal(bool canAssign) {
        switch (parser.previous.type) {
            case TOKEN_FALSE: emitByte(OPCODE_FALSE); break;
            case TOKEN_NIL: emitByte(OPCODE_NIL); break;
            case TOKEN_TRUE: emitByte(OPCODE_TRUE); break;
            default: return; // Unreachable.
        }
        
    }
    
    void Compiler::grouping(bool canAssign) {
        expression();
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    }
    
    void Compiler::number(bool canAssign) {
        int64_t value = strtoll(parser.previous.start, NULL, 10);
        emitConstant(Value(value));
    }
    
    void Compiler::or_(bool canAssign) {
        ptrdiff_t elseJump = emitJump(OPCODE_JUMP_IF_FALSE);
        ptrdiff_t endJump = emitJump(OPCODE_JUMP);
        
        patchJump(elseJump);
        emitByte(OPCODE_POP);
        
        parsePrecedence(PREC_OR);
        patchJump(endJump);
    }
    
    void Compiler::string(bool canAssign) {
        emitConstant(Value(copyString(parser.previous.start + 1,
                                      parser.previous.length - 2)));
    }
    
    void Compiler::namedVariable(Token name, bool canAssign) {
        uint8_t getOp, setOp;
        int arg = resolveLocal(&name);
        if (arg != -1) {
            getOp = OPCODE_GET_LOCAL;
            setOp = OPCODE_SET_LOCAL;
        } else if ((arg = resolveUpvalue(&name)) != -1) {
            getOp = OPCODE_GET_UPVALUE;
            setOp = OPCODE_SET_UPVALUE;
        } else {
            arg = identifierConstant(&name);
            getOp = OPCODE_GET_GLOBAL;
            setOp = OPCODE_SET_GLOBAL;
        }
        
        if (canAssign && parser.match(TOKEN_EQUAL)) {
            expression();
            emitBytes(setOp, arg);
        } else {
            emitBytes(getOp, arg);
        }
    }
    
    void Compiler::variable(bool canAssign) {
        namedVariable(parser.previous, canAssign);
    }
    
    Token syntheticToken(const char* text) {
        Token token;
        token.start = text;
        token.length = (int)strlen(text);
        return token;
    }
    
    void Compiler::super_(bool canAssign) {
        if (currentClass == NULL) {
            parser.error("Can't use 'super' outside of a class.");
        } else if (!currentClass->hasSuperclass) {
            parser.error("Can't user 'super' in a class with no superclass.");
        }
        
        parser.consume(TOKEN_DOT, "Expect '.' after super.");
        parser.consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
        uint8_t name = identifierConstant(&parser.previous);
        namedVariable(syntheticToken("this"), false);
        if (parser.match(TOKEN_LEFT_PAREN)) {
            uint8_t argCount = argumentList();
            namedVariable(syntheticToken("super"), false);
            emitBytes(OPCODE_SUPER_INVOKE, name);
            emitByte(argCount);
        } else {
            namedVariable(syntheticToken("super"), false);
            emitBytes(OPCODE_GET_SUPER, name);
        }
    }
    
    void Compiler::this_(bool canAssign) {
        if (currentClass == NULL) {
            parser.error("Can't use 'this' outside of a class.");
            return;
        }
        variable(false);
    }
    
    void Compiler::expression() {
         parsePrecedence(PREC_ASSIGNMENT);
    }
    
    void Compiler::block() {
        while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF)) {
             declaration();
        }
        parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    }
    
    void Compiler::functionDefinition(FunctionType type) {
        Compiler compiler;
        initCompiler(&compiler, type);
        compiler.beginScope();
        
        parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
        if (!parser.check(TOKEN_RIGHT_PAREN)) {
            do {
                compiler.function->arity++;
                if (compiler.function->arity > 255) {
                    parser.errorAtCurrent("Can't have more than 255 parameters.");
                }
                uint8_t constant = compiler.parseVariable("Expect parameter name.");
                compiler.defineVariable(constant);
            } while (parser.match(TOKEN_COMMA));
        }
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
        parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
        compiler.block();
        
        ObjectFunction* function = endCompiler(&compiler);
        emitBytes(OPCODE_CLOSURE, makeConstant(Value(function)));
        
        for (int i = 0; i < function->upvalueCount; i++) {
            emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
            emitByte(compiler.upvalues[i].index);
        }
        
    }
    
    void Compiler::method() {
        parser.consume(TOKEN_IDENTIFIER, "Expect method name.");
        uint8_t constant =  identifierConstant(&parser.previous);
        
        FunctionType type = TYPE_METHOD;
        if (parser.previous.length == 4 &&
            memcmp(parser.previous.start, "init", 4) == 0) {
            type = TYPE_INITIALIZER;
        }
        functionDefinition(type);
         emitBytes(OPCODE_METHOD, constant);
    }
    
    void Compiler::classDeclaration() {
        parser.consume(TOKEN_IDENTIFIER, "Expect class name.");
        Token className = parser.previous;
        uint8_t nameConstant =  identifierConstant(&parser.previous);
        declareVariable();
        
        emitBytes(OPCODE_CLASS, nameConstant);
        defineVariable(nameConstant);
        
        ClassCompiler classCompiler;
        classCompiler.enclosing = currentClass;
        classCompiler.hasSuperclass = false;
        currentClass = &classCompiler;
        
        if (parser.match(TOKEN_LESS)) {
            parser.consume(TOKEN_IDENTIFIER, "Expect superclass name.");
            variable(false);
            
            if (identifiersEqual(&className, &parser.previous)) {
                parser.error("A class can't inherit from itself.");
            }
            
            beginScope();
            addLocal(syntheticToken("super"));
            defineVariable(0);
            
            namedVariable(className, false);
            emitByte(OPCODE_INHERIT);
            classCompiler.hasSuperclass = true;
        }
        
        namedVariable(className, false);
        parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
        while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF)) {
            method();
        }
        parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
        emitByte(OPCODE_POP);
        
        if (classCompiler.hasSuperclass) {
            endScope();
        }
        
        currentClass = classCompiler.enclosing;
    }
    
    void Compiler::funDeclaration() {
        uint8_t global = parseVariable("Expect function name.");
        markInitialized();
        functionDefinition(TYPE_FUNCTION);
        defineVariable(global);
    }
    
    void Compiler::varDeclaration() {
        uint8_t global = parseVariable("Expect variable name.");
        if (parser.match(TOKEN_EQUAL)) {
            expression();
        } else {
            emitByte(OPCODE_NIL);
        }
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
        defineVariable(global);
    }
    
    void Compiler::expressionStatement() {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after value.");
        emitByte(OPCODE_POP);
    }
    
    void Compiler::forStatement() {
        beginScope();
        parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
        if (parser.match(TOKEN_SEMICOLON)) {
            // No initializer.
        } else if (parser.match(TOKEN_VAR)) {
            varDeclaration();
        } else {
            expressionStatement();
        }
        
        ptrdiff_t loopStart = chunk()->code.size();
        ptrdiff_t exitJump = -1;
        if (!parser.match(TOKEN_SEMICOLON)) {
            expression();
            parser.consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
            
            // Jump out of the loop if the condition is false.
            exitJump = emitJump(OPCODE_JUMP_IF_FALSE);
            emitByte(OPCODE_POP); // Condition
        }
        if (!parser.match(TOKEN_RIGHT_PAREN)) {
            ptrdiff_t bodyJump = emitJump(OPCODE_JUMP);
            ptrdiff_t incrementStart = chunk()->code.size();
            expression();
            emitByte(OPCODE_POP);
            parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
            
            emitLoop(loopStart);
            loopStart = incrementStart;
            patchJump(bodyJump);
        }
        
        
        statement();
        emitLoop(loopStart);
        
        if (exitJump != -1) {
            patchJump(exitJump);
            emitByte(OPCODE_POP); // Condition
        }
        
        endScope();
    }
    
    void Compiler::ifStatement() {
        parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
        expression();
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
        ptrdiff_t thenJump = emitJump(OPCODE_JUMP_IF_FALSE);
        emitByte(OPCODE_POP);
        statement();
        ptrdiff_t elseJump = emitJump(OPCODE_JUMP);
        patchJump(thenJump);
        emitByte(OPCODE_POP);
        
        if (parser.match(TOKEN_ELSE)) statement();
        patchJump(elseJump);
        
    }
    
    void Compiler::printStatement() {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after value.");
        emitByte(OPCODE_PRINT);
    }
    
    void Compiler::returnStatement() {
        if (type == TYPE_SCRIPT) {
            parser.error("Can't return from top-level code.");
        }
        
        if (parser.match(TOKEN_SEMICOLON)) {
            emitReturn();
        } else {
            if (type == TYPE_INITIALIZER) {
                parser.error("Can't return a value from an initializer.");
            }
            
            expression();
            parser.consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
            emitByte(OPCODE_RETURN);
        }
    }
    
    void Compiler::whileStatement() {
        ptrdiff_t loopStart = chunk()->code.size();
        parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
        expression();
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
        
        ptrdiff_t exitJump = emitJump(OPCODE_JUMP_IF_FALSE);
        emitByte(OPCODE_POP);
        statement();
        emitLoop(loopStart);
        
        patchJump(exitJump);
    }
    
    void synchronize() {
        parser.panicMode = false;
        while (parser.current.type != TOKEN_EOF) {
            if (parser.previous.type == TOKEN_SEMICOLON) return;
            switch (parser.current.type) {
                case TOKEN_CLASS:
                case TOKEN_FUN:
                case TOKEN_VAR:
                case TOKEN_FOR:
                case TOKEN_IF:
                case TOKEN_WHILE:
                case TOKEN_PRINT:
                case TOKEN_RETURN:
                    return;
                    
                default:
                    // Do nothing.
                    break;
            }
            
            parser.advance();
        }
    }
    
    void Compiler::declaration() {
        if (parser.match(TOKEN_CLASS)) {
            classDeclaration();
        } else if (parser.match(TOKEN_FUN)) {
            funDeclaration();
        } else if (parser.match(TOKEN_VAR)) {
            varDeclaration();
        } else {
            statement();
        }
        if (parser.panicMode) synchronize();
    }
    
    void Compiler::statement() {
        if (parser.match(TOKEN_PRINT)) {
            printStatement();
        } else if (parser.match(TOKEN_FOR)) {
            forStatement();
        } else if (parser.match(TOKEN_IF)) {
            ifStatement();
        } else if (parser.match(TOKEN_RETURN)) {
            returnStatement();
        } else if (parser.match(TOKEN_WHILE)) {
            whileStatement();
        } else if (parser.match(TOKEN_LEFT_BRACE)) {
            beginScope();
            block();
            endScope();
        } else {
            expressionStatement();
        }
    }
    
    
    void Compiler::unary(bool canAssign) {
        TokenType operatorType = parser.previous.type;
        
        // Compile the operand.
        parsePrecedence(PREC_UNARY);
        
        // Emit the operator instruction.
        switch (operatorType) {
            case TOKEN_BANG: emitByte(OPCODE_NOT); break;
            case TOKEN_MINUS: emitByte(OPCODE_NEGATE); break;
            default: return; // Unreachable.
        }
    }
    
    ParseRule rules[] = {
        [TOKEN_LEFT_PAREN] = { &Compiler::grouping, &Compiler::call, PREC_CALL },
        [TOKEN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE },
        [TOKEN_LEFT_BRACE] = { NULL, NULL, PREC_NONE },
        [TOKEN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE },
        [TOKEN_COMMA] = { NULL, NULL, PREC_NONE },
        [TOKEN_DOT] = { NULL, &Compiler::dot, PREC_CALL },
        [TOKEN_MINUS] = { &Compiler::unary, &Compiler::binary, PREC_TERM },
        [TOKEN_PLUS] = { NULL, &Compiler::binary, PREC_TERM },
        [TOKEN_SEMICOLON] = { NULL, NULL, PREC_NONE },
        [TOKEN_SLASH] = { NULL, &Compiler::binary, PREC_FACTOR },
        [TOKEN_STAR] = { NULL, &Compiler::binary, PREC_FACTOR },
        [TOKEN_BANG] = { &Compiler::unary, NULL, PREC_NONE },
        [TOKEN_BANG_EQUAL] = { NULL, &Compiler::binary, PREC_EQUALITY },
        [TOKEN_EQUAL] = { NULL, NULL, PREC_NONE },
        [TOKEN_EQUAL_EQUAL] = { NULL, &Compiler::binary, PREC_EQUALITY },
        [TOKEN_GREATER] = { NULL, &Compiler::binary, PREC_COMPARISON },
        [TOKEN_GREATER_EQUAL] = { NULL, &Compiler::binary, PREC_COMPARISON },
        [TOKEN_LESS] = { NULL, &Compiler::binary, PREC_COMPARISON },
        [TOKEN_LESS_EQUAL] = { NULL, &Compiler::binary, PREC_COMPARISON },
        [TOKEN_IDENTIFIER] = { &Compiler::variable, NULL, PREC_NONE },
        [TOKEN_STRING] = { &Compiler::string, NULL, PREC_NONE },
        [TOKEN_NUMBER] = { &Compiler::number, NULL, PREC_NONE },
        [TOKEN_AND]  = { NULL, &Compiler::and_, PREC_AND },
        [TOKEN_CLASS]  = { NULL, NULL, PREC_NONE },
        [TOKEN_ELSE]   = { NULL, NULL, PREC_NONE },
        [TOKEN_FALSE]   = { &Compiler::literal, NULL, PREC_NONE },
        [TOKEN_FOR]          = { NULL, NULL, PREC_NONE },
        [TOKEN_FUN]           = { NULL, NULL, PREC_NONE },
        [TOKEN_IF]            = { NULL, NULL, PREC_NONE },
        [TOKEN_NIL]           = { &Compiler::literal, NULL, PREC_NONE },
        [TOKEN_OR]            = { NULL,     &Compiler::or_,    PREC_OR         },
        [TOKEN_PRINT]         = { NULL, NULL, PREC_NONE },
        [TOKEN_RETURN]        = { NULL, NULL, PREC_NONE },
        [TOKEN_SUPER]         = { &Compiler::super_, NULL, PREC_NONE },
        [TOKEN_THIS]          = { &Compiler::this_,    NULL,   PREC_NONE       },
        [TOKEN_TRUE]          = { &Compiler::literal, NULL, PREC_NONE },
        [TOKEN_VAR]           = { NULL, NULL, PREC_NONE },
        [TOKEN_WHILE]         = { NULL, NULL, PREC_NONE },
        [TOKEN_ERROR]         = { NULL, NULL, PREC_NONE },
        [TOKEN_EOF]           = { NULL, NULL, PREC_NONE },
    };
    
    void Compiler::parsePrecedence(Precedence precedence) {
        parser.advance();
        ParseFn prefixRule = getRule(parser.previous.type)->prefix;
        if (prefixRule == NULL) {
            parser.error("Expect expression.");
            return;
        }
        
        bool canAssign = precedence <= PREC_ASSIGNMENT;
        std::invoke(prefixRule, this, canAssign);
        
        while (precedence <= getRule(parser.current.type)->precedence) {
            parser.advance();
            ParseFn infixRule = getRule(parser.previous.type)->infix;
            std::invoke(infixRule, this, canAssign);
        }
        
        if (canAssign && parser.match(TOKEN_EQUAL)) {
            parser.error("Invalid assignment target.");
        }
        
    }
    
    ParseRule* getRule(TokenType type) {
        return &rules[type];
    }
    
}

ObjectFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    
    parser.hadError = false;
    parser.panicMode = false;
    
    parser.advance();
    while (!parser.match(TOKEN_EOF)) {
        compiler.declaration();
    }
    ObjectFunction* function = endCompiler(&compiler);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject(compiler->function);
        compiler = compiler->enclosing;
    }
}
