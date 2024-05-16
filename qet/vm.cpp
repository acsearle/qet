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
#include "object.hpp"
#include "opcodes.hpp"
#include "string.hpp"
#include "vm.hpp"

namespace lox {
    
    // VM vm;
    
    GC gc;
    
    static Value clockNative(int argCount, AtomicValue* args) {
        return Value((int64_t)(clock() / CLOCKS_PER_SEC));
    }
    
    void VM::resetStack() {
        stackTop = stack;
        frameCount = 0;
        openUpvalues = nullptr;
    }
    
    void VM::runtimeError(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputs("\n", stderr);
        
        for (int i = frameCount - 1; i >= 0; i--) {
            CallFrame* frame = &frames[i];
            const ObjectFunction* function = frame->closure->function;
            ptrdiff_t instruction = frame->ip - function->chunk.code.data() - 1;
            fprintf(stderr, "[line %d] in ",
                    function->chunk.lines[instruction]);
            if (function->name == NULL) {
                fprintf(stderr, "script\n");
            } else {
                fprintf(stderr, "%s()\n", function->name->_data);
            }
        }
        
        resetStack();
    }
    
    void VM::defineNative(const char* name, NativeFn function) {
        tableSet(&globals, 
                 copyString(name, (int) strlen(name)),
                 Value(new ObjectNative(function)));
    }
    
    void VM::initVM() {
        resetStack();
        initTable(&globals);
        defineNative("clock", clockNative);
    }
    
    void initGC() {
        //gc.objects = NULL;
        //gc.bytesAllocated = 0;
        //gc.nextGC = 1024 * 1024;
        //initTable(&gc.strings);
        gc.initString = NULL;
        gc.initString = copyString("init", 4);
    }
    
    void VM::freeVM() {
        freeTable(&globals);
        initVM();
    }
    
    void freeGC() {
       // freeTable(&gc.strings);
        gc.initString = NULL;
        //freeObjects();
        initGC(); // oddly this makes a new initString
    }
    
    void VM::push(Value value) {
        *stackTop = value;
        stackTop++;
    }
    
    Value VM::pop() {
        stackTop--;
        Value value = stackTop->reset();
        gc::shade(value.as_object());
        return value;
    }
    
    Value VM::peek(int distance) {
        return stackTop[-1 - distance].load();
    }
    
    bool VM::call(ObjectClosure* closure, int argCount) {
        if (argCount != closure->function->arity) {
            runtimeError("Expected %d arguments but got %d.",
                         closure->function->arity, argCount);
            return false;
        }
        
        if (frameCount == FRAMES_MAX) {
            runtimeError("Stack overflow.");
            return false;
        }
        
        CallFrame* frame = &frames[frameCount++];
        frame->closure = closure;
        frame->ip = closure->function->chunk.code.data();
        frame->slots = stackTop - argCount - 1;
        return true;
    }
    
    bool Object::callObject(VM& vm, int argCount) {
        vm.runtimeError("Can only call functions and classes.");
        return false;
    }
    
    bool ObjectBoundMethod::callObject(VM& vm, int argCount) {
        vm.stackTop[-argCount - 1] = receiver;
        return vm.call(this->method, argCount);
    }

    bool ObjectClass::callObject(VM& vm, int argCount) {
        vm.stackTop[-argCount - 1] = Value(new ObjectInstance(this));
        Value initializer;
        if (tableGet(&this->methods, gc.initString, &initializer)) {
            return vm.call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
            vm.runtimeError("Expected 0 arguments but got %d.\n", argCount);
            return false;
        }
        return true;
    }

    bool ObjectClosure::callObject(VM& vm, int argCount) {
        return vm.call(this, argCount);
    }
    
