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
((capacity) < 8 ? 8 : (capacity) * 2)

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
        // std::unique_lock lock{table->_mutex};
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
    
    static Entry* findEntry(gc::Array<Entry>* v, ObjectString* key) {
        Entry* entries = v->_data;
        int capacity = (int) v->_capacity;
        uint32_t index = key->_hash & (capacity - 1);
        Entry* tombstone = NULL;
        for (;;) {
            Entry* entry = &entries[index];
            if (entry->key == nullptr) {
                if (entry->value.load().is_nil()) {
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
        // std::unique_lock lock{table->_mutex};
        if (table->count == 0) return false;
        
        Entry* entry = findEntry(table->entries.inner.load(std::memory_order_relaxed), key);
        if (entry->key == nullptr) return false;
        
        *value = entry->value.load();
        return true;
    }
    
    bool tableDelete(Table* table, ObjectString* key) {
        // std::unique_lock lock{table->_mutex};
        if (table->count == 0) return false;
        
        // Find the entry.
        Entry* entry = findEntry(table->entries.inner.load(std::memory_order_relaxed), key);
        if (entry->key == nullptr) return false;
        
        // Place a tombstone in the entry.
        entry->key = nullptr;
        entry->value = Value(true);
        return true;
    }
    
    static void adjustCapacity(Table* table, int capacity) {
        // Entry* entries = (Entry*) operator new(sizeof(Entry) * capacity);
        gc::Array<Entry>* new_entries = gc::Array<Entry>::make(capacity);
        for (int i = 0; i < capacity; i++) {
            new_entries->_data[i].key = nullptr;
            new_entries->_data[i].value = Value();
        }
        table->count = 0;
        gc::Array<Entry>* old_entries = table->entries.inner.load(std::memory_order_relaxed);
        if (old_entries) {
            for (int i = 0; i < old_entries->_capacity; i++) {
                Entry* entry = &old_entries->_data[i];
                if (entry->key == nullptr)
                    continue;                
                Entry* dest = findEntry(new_entries, (ObjectString*) entry->key);
                dest->key = entry->key;
                dest->value = entry->value;
                table->count++;
            }
        }
        
        //reallocate(table->entries, table->capacity * sizeof(Entry), 0);
        //operator delete(table->entries, table->capacity * sizeof(Entry));
        table->entries = new_entries;
        //table->capacity = capacity;
    }
    
    bool tableSet(Table* table, ObjectString* key, Value value) {
        // std::unique_lock lock{table->_mutex};
        gc::Array<Entry>* v = table->entries.inner.load(std::memory_order_relaxed);
        if ((v == nullptr) || ((table->count + 1) > (v->_capacity * TABLE_MAX_LOAD))) {
            int new_capacity = GROW_CAPACITY(v ? (int) v->_capacity : 0);
            adjustCapacity(table, new_capacity);
            v = table->entries.inner.load(std::memory_order_relaxed);
        }
        Entry* entry = findEntry(v, key);
        bool isNewKey = (entry->key == nullptr);
        if (isNewKey && entry->value.load().is_nil())
            ++(table->count);
        entry->key = key;
        entry->value = value;
        return isNewKey;
    }
    
    void tableAddAll(Table* from, Table* to) {
        // std::unique_lock lock{from->_mutex};
        gc::Array<Entry>* from_v = from->entries.inner.load(std::memory_order_relaxed);
        if (from_v) {
            for (int i = 0; i < from_v->_capacity; i++) {
                Entry* entry = &from_v->_data[i];
                if (entry->key != nullptr) {
                    tableSet(to, (ObjectString*) entry->key, entry->value.load());
                }
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
        gc::LOG("Table scan");
        context.push(self.entries);
    }
    
    void debugTable(Table* table) {
        // std::unique_lock lock{table->_mutex};
        printf("struct Table {\n");
        printf("    int count = %d;\n", table->count);
        gc::Array<Entry>* v = table->entries.inner.load(std::memory_order_acquire);
        printf("    int capacity = %d;\n", (int) v->_capacity);
        printf("    Entry* entries = {\n");
        for (int i = 0; i < v->_capacity; i++) {
            printf("        [%d] = { ", i);
            Entry* entry = &v->_data[i];
            if (entry->key) {
                printValue(Value((ObjectString*) entry->key));
            } else {
                printf("NULL");
            }
            printf(", ");
            printValue(entry->value.load());
            printf("    },\n");
        }
        printf("}\n");
    }
    
    void printTable(Table* table) {
        // std::unique_lock lock{table->_mutex};
        printf("{\n");
        gc::Array<Entry>* v = table->entries.inner.load(std::memory_order_acquire);
        for (int i = 0; i < v->_capacity; i++) {
            Entry* entry = &v->_data[i];
            if (entry->key) {
                printf("\"%s\" : ", entry->key->_data);
                printValue(entry->value.load());
                printf(",\n");
            }
        }
        printf("}\n");
    }
    
    
    void Entry::scan(gc::ScanContext& context) const {
        context.push(key);
        lox::scan(value, context);
    }

    
    /*
    int Table::capacity() const {
        auto p = entries.ptr.ptr.load(std::memory_order_acquire);
        return p ? (int) p->_capacity : 0;
    }
     */
    
} // namespace lox
