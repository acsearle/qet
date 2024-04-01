//
//  vm.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "common.hpp"
#include "compiler.hpp"
#include "debug.hpp"
#include "memory.hpp"
#include "object.hpp"
#include "opcodes.hpp"
#include "vm.hpp"

VM vm;

GC gc;

static Value clockNative(int argCount, Value* args) {
    return Value((int64_t)(clock() / CLOCKS_PER_SEC));
}

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjectFunction* function = frame->closure->function;
        ptrdiff_t instruction = frame->ip - function->chunk.code.data() - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    
    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(Value(copyString(name, (int) strlen(name))));
    push(Value(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stackTop[-2]), vm.stackTop[-1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    
    initTable(&vm.globals);
    
    defineNative("clock", clockNative);
}

void initGC() {
    gc.objects = NULL;
    gc.bytesAllocated = 0;
    gc.nextGC = 1024 * 1024;
    initTable(&gc.strings);
    gc.initString = NULL;
    gc.initString = copyString("init", 4);
}

void freeVM() {
    freeTable(&vm.globals);
    initVM();
}

void freeGC() {
    freeTable(&gc.strings);
    gc.initString = NULL;
    freeObjects();
    initGC();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjectClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.",
                     closure->function->arity, argCount);
        return false;
    }
    
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code.data();
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (callee.is_object()) {
        switch (OBJECT_TYPE(callee)) {
            case OBJECT_BOUND_METHOD: {
                ObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJECT_CLASS: {
                ObjectClass* class_ = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = Value(newInstance(class_));
                Value initializer;
                if (tableGet(&class_->methods, gc.initString, &initializer)) {
                    return call(AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d.\n", argCount);
                    return false;
                }
                return true;
            }
            case OBJECT_CLOSURE: {
                return call(AS_CLOSURE(callee), argCount);
            }
            case OBJECT_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break; // Non-callable object.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static bool invokeFromClass(ObjectClass* class_, ObjectString* name, int argCount) {
    Value method;
    if (!tableGet(&class_->methods, name, &method)) {
        runtimeError("Undefined property '%s'.\n");
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjectString* name, int argCount) {
    Value receiver = peek(argCount);
    
    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    
    ObjectInstance* instance = AS_INSTANCE(receiver);
    
    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        // Name is a field, and shadows any method with the same name.
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    
    // Name is not a field, and must be a method (or undefined).
    return invokeFromClass(instance->class_, name, argCount);
}

static bool bindMethod(ObjectClass* class_, ObjectString* name) {
    Value method;
    if (!tableGet(&class_->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    ObjectBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(Value(bound));
    return true;
}

static ObjectUpvalue* captureUpvalue(Value* local) {
    ObjectUpvalue* prevUpvalue = NULL;
    ObjectUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }
    
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    
    ObjectUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last) {
        ObjectUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;        
    }
}

static void defineMethod(ObjectString* name) {
    Value method = peek(0);
    ObjectClass* class_ = AS_CLASS(peek(1));
    tableSet(&class_->methods, name, method);
    pop();
}

static void concatenate() {
    ObjectString* b = AS_STRING(peek(0));
    ObjectString* a = AS_STRING(peek(1));
    
    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    
    ObjectString* result = takeString(chars, length);
    pop();
    pop();
    push(Value(result));
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    
#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() (frame->closure->function->chunk.constants[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
    
#define BINARY_OP(valueType, op) \
    do { \
        if ( !peek(0).is_int64() || !peek(1).is_int64() ) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        int64_t b = pop().as_int64(); \
        int64_t a = pop().as_int64(); \
        push(Value(a op b)); \
    } while(false)
    
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk,
                               frame->ip - frame->closure->function->chunk.code.data());
#endif
        
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OPCODE_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OPCODE_NIL: push(Value()); break;
            case OPCODE_TRUE: push(Value(true)); break;
            case OPCODE_FALSE: push(Value(false)); break;
            case OPCODE_POP: pop(); break;
            case OPCODE_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OPCODE_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OPCODE_GET_GLOBAL: {
                ObjectString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OPCODE_DEFINE_GLOBAL: {
                ObjectString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OPCODE_SET_GLOBAL: {
                ObjectString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OPCODE_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OPCODE_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OPCODE_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                                
                ObjectInstance* instance = AS_INSTANCE(peek(0));
                ObjectString* name = READ_STRING();
                
                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop(); // Instance.
                    push(value);
                    break;
                }
                
                if (!bindMethod(instance->class_, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OPCODE_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjectInstance* instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OPCODE_GET_SUPER: {
                ObjectString* name = READ_STRING();
                ObjectClass* superclass = AS_CLASS(pop());
                if (!bindMethod(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OPCODE_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value(a == b));
                break;
            }
            case OPCODE_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OPCODE_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OPCODE_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (peek(0).is_int64() && peek(1).is_int64()) {
                    int64_t b = pop().as_int64();
                    int64_t a = pop().as_int64();
                    push(Value(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OPCODE_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OPCODE_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OPCODE_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OPCODE_NOT:
                push(Value(!(bool)pop()));
                break;
            case OPCODE_NEGATE:
                if (!peek(0).is_int64()) {
                    runtimeError("Operand must be a number.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(Value(-pop().as_int64()));
                break;
            case OPCODE_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OPCODE_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OPCODE_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!(bool)peek(0))
                    frame->ip += offset;
                break;
            }
            case OPCODE_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OPCODE_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OPCODE_INVOKE: {
                ObjectString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OPCODE_SUPER_INVOKE: {
                ObjectString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjectClass* superclass = AS_CLASS(pop());
                if (!invokeFromClass(superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OPCODE_CLOSURE: {
                ObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjectClosure* closure = newClosure(function);
                push(Value(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = 
                            captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OPCODE_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OPCODE_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }
                
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OPCODE_CLASS: {
                push(Value(newClass(READ_STRING())));
                break;
            }
            case OPCODE_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjectClass* subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop(); // Subclass.
                break;
            }
            case OPCODE_METHOD: {
                defineMethod(READ_STRING());
                break;
            }
        }
    }
    
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
    
}


InterpretResult interpret(const char* source) {
    ObjectFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;
    
    push(Value(function));
    ObjectClosure* closure = newClosure(function);
    pop();
    push(Value(closure));
    call(closure, 0);
    
    return run();
}
