//
//  table.hpp
//  qet
//
//  Created by Antony Searle on 22/3/2024.
//

#ifndef table_hpp
#define table_hpp

#include "common.hpp"
#include "gc.hpp"
#include "value.hpp"

namespace gc {
    namespace _string {
        struct SNode;
    }
}

namespace lox {
    
    // TODO:
    //
    // Relative performance of virtual functions vs switch-on-enum
    //
    // Using virtual functins for prototyping, contra clox
    
    struct VM;
    
    struct Object;
    
    
    using ObjectString = ::gc::_string::SNode;
}

namespace lox {
    
    // struct ObjectString;
    
    struct Entry {
        gc::StrongPtr<ObjectString> key;
        AtomicValue value;
        
        void scan(gc::ScanContext& context) const;
        
    };
    
    // void scan(const Entry&, gc::ScanContext&);
    
    struct Table {
        int count;
        // int capacity;
        // Entry* entries;
        gc::StrongPtr<gc::Array<Entry>> entries;
        void scan(gc::ScanContext& context) const;        
    };
    
    void initTable(Table* table);
    void freeTable(Table* table);
    bool tableSet(Table* table, ObjectString* key, Value value);
    bool tableGet(Table* table, ObjectString* key, Value* value);
    bool tableDelete(Table* table, ObjectString* key);
    void tableAddAll(Table* from, Table* to);
    // ObjectString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
    // void tableRemoveWhite(Table* table);
    // void markTable(const Table* table);
    void printTable(Table* table);
    
    // void scan(const Table&, gc::ScanContext&);
    
} // namespace lox

#endif /* table_hpp */
