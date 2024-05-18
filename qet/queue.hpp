//
//  queue.hpp
//  gch
//
//  Created by Antony Searle on 9/4/2024.
//

#ifndef queue_hpp
#define queue_hpp

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

#include "gc.hpp"

namespace gc {
    
    
    template<typename T>
    struct MichaelScottQueue : Object {
        
        struct Node : Object {
            
            Atomic<StrongPtr<Node>> next;
            T value;
            
            virtual ~Node() override = default;
            
            virtual void _gc_scan(ScanContext& context) const override {
                context.push(this->next);
                // context.push(this->value);
            }
            
        }; // struct Node
        
        Atomic<StrongPtr<Node>> head;
        Atomic<StrongPtr<Node>> tail;
        
        MichaelScottQueue() : MichaelScottQueue(new Node) {} // <-- default constructible T
        MichaelScottQueue(const MichaelScottQueue&) = delete;
        MichaelScottQueue(MichaelScottQueue&&) = delete;
        virtual ~MichaelScottQueue() = default;
        MichaelScottQueue& operator=(const MichaelScottQueue&) = delete;
        MichaelScottQueue& operator=(MichaelScottQueue&&) = delete;
        
        explicit MichaelScottQueue(Node* sentinel)
        : head(sentinel)
        , tail(sentinel) {
        }
        
        virtual void _gc_scan(ScanContext& context) const override {
            context.push(this->head);
        }
        
        void push(T value) {
            // Make new node
            Node* a = new Node;
            assert(a);
            a->value = std::move(value);
            
            // Load the tail
            Node* b = tail.load(std::memory_order::acquire);
            for (;;) {
                assert(b);
                // If tail->next is null, install the new node
                Node* next = nullptr;
                if (b->next.compare_exchange_strong(next, a,
                                                    std::memory_order::release,
                                                    std::memory_order::acquire))
                    return;
                assert(next);
                // tail is lagging, advance it to the next value
                if (tail.compare_exchange_strong(b, next,
                                                 std::memory_order::release,
                                                 std::memory_order::acquire))
                    b = next;
                // Either way, b is now our last observation of tail
            }
        }
        
        bool pop(T& value) {
            Node* expected = head.load(std::memory_order::acquire);
            for (;;) {
                assert(expected);
                Node* next = expected->next.load(std::memory_order::acquire);
                if (next == nullptr)
                    // The queue contains only the sentinel node
                    return false;
                if (head.compare_exchange_strong(expected, next, std::memory_order::release, std::memory_order::acquire)) {
                    // We moved head forward
                    value = std::move(next->value);
                    return true;
                }
                // Else we loaded an unexpected value for head, try again
            }
        }
        
    }; // MichaelScottQueue<T>
    
    
} // namespace gc




#endif /* queue_hpp */
