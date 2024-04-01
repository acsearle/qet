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


struct Parser {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
};

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

using ParseFn = void (*)(bool canAssign);

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
};

struct ClassCompiler {
    ClassCompiler* enclosing;
    bool hasSuperclass;
};

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    currentChunk()->write(byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(ptrdiff_t loopStart) {
    emitByte(OPCODE_LOOP);
    ptrdiff_t offset = currentChunk()->code.size() - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static ptrdiff_t emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->code.size() - 2;
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OPCODE_GET_LOCAL, 0);
    } else {
        emitByte(OPCODE_NIL);
    }
    
    emitByte(OPCODE_RETURN);
}

static uint8_t makeConstant(Value value) {
    ptrdiff_t constant = currentChunk()->add_constant(value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    
    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OPCODE_CONSTANT, makeConstant(value));
}

static void patchJump(ptrdiff_t offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    ptrdiff_t jump = currentChunk()->code.size() - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }
    // this is big-endian for some stupid reason
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
    

    
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }
    
    Local* local = &current->locals[current->localCount++];
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

static ObjectFunction* endCompiler() {
    emitReturn();
    ObjectFunction* function = current->function;
    
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    
    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OPCODE_CLOSE_UPVALUE);
        } else {
            emitByte(OPCODE_POP);
        }
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token* name) {
    return makeConstant(Value(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer");
            }
            return i;
        }
    }
    
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;
    
    for (int i = 0; i != upvalueCount; ++i) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }
    
    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }
    
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;
    
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }
    
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }
    
    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; // Sentinel value for uninitialized variables.
    local->isCaptured = false;
}

static void declareVariable() {
    if (current->scopeDepth == 0)
        return;
    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable();
    if (current->scopeDepth > 0) return 0;
    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth =
        current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OPCODE_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            argCount++;
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after rguments.");
    return argCount;
}

static void and_(bool canAssign) {
    ptrdiff_t endJump = emitJump(OPCODE_JUMP_IF_FALSE);
    emitByte(OPCODE_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}
    
    
static void binary(bool canAssign) {
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

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OPCODE_CALL, argCount);
}

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'");
    uint8_t name = identifierConstant(&parser.previous);
    
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OPCODE_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        emitBytes(OPCODE_INVOKE, name);
        emitByte(argCount);
    } else {
        emitBytes(OPCODE_GET_PROPERTY, name);
    }
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OPCODE_FALSE); break;
        case TOKEN_NIL: emitByte(OPCODE_NIL); break;
        case TOKEN_TRUE: emitByte(OPCODE_TRUE); break;
        default: return; // Unreachable.
    }

}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
    int64_t value = strtoll(parser.previous.start, NULL, 10);
    emitConstant(Value(value));
}

static void or_(bool canAssign) {
    ptrdiff_t elseJump = emitJump(OPCODE_JUMP_IF_FALSE);
    ptrdiff_t endJump = emitJump(OPCODE_JUMP);
    
    patchJump(elseJump);
    emitByte(OPCODE_POP);
    
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign) {
    emitConstant(Value(copyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OPCODE_GET_LOCAL;
        setOp = OPCODE_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OPCODE_GET_UPVALUE;
        setOp = OPCODE_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OPCODE_GET_GLOBAL;
        setOp = OPCODE_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, arg);
    } else {
        emitBytes(getOp, arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't user 'super' in a class with no superclass.");
    }
    
    consume(TOKEN_DOT, "Expect '.' after super.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);
    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitBytes(OPCODE_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emitBytes(OPCODE_GET_SUPER, name);
    }
}

static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();
    
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();
    
    ObjectFunction* function = endCompiler();
    emitBytes(OPCODE_CLOSURE, makeConstant(Value(function)));
    
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
    
}

static void method() {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);
    
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 &&
        memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitBytes(OPCODE_METHOD, constant);
}

static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();
    
    emitBytes(OPCODE_CLASS, nameConstant);
    defineVariable(nameConstant);
    
    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    classCompiler.hasSuperclass = false;
    currentClass = &classCompiler;
    
    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);
        
        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }
        
        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);
        
        namedVariable(className, false);
        emitByte(OPCODE_INHERIT);
        classCompiler.hasSuperclass = true;
    }
    
    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OPCODE_POP);
    
    if (classCompiler.hasSuperclass) {
        endScope();
    }
    
    currentClass = classCompiler.enclosing;
}

