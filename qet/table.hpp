//
//  table.hpp
//  qet
//
//  Created by Antony Searle on 22/3/2024.
//

#ifndef table_hpp
#define table_hpp

#include "common.hpp"
#include "value.hpp"

struct Entry {
    ObjString* key;
    Value value;
};

struct Table {
    int count;
    int capacity;
    Entry* entries;
};

void initTable(Table* table);
void freeTable(Table* table);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
void markTable(Table* table);

#endif /* table_hpp */
