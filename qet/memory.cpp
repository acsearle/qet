//
//  memory.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cstdlib>

#include "compiler.hpp"
#include "memory.hpp"
#include "vm.hpp"

#ifdef DEBUG_LOG_GC
#include <cstdio>
#include "debug.hpp"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    gc.bytesAllocated += (newSize - oldSize);
    if (newSize > oldSize
#ifndef DEBUG_STRESS_GC
        && gc.bytesAllocated > gc.nextGC
#endif
        ) {
        collectGarbage();
    }
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    
    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(Object* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(Value(object));
    printf("\n");
#endif
    object->isMarked = true;
    
    gc.grayStack.push_back(object);
}

void markValue(Value value) {
    if (value.is_object())
        markObject(value.as_object());
}

static void markArray(std::vector<Value>* array) {
    // for (int i = 0; i < array->count; i++) {
    //    markValue(array->values[i]);
    // }
    for (Value value : *array)
        markValue(value);
}

static void blackenObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(Value(object));
    printf("\n");
#endif
    
    switch (object->type) {
        case OBJECT_BOUND_METHOD: {
            ObjectBoundMethod* bound = (ObjectBoundMethod*)object;
            markValue(bound->receiver);
            markObject(bound->method);
            break;
        }
        case OBJECT_CLASS: {
            ObjectClass* class_ = (ObjectClass*)object;
            markObject(class_->name);
            markTable(&class_->methods);
            break;
        }
        case OBJECT_CLOSURE: {
            ObjectClosure* closure = (ObjectClosure*)object;
            markObject(closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(closure->upvalues[i]);
            }
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* function = (ObjectFunction*)object;
            markObject(function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJECT_INSTANCE: {
            ObjectInstance* instance = (ObjectInstance*)object;
            markObject(instance->class_);
            markTable(&instance->fields);
            break;
        }
        case OBJECT_NATIVE: {
            break;
        }
        case OBJECT_STRING: {
            break;
        }
        case OBJECT_UPVALUE: {
            markValue(((ObjectUpvalue*)object)->closed);
            break;
        }
    }
}

static void freeObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("%p freeObject of ObjectType %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJECT_BOUND_METHOD:
            FREE(ObjectBoundMethod, object);
            break;
        case OBJECT_CLASS: {
            ObjectClass* class_ = (ObjectClass*)object;
            freeTable(&class_->methods);
            FREE(OBJECT_CLASS, object);
            break;
        }
        case OBJECT_CLOSURE: {
            ObjectClosure* closure = (ObjectClosure*)object;
            FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjectClosure, object);
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* function = (ObjectFunction*)object;
            std::destroy_at(&function->chunk);
            FREE(ObjectFunction, object);
            break;
        }
        case OBJECT_INSTANCE: {
            ObjectInstance* instance = (ObjectInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjectInstance, object);
            break;
        }
        case OBJECT_NATIVE:
            FREE(ObjectNative, object);
            break;
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjectString, object);
            break;
        }
        case OBJECT_UPVALUE:
            FREE(ObjectUpvalue, object);
            break;
    }
}

static void markRoots() {

    for (Value value : gc.roots)
        markValue(value);

    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }
    
    for (int i = 0; i < vm.frameCount; ++i) {
        markObject(vm.frames[i].closure);
    }
    
    for (ObjectUpvalue* upvalue = vm.openUpvalues;
         upvalue != NULL;
         upvalue = upvalue->next) {
        markObject(upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
    markObject(gc.initString);
}

static void traceReferences() {
    while (!gc.grayStack.empty()) {
        Object* object = gc.grayStack.back();
        gc.grayStack.pop_back();
        blackenObject(object);
    }
}

static void sweep() {
    Object* previous = NULL;
    Object* object = gc.objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Object* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                gc.objects = object;
            }
            
            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin (marking)\n");
    size_t before = gc.bytesAllocated;
#endif

    markRoots();
    //printf("-- gc continues (tracing)\n");
    traceReferences();
    //printf("-- gc continues (weak dictionary)\n");
    tableRemoveWhite(&gc.strings);
    //printf("-- gc continues (sweep)\n");
    sweep();
    
    gc.nextGC = gc.bytesAllocated * GC_HEAP_GROW_FACTOR;
    
#ifdef DEBUG_LOG_GC
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - gc.bytesAllocated,
           before,
           gc.bytesAllocated,
           gc.nextGC);
    printf("-- gc end\n");
#endif
}

void freeObjects() {
    Object* object = gc.objects;
    while (object != NULL) {
        Object* next = object->next;
        freeObject(object);
        object = next;
    }
    gc.grayStack.clear();
}
