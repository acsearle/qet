//
//  stack.hpp
//  gch
//
//  Created by Antony Searle on 23/4/2024.
//

#ifndef stack_hpp
#define stack_hpp

#include "gc.hpp"

namespace gc {
    
    template<typename T>
    struct TrieberStack : Object {
        struct Node : Object {
            Atomic<StrongPtr<Node>> next;
            T value;
            virtual ~Node() override = default;
            virtual void _gc_scan(ScanContext& context) const override {
                context.push(this->next);
                context.push(this->value);
            }
        }; // struct Node
        
        Atomic<StrongPtr<Node>> head;
        
        virtual void _gc_scan(ScanContext& context) const override {
            context.push(this->head);
        }
        
        void push(T value) {
            Node* desired = new Node;
            desired->value = std::move(value);
            Node* expected = head.load(std::memory_order::acquire);
            do {
                desired->next.ptr.store(expected, std::memory_order::relaxed);
            } while (!head.compare_exchange_strong(expected,
                                                   desired,
                                                   std::memory_order::release,
                                                   std::memory_order::acquire));
        }
        
        bool pop(T& value) {
            Node* expected = head.load(std::memory_order::acquire);
            for (;;) {
                if (expected == nullptr)
                    return false;
                Node* desired = expected->next.load(std::memory_order::relaxed);
                if (head.compare_exchange_strong(expected,
                                                 desired,
                                                 std::memory_order::relaxed,
                                                 std::memory_order::acquire)) {
                    value = std::move(expected->value);
                    return true;
                }
            }
        }
        
    }; // TrieberStack<T>
}


#endif /* stack_hpp */
