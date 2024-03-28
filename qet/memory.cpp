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
    vm.bytesAllocated += (newSize - oldSize);
    if (newSize > oldSize
#ifndef DEBUG_STRESS_GC
        && vm.bytesAllocated > vm.nextGC
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

void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;
    
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack,
                                      sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL) exit(1);
    }
    
    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p freeObject of ObjType %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
            printf("  ObjClosure\n");
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            printf("  ObjFunction\n");
            ObjFunction* function = (ObjFunction*)object;
            printf("    Chunk\n");
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);         
            break;
        }
        case OBJ_NATIVE:
            printf("  ObjNative\n");
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            printf("  ObjString\n");
            ObjString* string = (ObjString*)object;
            printf("    \"%s\"\n", string->chars);
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            printf("  ObjUpvalue\n");
            FREE(ObjUpvalue, object);
            break;
    }
}

static void markRoots() {
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }
    
    for (int i = 0; i < vm.frameCount; ++i) {
        markObject((Obj*)vm.frames[i].closure);
    }
    
    for (ObjUpvalue* upvalue = vm.openUpvalues;
         upvalue != NULL;
         upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
}

static void traceReferences() {
    while (vm.grayCount) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }
            
            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin (marking)\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    printf("-- gc continues (tracing)\n");
    traceReferences();
    printf("-- gc continues (weak dictionary)\n");
    tableRemoveWhite(&vm.strings);
    printf("-- gc continues (sweep)\n");
    sweep();
    
    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
    
#ifdef DEBUG_LOG_GC
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated,
           before,
           vm.bytesAllocated,
           vm.nextGC);
    printf("-- gc end\n");
#endif
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}
