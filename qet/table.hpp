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

namespace lox {
    
    struct ObjectString;
    
    struct Entry {
        ObjectString* key;
        Value value;
    };
    
    struct Table {
        int count;
        int capacity;
        Entry* entries;
        
        void scan(gc::ScanContext& context) const;
        
    };
    
    void initTable(Table* table);
    void freeTable(Table* table);
    bool tableSet(Table* table, ObjectString* key, Value value);
    bool tableGet(Table* table, ObjectString* key, Value* value);
    bool tableDelete(Table* table, ObjectString* key);
    void tableAddAll(Table* from, Table* to);
    ObjectString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
    void tableRemoveWhite(Table* table);
    void markTable(const Table* table);
    void printTable(Table* table);
    
} // namespace lox

#endif /* table_hpp */