static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OPCODE_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OPCODE_POP);
}

static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }
    
    ptrdiff_t loopStart = currentChunk()->code.size();
    ptrdiff_t exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        
        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OPCODE_JUMP_IF_FALSE);
        emitByte(OPCODE_POP); // Condition
    }
    if (!match(TOKEN_RIGHT_PAREN)) {
        ptrdiff_t bodyJump = emitJump(OPCODE_JUMP);
        ptrdiff_t incrementStart = currentChunk()->code.size();
        expression();
        emitByte(OPCODE_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        
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

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    ptrdiff_t thenJump = emitJump(OPCODE_JUMP_IF_FALSE);
    emitByte(OPCODE_POP);
    statement();
    ptrdiff_t elseJump = emitJump(OPCODE_JUMP);
    patchJump(thenJump);
    emitByte(OPCODE_POP);
    
    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
    
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OPCODE_PRINT);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    
    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OPCODE_RETURN);
    }
}

static void whileStatement() {
    ptrdiff_t loopStart = currentChunk()->code.size();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    
    ptrdiff_t exitJump = emitJump(OPCODE_JUMP_IF_FALSE);
    emitByte(OPCODE_POP);
    statement();
    emitLoop(loopStart);
    
    patchJump(exitJump);
}

static void synchronize() {
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
        
        advance();
    }
}

static void declaration() {
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}


static void unary(bool canAssign) {
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
    [TOKEN_LEFT_PAREN]    = { grouping, call,   PREC_CALL       },
    [TOKEN_RIGHT_PAREN]   = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_LEFT_BRACE]    = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_RIGHT_BRACE]   = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_COMMA]         = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_DOT]           = { NULL,     dot,    PREC_CALL       },
    [TOKEN_MINUS]         = { unary,    binary, PREC_TERM       },
    [TOKEN_PLUS]          = { NULL,     binary, PREC_TERM       },
    [TOKEN_SEMICOLON]     = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_SLASH]         = { NULL,     binary, PREC_FACTOR     },
    [TOKEN_STAR]          = { NULL,     binary, PREC_FACTOR     },
    [TOKEN_BANG]          = { unary,    NULL,   PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { NULL,     binary, PREC_EQUALITY   },
    [TOKEN_EQUAL]         = { NULL,     NULL,   PREC_NONE       },
    [TOKEN_EQUAL_EQUAL]   = { NULL,     binary, PREC_EQUALITY   },
    [TOKEN_GREATER]       = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_LESS]          = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_IDENTIFIER]    = { variable, NULL,   PREC_NONE       },
    [TOKEN_STRING]        = { string,   NULL,   PREC_NONE       },
    [TOKEN_NUMBER]        = { number,   NULL,   PREC_NONE       },
    [TOKEN_AND]           = { NULL,     and_,   PREC_AND        },
    [TOKEN_CLASS] = { NULL, NULL, PREC_NONE },
    [TOKEN_ELSE] = { NULL, NULL, PREC_NONE },
    [TOKEN_FALSE] = { literal, NULL, PREC_NONE },
    [TOKEN_FOR] = { NULL, NULL, PREC_NONE },
    [TOKEN_FUN] = { NULL, NULL, PREC_NONE },
    [TOKEN_IF] = { NULL, NULL, PREC_NONE },
    [TOKEN_NIL] = { literal, NULL, PREC_NONE },
    [TOKEN_OR]            = { NULL,     or_,    PREC_OR         },
    [TOKEN_PRINT] = { NULL, NULL, PREC_NONE },
    [TOKEN_RETURN] = { NULL, NULL, PREC_NONE },
    [TOKEN_SUPER] = { super_, NULL, PREC_NONE },
    [TOKEN_THIS]          = { this_,    NULL,   PREC_NONE       },
    [TOKEN_TRUE] = { literal, NULL, PREC_NONE },
    [TOKEN_VAR] = { NULL, NULL, PREC_NONE },
    [TOKEN_WHILE] = { NULL, NULL, PREC_NONE },
    [TOKEN_ERROR] = { NULL, NULL, PREC_NONE },
    [TOKEN_EOF] = { NULL, NULL, PREC_NONE },
};

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }
    
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);
    
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }
    
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
    
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

ObjectFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    
    parser.hadError = false;
    parser.panicMode = false;
    
    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    ObjectFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject(compiler->function);
        compiler = compiler->enclosing;
    }
}
