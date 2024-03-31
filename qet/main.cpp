#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common.hpp"
#include "chunk.hpp"
#include "debug.hpp"
#include "vm.hpp"


static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
    }
}



static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    
    char* buffer = (char*) malloc(fileSize + 1);
    if (buffer == nullptr) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
        
    }
    buffer[bytesRead] = '\0';
    
    fclose(file);
    return buffer;
}





void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);
    
    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

const char* preamble =
R"""(

// Exercise all samples at startup

fun outer() {
    var x = "outside";
    fun inner() {
        print x;
    }
    inner();
}
outer();

class Pair {}
var pair = Pair();
pair.first = 1;
pair.second = 2;
print pair.first + pair.second;

class Nested { 
    method() {
        fun function() {
            print this;
        }
    function();
    }
}

Nested().method();

class Brunch { eggs() {} }
var brunch = Brunch();
var eggs = brunch.eggs;

class Scone { 
    topping(first, second) {
        print "scone with " + first + " and " + second;
    }
}

var scone = Scone();
scone.topping("berries", "cream");

class CoffeeMaker {
    init(coffee) {
        this.coffee = coffee;
    }
    brew() {
        print "Enjoy your cup of " + this.coffee;
        this.coffee = nil; 
    }
}
var maker = CoffeeMaker("coffee and chicory");
maker.brew();

fun fib(n) {
    if (n < 2) 
        return n;
    return fib(n - 2) + fib(n - 1);
} 

var start = clock();
print fib(7); // was 29 // was 35
print clock() - start;

)""";

int main(int argc, const char * argv[]) {
    initGC();
    initVM();
    interpret(preamble);
    if (true) {
        if (argc == 1) {
            repl();
        } else if (argc == 2) {
            runFile(argv[1]);
        } else {
            fprintf(stderr, "Usage: qet [path]\n");
            exit(64);
        }
    }
    freeVM();
    freeGC();
    return 0;
}
