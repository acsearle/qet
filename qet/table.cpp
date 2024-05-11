//
//  table.cpp
//  qet
//
//  Created by Antony Searle on 22/3/2024.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "object.hpp"
#include "table.hpp"
#include "value.hpp"

#define TABLE_MAX_LOAD 0.75

#define GROW_CAPACITY(capacity) \
((capacity) < 8 ? 8 : capacity * 2)

namespace lox {
    
    void Table::scan(gc::ScanContext& context) const {
        std::unique_lock lock{_mutex};
        for (int index = 0; index != capacity; ++index) {
            Entry* entry = &entries[index];
            if (entry->key) {
                context.push(entry->key);
                entry->value.scan(context);
            }
        }
    }
    
    void initTable(Table* table) {
        table->count = 0;
        table->capacity = 0;
        table->entries = NULL;
    }
    
    void freeTable(Table* table) {
        //reallocate(table->entries, table->capacity * sizeof(Entry), 0);
        operator delete(table->entries, table->capacity * sizeof(Entry));
        initTable(table);
    }
    
    static Entry* findEntry(Entry* entries, int capacity, ObjectString* key) {
        uint32_t index = key->hash & (capacity - 1);
        Entry* tombstone = NULL;
        for (;;) {
            Entry* entry = &entries[index];
            if (entry->key == NULL) {
                if (entry->value.is_nil()) {
                    // Empty entry.
                    return (tombstone != NULL) ? tombstone : entry;
                } else {
                    // We found a tombstone.
                    if (tombstone == NULL) tombstone = entry;
                }
            } else if (entry->key == key) {
                return entry;
            }
            index = (index + 1) & (capacity - 1);
        }
    }
    
    bool tableGet(Table* table, ObjectString* key, Value* value) {
        std::unique_lock lock{table->_mutex};
        if (table->count == 0) return false;
        
        Entry* entry = findEntry(table->entries, table->capacity, key);
        if (entry->key == NULL) return false;
        
        *value = entry->value;
        return true;
    }
    
    bool tableDelete(Table* table, ObjectString* key) {
        std::unique_lock lock{table->_mutex};
        if (table->count == 0) return false;
        
        // Find the entry.
        Entry* entry = findEntry(table->entries, table->capacity, key);
        if (entry->key == NULL) return false;
        
        // Place a tombstone in the entry.
        entry->key = NULL;
        entry->value = Value(true);
        return true;
    }
    
    static void adjustCapacity(Table* table, int capacity) {
        Entry* entries = (Entry*) operator new(sizeof(Entry) * capacity);
        for (int i = 0; i < capacity; i++) {
            entries[i].key = NULL;
            entries[i].value = Value();
        }
        table->count = 0;
        for (int i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            if (entry->key == NULL) continue;
            
            Entry* dest = findEntry(entries, capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
            table->count++;
        }
        
        //reallocate(table->entries, table->capacity * sizeof(Entry), 0);
        operator delete(table->entries, table->capacity * sizeof(Entry));
        table->entries = entries;
        table->capacity = capacity;
    }
    
    bool tableSet(Table* table, ObjectString* key, Value value) {
        std::unique_lock lock{table->_mutex};
        if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
            int capacity = GROW_CAPACITY(table->capacity);
            adjustCapacity(table, capacity);
        }
        Entry* entry = findEntry(table->entries, table->capacity, key);
        bool isNewKey = (entry->key == NULL);
        if (isNewKey && entry->value.is_nil())
            ++(table->count);
        entry->key = key;
        entry->value = value;
        return isNewKey;
    }
    
    void tableAddAll(Table* from, Table* to) {
        std::unique_lock lock{from->_mutex};
        for (int i = 0; i < from->capacity; i++) {
            Entry* entry = &from->entries[i];
            if (entry->key != NULL) {
                tableSet(to, entry->key, entry->value);
            }
        }
    }
    
    ObjectString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
        std::unique_lock lock{table->_mutex};
        if (table->count == 0) return NULL;
        
        /*
        for(int i = 0; i != table->capacity; ++i) {
            Entry* entry = &table->entries[i];
            if (entry->key) {
                entry->key->printObject();
                // printObject(entry->value);
            }
        }
         */
        
        uint32_t index = hash & (table->capacity - 1);
        for (;;) {
            Entry* entry = &table->entries[index];
            if (entry->key == NULL) {
                // Stop if we find an empty non-tombstone entry.
                if (entry->value.is_nil()) {
                    return NULL;
                }
            } else if (entry->key->length == length &&
                       entry->key->hash == hash &&
                       memcmp(entry->key->chars, chars, length) == 0) {
                return entry->key;
            }
            index = (index + 1) & (table->capacity - 1);
        }
    }
    
    /*
    void tableRemoveWhite(Table* table) {
        for (int i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            if (entry->key != NULL && !entry->key->isMarked) {
                 printf("Deleting weak entry (");
                 printValue(Value(entry->key));
                 printf(", ");
                 printValue(entry->value);
                 printf(")\n");
                tableDelete(table, entry->key);
            }
        }
    }

     */
    
    void markTable(const Table* table) {
        std::unique_lock lock{table->_mutex};
        for (int i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            // markObject(entry->key);
            // markValue(entry->value);
            gc::shade(entry->key);            
            gc::shade(entry->value.as_object());
        }
    }
    
    void debugTable(Table* table) {
        std::unique_lock lock{table->_mutex};
        printf("struct Table {\n");
        printf("    int count = %d;\n", table->count);
        printf("    int capacity = %d;\n", table->capacity);
        printf("    Entry* entries = {\n");
        for (int i = 0; i < table->capacity; i++) {
            printf("        [%d] = { ", i);
            Entry* entry = &table->entries[i];
            if (entry->key) {
                printValue(Value(entry->key));
            } else {
                printf("NULL");
            }
            printf(", ");
            printValue(entry->value);
            printf("    },\n");
        }
        printf("}\n");
    }
    
    void printTable(Table* table) {
        std::unique_lock lock{table->_mutex};
        printf("{\n");
        for (int i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            if (entry->key) {
                printf("\"%s\" : ", entry->key->chars);
                printValue(entry->value);
                printf(",\n");
            }
        }
        printf("}\n");
    }
    
} // namespace lox