    bool ObjectNative::callObject(VM& vm, int argCount) {
        Value result = this->function(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        vm.push(result);
        return true;
    }
    
    bool VM::callValue(Value callee, int argCount) {
        return callee.as_object()->callObject(*this, argCount);
    }

    bool VM::invokeFromClass(ObjectClass* class_, ObjectString* name, int argCount) {
        Value method;
        if (!tableGet(&class_->methods, name, &method)) {
            runtimeError("Undefined property '%s'.\n");
            return false;
        }
        return call(AS_CLOSURE(method), argCount);
    }
    
    bool VM::invoke(ObjectString* name, int argCount) {
        Value receiver = peek(argCount);
        
        ObjectInstance* instance = dynamic_cast<ObjectInstance*>(receiver.as_object());

        
        if (!instance) {
            runtimeError("Only instances have methods.");
            return false;
        }
                
        Value value;
        if (tableGet(&instance->fields, name, &value)) {
            // Name is a field, and shadows any method with the same name.
            stackTop[-argCount - 1] = value;
            return callValue(value, argCount);
        }
        
        // Name is not a field, and must be a method (or undefined).
        return invokeFromClass(instance->class_, name, argCount);
    }
    
    bool VM::bindMethod(ObjectClass* class_, ObjectString* name) {
        Value method;
        if (!tableGet(&class_->methods, name, &method)) {
            runtimeError("Undefined property '%s'.", name->_data);
            return false;
        }
        ObjectBoundMethod* bound = new ObjectBoundMethod(peek(0), AS_CLOSURE(method));
        pop();
        push(Value(bound));
        return true;
    }
    
    ObjectUpvalue* VM::captureUpvalue(AtomicValue* local) {
        ObjectUpvalue* prevUpvalue = NULL;
        ObjectUpvalue* upvalue = (ObjectUpvalue*) openUpvalues;
        while (upvalue != NULL && upvalue->location > local) {
            prevUpvalue = upvalue;
            upvalue = upvalue->next;
        }
        
        if (upvalue != NULL && upvalue->location == local) {
            return upvalue;
        }
        
        ObjectUpvalue* createdUpvalue = new ObjectUpvalue(local);
        createdUpvalue->next = upvalue;
        if (prevUpvalue == NULL) {
            openUpvalues = createdUpvalue;
        } else {
            prevUpvalue->next = createdUpvalue;
        }
        return createdUpvalue;
    }
    
    void VM::closeUpvalues(AtomicValue* last) {
        while (openUpvalues != nullptr &&
               openUpvalues->location >= last) {
            ObjectUpvalue* upvalue = (ObjectUpvalue*) openUpvalues;
            upvalue->closed = *upvalue->location;
            upvalue->location = &upvalue->closed;
            openUpvalues = upvalue->next;
        }
    }
    
    void VM::defineMethod(ObjectString* name) {
        Value method = peek(0);
        ObjectClass* class_ = AS_CLASS(peek(1));
        tableSet(&class_->methods, name, method);
        pop();
    }
    
    void VM::concatenate() {
        ObjectString* b = AS_STRING(peek(0));
        ObjectString* a = AS_STRING(peek(1));
        
        int length = a->_size + b->_size;
        char* chars = (char*) operator new(length + 1);
        memcpy(chars, a->_data, a->_size);
        memcpy(chars + a->_size, b->_data, b->_size);
        chars[length] = '\0';
        
        ObjectString* result = takeString(chars, length);
        pop();
        pop();
        push(Value(result));
    }
    
    InterpretResult VM::run() {
        CallFrame* frame = &frames[this->frameCount - 1];
        
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
        
        for (int qqq = 0;; ++qqq) {
#ifdef LOX_DEBUG_TRACE_EXECUTION
            printf("          ");
            for (AtomicValue* slot = this->stack; slot < this->stackTop; slot++) {
                printf("[ ");
                printValue(slot->load());
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk,
                                   frame->ip - frame->closure->function->chunk.code.data());
#endif
            
            // handshake every 128 instructions
            if (!(qqq & 127)) {
                // printf("---\n");
                gc::handshake();
                gc::shade(this);
            }

            
            
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
                    push(frame->slots[slot].load());
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
                    if (!tableGet(&this->globals, name, &value)) {
                        runtimeError("Undefined variable '%s'.", name->_data);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(value);
                    break;
                }
                case OPCODE_DEFINE_GLOBAL: {
                    ObjectString* name = READ_STRING();
                    tableSet(&this->globals, name, peek(0));
                    pop();
                    break;
                }
                case OPCODE_SET_GLOBAL: {
                    ObjectString* name = READ_STRING();
                    if (tableSet(&this->globals, name, peek(0))) {
                        tableDelete(&this->globals, name);
                        runtimeError("Undefined variable '%s'.", name->_data);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OPCODE_GET_UPVALUE: {
                    uint8_t slot = READ_BYTE();
                    push(frame->closure->upvalues[slot]->location->load());
                    break;
                }
                case OPCODE_SET_UPVALUE: {
                    uint8_t slot = READ_BYTE();
                    *frame->closure->upvalues[slot]->location = peek(0);
                    break;
                }
                case OPCODE_GET_PROPERTY: {
                    ObjectInstance* instance = dynamic_cast<ObjectInstance*>(peek(0).as_object());
                    if (!instance) {
                        runtimeError("Only instances have properties.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
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
                    ObjectInstance* instance = dynamic_cast<ObjectInstance*>(peek(1).as_object());
                    if (!instance) {
                        runtimeError("Only instances have properties.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
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
                    frame = &this->frames[this->frameCount - 1];
                    break;
                }
                case OPCODE_INVOKE: {
                    ObjectString* method = READ_STRING();
                    int argCount = READ_BYTE();
                    if (!invoke(method, argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    frame = &this->frames[this->frameCount - 1];
                    break;
                }
                case OPCODE_SUPER_INVOKE: {
                    ObjectString* method = READ_STRING();
                    int argCount = READ_BYTE();
                    ObjectClass* superclass = AS_CLASS(pop());
                    if (!invokeFromClass(superclass, method, argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    frame = &this->frames[this->frameCount - 1];
                    break;
                }
                case OPCODE_CLOSURE: {
                    ObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
                    ObjectClosure* closure = new(gc::extra_val_t{function->upvalueCount * sizeof(ObjectUpvalue*)}) ObjectClosure(function);
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
                    closeUpvalues(this->stackTop - 1);
                    pop();
                    break;
                }
                case OPCODE_RETURN: {
                    Value result = pop();
                    closeUpvalues(frame->slots);
                    this->frameCount--;
                    if (this->frameCount == 0) {
                        pop();
                        return INTERPRET_OK;
                    }
                    
                    this->stackTop = frame->slots;
                    push(result);
                    frame = &this->frames[this->frameCount - 1];
                    break;
                }
                case OPCODE_CLASS: {
                    push(Value(new ObjectClass(READ_STRING())));
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
    
    
    InterpretResult VM::interpret(const char* first, const char* last) {
        ObjectFunction* function = compile(first, last);
        if (function == NULL) return INTERPRET_COMPILE_ERROR;
        
        push(Value(function));
        ObjectClosure* closure = new(gc::extra_val_t{function->upvalueCount * sizeof(ObjectUpvalue*)}) ObjectClosure(function);
        pop();
        push(Value(closure));
        call(closure, 0);
        
        return run();
    }
    
    void VM::scan(gc::ScanContext& context) const {
        // We have to scan the whole fixed size arrays else we race with
        // frameCount and stackTop
        for (int i = 0; i != FRAMES_MAX; ++i)
            context.push(frames[i].closure);
        for (int i = 0; i != STACK_MAX; ++i)
            context.push(stack[i].load().as_object());
        this->globals.scan(context);
        context.push(openUpvalues);
    }
    
}
