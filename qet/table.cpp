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
#include "string.hpp"

#define TABLE_MAX_LOAD 0.75

#define GROW_CAPACITY(capacity) \
((capacity) < 8 ? 8 : capacity * 2)

namespace lox {
    
    void Table::scan(gc::ScanContext& context) const {
        // std::unique_lock lock{_mutex};
        /*
        for (int index = 0; index != capacity; ++index) {
            Entry* entry = &entries[index];
            if (entry->key) {
                context.push(entry->key);
                lox::scan(entry->value, context);
            }
        }
         */
        // entries->scan(context);
        context.push((gc::Array<Entry>*) entries);
    }
    
    void initTable(Table* table) {
        std::unique_lock lock{table->_mutex};
        table->count = 0;
        // table->capacity = 0;
        // table->entries = NULL;
        table->entries = nullptr;
    }
    
    void freeTable(Table* table) {
        // reallocate(table->entries, table->capacity * sizeof(Entry), 0);
        // operator delete(table->entries, table->capacity * sizeof(Entry));
        initTable(table);
    }
    
    static Entry* findEntry(Entry* entries, int capacity, ObjectString* key) {
        uint32_t index = key->_hash & (capacity - 1);
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
        
        Entry* entry = findEntry(table->entries->_data, table->capacity(), key);
        if (entry->key == NULL) return false;
        
        *value = entry->value;
        return true;
    }
    
    bool tableDelete(Table* table, ObjectString* key) {
        std::unique_lock lock{table->_mutex};
        if (table->count == 0) return false;
        
        // Find the entry.
        Entry* entry = findEntry(table->entries->_data, table->capacity(), key);
        if (entry->key == NULL) return false;
        
        // Place a tombstone in the entry.
        entry->key = NULL;
        entry->value = Value(true);
        return true;
    }
    
    static void adjustCapacity(Table* table, int capacity) {
        // Entry* entries = (Entry*) operator new(sizeof(Entry) * capacity);
        gc::Array<Entry>* entries = gc::Array<Entry>::make(capacity);
        //for (int i = 0; i < capacity; i++) {
        //    entries->_data[i].key = NULL;
        //    entries->_data[i].value = Value();
        //}
        //table->count = 0;
        for (int i = 0; i < table->capacity(); i++) {
            Entry* entry = &table->entries->_data[i];
            if (entry->key == NULL) continue;
            
            Entry* dest = findEntry(entries->_data, capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
          //  table->count++;
        }
        
        //reallocate(table->entries, table->capacity * sizeof(Entry), 0);
        //operator delete(table->entries, table->capacity * sizeof(Entry));
        table->entries = entries;
        //table->capacity = capacity;
    }
    
    bool tableSet(Table* table, ObjectString* key, Value value) {
        std::unique_lock lock{table->_mutex};
        if (table->count + 1 > table->capacity() * TABLE_MAX_LOAD) {
            int capacity = GROW_CAPACITY(table->capacity());
            adjustCapacity(table, capacity);
        }
        Entry* entry = findEntry(table->entries->_data, table->capacity(), key);
        bool isNewKey = (entry->key == NULL);
        if (isNewKey && entry->value.is_nil())
            ++(table->count);
        entry->key = key;
        entry->value = value;
        return isNewKey;
    }
    
    void tableAddAll(Table* from, Table* to) {
        std::unique_lock lock{from->_mutex};
        for (int i = 0; i < from->capacity(); i++) {
            Entry* entry = &from->entries->_data[i];
            if (entry->key != NULL) {
                tableSet(to, entry->key, entry->value);
            }
        }
    }
    
    /*
    ObjectString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
        std::unique_lock lock{table->_mutex};
        if (table->count == 0) return NULL;
        uint32_t index = hash & (table->capacity - 1);
        for (;;) {
            Entry* entry = &table->entries[index];
            if (entry->key == NULL) {
                // Stop if we find an empty non-tombstone entry.
                if (entry->value.is_nil()) {
                    return NULL;
                }
            } else if (entry->key->_size == length &&
                       entry->key->_hash == hash &&
                       memcmp(entry->key->_data, chars, length) == 0) {
                return entry->key;
            }
            index = (index + 1) & (table->capacity - 1);
        }
    }
     */
    
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
    
    /*
    void markTable(const Table* table) {
        std::unique_lock lock{table->_mutex};
        //int n = table->capacity();
        for (int i = 0; i < n; i++) {
            Entry* entry = &table->entries[i];
            // markObject(entry->key);
            // markValue(entry->value);
            gc::shade(entry->key);            
            gc::shade(entry->value.as_object());
        }
    }
     */
    void scan(const Table& self, gc::ScanContext& context) {
        using lox::scan;
        using gc::scan;
        scan(self.entries, context);
    }
    
    void debugTable(Table* table) {
        std::unique_lock lock{table->_mutex};
        printf("struct Table {\n");
        printf("    int count = %d;\n", table->count);
        printf("    int capacity = %d;\n", table->capacity());
        printf("    Entry* entries = {\n");
        for (int i = 0; i < table->capacity(); i++) {
            printf("        [%d] = { ", i);
            Entry* entry = &table->entries->_data[i];
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
        for (int i = 0; i < table->capacity(); i++) {
            Entry* entry = &table->entries->_data[i];
            if (entry->key) {
                printf("\"%s\" : ", entry->key->_data);
                printValue(entry->value);
                printf(",\n");
            }
        }
        printf("}\n");
    }
    
    
    void Entry::scan(gc::ScanContext& context) const {
        if (key)
            key->scan(context);
        using lox::scan;
        scan(value, context);
    }

    
    int Table::capacity() const {
        auto p = entries.ptr.ptr.load(std::memory_order_acquire);
        return p ? (int) p->_capacity : 0;
    }
    
} // namespace lox
