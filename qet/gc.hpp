//
//  gc.hpp
//  gch
//
//  Created by Antony Searle on 10/4/2024.
//

#ifndef gc_hpp
#define gc_hpp

// for pthread_np_*name
#include <pthread/pthread.h>

#include <cstdint>

#include <atomic>
#include <stack>

#include "deque.hpp"

namespace gc {
    
    // TODO:
    //
    // Do we need to be able to 
    
    enum class extra_val_t : std::size_t {};
        
    using Color = std::intptr_t;
    // inline constexpr Color WHITE = 0
    // inline constexpr Color BLACK = 1
    inline constexpr Color GRAY = 2;
    inline constexpr Color RED  = 3;
    
    // WHITE -> not yet reached
    // GRAY  -> reached but must scan fields
    // BLACK -> reached and no further work required
    // RED   -> weak-reachable but not eligible for upgrade to strong
    
    using Order = std::memory_order;
    inline constexpr Order RELAXED = std::memory_order_relaxed;
    inline constexpr Order ACQUIRE = std::memory_order_acquire;
    inline constexpr Order RELEASE = std::memory_order_release;
    inline constexpr Order ACQ_REL = std::memory_order_acq_rel;
    
    struct Object;
    struct Global;
    struct Local;
    struct Channel;
    struct CollectionContext;
    struct ShadeContext;
    struct ScanContext;
    struct SweepContext;
    template<typename> struct StrongPtr;
    template<typename> struct Atomic;
    template<typename> struct Array;
    
    void LOG(const char* format, ...);

    void enter();
    void handshake();
    void leave();
    
    // Mutators shade their roots, and their pointer writes
    //     WHITE -> GRAY
    
    // The collector scans GRAY objects, shading their fields
    //     WHITE -> GRAY
    // and then blackening the object
    //     GRAY  -> BLACK
    
    // The collector sweeps
    //     WHITE -> (free)
    
    // The mutator makes work for the collector by marking objects GRAY
    
    // Scanning only terminates when the collector has found no GRAY objects
    // and all threads have made no GRAY objects
    
    
    
    
    
    
    
    // Leaf nodes have no fields; the mutator may directly them WHITE -> BLACK
    
    // Weak leaf nodes may be upgraded by a mutator
    //     WHITE -> BLACK
    // or they may be condemned by

    
    
    
    
    
   
    
    void shade(const Object* object);
    void shade(const Object* object, ShadeContext& context);

    [[noreturn]] void collect();
    
    
    struct Object {
        
        mutable std::atomic<Color> color;
        
        static void* operator new(std::size_t);
        static void* operator new(std::size_t, extra_val_t);
        
        Object();
        Object(Object const&);
        virtual ~Object() = default;
        Object& operator=(Object const&) = delete;
        Object& operator=(Object&&) = delete;
        
        virtual void _gc_shade(ShadeContext&) const = 0;
        virtual void _gc_scan(ScanContext&) const = 0;
        [[nodiscard]] virtual Color _gc_sweep(SweepContext&);
        virtual std::size_t _gc_bytes() const = 0;
        
        void _gc_shade_as_inner(ShadeContext&) const;
        void _gc_shade_as_leaf(ShadeContext&) const;

        virtual void _gc_shade_weak(ShadeContext& context) const;
        virtual void _gc_scan_weak(ScanContext& context) const;
        
        virtual void debug() const;

    }; // struct Object
    
    
    // TODO: Leaf<Base> : Base ?
    /*
    struct Leaf : Object {
        
        Leaf();
        Leaf(Leaf const&) = default;
        virtual ~Leaf() = default;
        Leaf& operator=(Leaf const&) = delete;
        Leaf& operator=(Leaf&&) = delete;
        
        virtual void _gc_shade(ShadeContext&) const override final;
        virtual void _gc_scan(ScanContext&) const override final;
        
        virtual void debug() const override;
        
    }; // struct Leaf
     */
    
    
    template<typename T>
    struct Atomic<StrongPtr<T>> {
        
        std::atomic<T*> ptr;
        
        Atomic() = default;
        Atomic(Atomic const&) = delete;
        Atomic(Atomic&&) = delete;
        ~Atomic() = default;
        Atomic& operator=(Atomic const&) = delete;
        Atomic& operator=(Atomic&&) = delete;
        
        explicit Atomic(std::nullptr_t);
        explicit Atomic(T*);
        
        void store(T* desired, Order order);
        T* load(Order order) const;
        T* exchange(T* desired, Order order);
        bool compare_exchange_weak(T*& expected, T* desired, Order success, Order failure);
        bool compare_exchange_strong(T*& expected, T* desired, Order success, Order failure);
        
    }; // Atomic<StrongPtr<T>>
    

    template<typename T>
    struct StrongPtr {
        
        Atomic<StrongPtr<T>> ptr;
        
        StrongPtr() = default;
        StrongPtr(StrongPtr const&);
        StrongPtr(StrongPtr&&);
        ~StrongPtr() = default;
        StrongPtr& operator=(StrongPtr const&);
        StrongPtr& operator=(StrongPtr&&);
        
        bool operator==(const StrongPtr&) const;
        
        explicit StrongPtr(std::nullptr_t);
        StrongPtr& operator=(std::nullptr_t);
        bool operator==(std::nullptr_t) const;
        
        explicit StrongPtr(T*);
        StrongPtr& operator=(T*);
        explicit operator T*() const;
        bool operator==(T*) const;
        
        explicit operator bool() const;
        
        T* operator->() const;
        T& operator*() const;
        bool operator!() const;
        
        void scan(ScanContext& context) const;
        
    }; // StrongPtr<T>

    
    template<typename T>
    struct Array final : Object {
        std::size_t _capacity;
        T _data[0];
        static Array* make(std::size_t count);
        virtual ~Array() override;
        virtual void _gc_shade(ShadeContext&) const override;
        virtual void _gc_scan(ScanContext&) const override;
        virtual std::size_t _gc_bytes() const override;
    };

    struct Global {
        
        // public sequential state

        std::mutex mutex;
        std::condition_variable condition_variable;
        
        Color WHITE = 0;
        Color ALLOC = 0;
        
        std::vector<Channel*> entrants;
        deque<Object*> roots;
        
    };
        
    
    struct Channel {
        std::mutex mutex;
        std::condition_variable condition_variable;
        // TODO: make these bit flags
        bool abandoned = false;
        bool pending = false;
        bool dirty = false;
        bool request_infants = false;
        Color WHITE = -1;
        Color ALLOC = -1;
        deque<Object*> infants;
    };

    
    // Thread local state
    struct Local {
        Color WHITE = -1;
        Color BLACK() const { return WHITE^1; }
        Color ALLOC = -1;
        int depth = 0;
        bool dirty = false;
        deque<Object*> allocations;
        deque<Object*> roots;
        Channel* channel = nullptr;
    };
    
    // Context passed to gc operations to avoid, for example, repeated atomic
    // loads from global.white
    
    struct CollectionContext {
        Color WHITE;
        Color BLACK() const { return WHITE^1; }
    };

    struct ShadeContext : CollectionContext {
    };
    
    struct ScanContext : CollectionContext {
        
        void push(Object const*const& field);
        // void push(Leaf const*const& field);

        template<typename T> 
        void push(StrongPtr<T> const& field) {
            push(field.ptr.load(ACQUIRE));
        }
        
        template<typename T> 
        void push(Atomic<StrongPtr<T>> const& field) {
            push(field.load(ACQUIRE));
        }

        std::stack<Object const*, std::vector<Object const*>> _stack;
        
    };
    
    struct SweepContext : CollectionContext {
        
    };
        
    
    
    inline Global global;
    inline thread_local Local local;


    inline void shade(const Object* object, ShadeContext& context) {
        if (object) {
            object->_gc_shade(context);
        }
    }

    inline void shade(const Object* object) {
        if (object) {
            ShadeContext context;
            context.WHITE = local.WHITE;
            object->_gc_shade(context);
        }
    }
    
    
    inline void* Object::operator new(std::size_t count) {
        return ::operator new(count);
    }
    
    inline void* Object::operator new(std::size_t count, extra_val_t extra) {
        return operator new(count + static_cast<std::size_t>(extra));
    }
    
    inline Object::Object()
    : color(local.ALLOC) {
        assert(local.depth); // <-- catch allocations that are not inside a mutator state
        local.allocations.push_back(this);
    }
    
    inline Object::Object(const Object& other)
    : color(local.ALLOC) {
        assert(local.depth); // <-- catch allocations that are not inside a mutator state
        local.allocations.push_back(this);
    }
    
    inline void Object::_gc_shade_as_inner(ShadeContext& context) const {
        Color expected = context.WHITE;
        if (color.compare_exchange_strong(expected,
                                          GRAY,
                                          RELAXED,
                                          RELAXED)) {
            local.dirty = true;
        }
    }
    
    inline void Object::_gc_shade_as_leaf(ShadeContext& context) const {
        Color expected = context.WHITE;
        color.compare_exchange_strong(expected,
                                      context.BLACK(),
                                      RELAXED,
                                      RELAXED);
    }
    
    inline void Object::_gc_scan(ScanContext& context) const {
        // no-op
    }

    inline Color Object::_gc_sweep(SweepContext& context) {
        Color color = this->color.load(RELAXED);
        if (color == context.WHITE) {
            delete this;
        }
        return color;
    }
    
    inline void Object::_gc_shade_weak(ShadeContext& context) const {
        this->_gc_shade(context);
    }
    inline void Object::_gc_scan_weak(ScanContext& context) const {
        context.push(this);
    }
    
    /*
    inline Leaf::Leaf() : Object() {
    }
    
    inline void Leaf::_gc_shade(ShadeContext& context) const {
        Color expected = context.WHITE;
        color.compare_exchange_strong(expected,
                                      context.BLACK(),
                                      RELAXED,
                                      RELAXED);
    }
    
    inline void Leaf::_gc_scan(ScanContext& context) const {
        // no-op
    }
     */
        
    template<typename T>
    Atomic<StrongPtr<T>>::Atomic(T* desired)
    : ptr(desired) {
        shade(desired);
    }
    
    template<typename T>
    T* Atomic<StrongPtr<T>>::load(Order order) const {
        return ptr.load(order);
    }
    
    template<typename T> 
    void Atomic<StrongPtr<T>>::store(T* desired, Order order) {
        shade(desired);
        T* old = ptr.exchange(desired, order);
        shade(old);
    }

    template<typename T>
    T* Atomic<StrongPtr<T>>::exchange(T* desired, Order order) {
        shade(desired);
        T* old = ptr.exchange(desired, order);
        shade(old);
        return old;
    }

    template<typename T>
    bool Atomic<StrongPtr<T>>::compare_exchange_strong(T*& expected,
                                          T* desired,
                                          Order success,
                                          Order failure) {
        return (ptr.compare_exchange_strong(expected, desired, success, failure)
                && (shade(expected), shade(desired), true));
    }
    
    template<typename T>
    bool Atomic<StrongPtr<T>>::compare_exchange_weak(T*& expected,
                                          T* desired,
                                          Order success,
                                          Order failure) {
        return (ptr.compare_exchange_weak(expected, desired, success, failure)
                && (shade(expected), shade(desired), true));
    }

    
    
    template<typename T>
    StrongPtr<T>::StrongPtr(StrongPtr const& other)
    : ptr(other.ptr.load(RELAXED)) {
    }

    template<typename T>
    StrongPtr<T>::StrongPtr(StrongPtr&& other)
    : ptr(other.ptr.load(RELAXED)) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(StrongPtr<T> const& other) {
        ptr.store(other.ptr.load(RELAXED), RELEASE);
        return *this;
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(StrongPtr<T>&& other) {
        ptr.store(other.ptr.load(RELAXED), RELEASE);
        return *this;
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(const StrongPtr& other) const {
        return ptr.load(RELAXED) == other.ptr.load(RELAXED);
    }
    
    template<typename T>
    StrongPtr<T>::StrongPtr(std::nullptr_t)
    : ptr(nullptr) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(std::nullptr_t) {
        ptr.store(nullptr, RELEASE);
        return *this;
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(std::nullptr_t) const {
        return ptr.load(RELAXED) == nullptr;
    }
    
    template<typename T>
    StrongPtr<T>::StrongPtr(T* object)
    : ptr(object) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(T* other) {
        ptr.store(other, RELEASE);
        return *this;
    }
    
    template<typename T>
    StrongPtr<T>::operator T*() const {
        return ptr.load(RELAXED);
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(T* other) const {
        return ptr.load(RELAXED) == other;
    }
    
    template<typename T>
    StrongPtr<T>::operator bool() const {
        return ptr.load(RELAXED);
    }
    
    template<typename T>
    T* StrongPtr<T>::operator->() const {
        T* a = ptr.load(RELAXED);
        assert(a);
        return a;
    }

    template<typename T>
    T& StrongPtr<T>::operator*() const {
        T* a = ptr.load(RELAXED);
        assert(a);
        return *a;
    }

    template<typename T>
    bool StrongPtr<T>::operator!() const {
        return !ptr.load(RELAXED);
    }
    
    // WHITE OBJECT -> BLACK PUSH - we process it later
    // GRAY OBJECT -> NOOP - we find it later in worklist
    // BLACK OBJECT -> no need to schedule it
    // WHITE LEAF -> BLACK NOPUSH - no need to schedule
    // GRAY LEAF -> impossible?
    
    inline void ScanContext::push(Object const* const& object) {
        Color expected = WHITE;
        if (object &&
            object->color.compare_exchange_strong(expected,
                                                  BLACK(),
                                                  RELAXED,
                                                  RELAXED)) {
            _stack.push(object);
        }
    }

    /*
    inline void ScanContext::push(Leaf const* const& object) {
        Color expected = WHITE;
        if (object) {
            object->color.compare_exchange_strong(expected,
                                                  BLACK(),
                                                  RELAXED,
                                                  RELAXED);
            // Leaf nodes are never pushed as they never produce more work
            // TODO: this relies on compile-time knowledge, and in turn we
            // can't rely on actual leaf nodes never being pushed
        }
    }
     */
    
    inline void scan(Object* object, ScanContext& context) {
        if (object)
            context.push(object);
    }
    
    template<typename T>
    Array<T>::~Array<T>() {
        std::destroy_n(_data, _capacity);
    }
    
    template<typename T>
    Array<T>* Array<T>::make(std::size_t n) {
        Array<T>* p = new(extra_val_t{n * sizeof(T)}) Array<T>;
        p->_capacity = n;
        std::uninitialized_value_construct_n(p->_data, p->_capacity);
        return p;
    }

    template<typename T>
    void Array<T>::_gc_shade(ShadeContext& context) const {
        this->_gc_shade_as_inner(context);
    }

    template<typename T>
    void Array<T>::_gc_scan(ScanContext& context) const {
        LOG("Array scan");
        for (std::size_t i = 0; i != _capacity; ++i)
            _data[i].scan(context);
    }
    
    template<typename T>
    std::size_t Array<T>::_gc_bytes() const {
        return sizeof(Array<T>) + sizeof(T) * _capacity;
    }


    /*
    template<typename T>
    void scan(const StrongPtr<T>& self, ScanContext& context) {
        LOG("StrongPtr scan");
        context.push(self.ptr.ptr.load(std::memory_order_acquire));
    }
     */
    
    template<typename T>
    void StrongPtr<T>::scan(ScanContext& context) const {
        context.push(ptr.ptr.load(std::memory_order_acquire));
    }


} // namespace gc



/*
namespace gc2 {
    
    struct Allocation {
        std::atomic<std::intptr_t> color;
    };
    
    struct Class {
    };
    
    struct Object {
        Class* _class;
        unsigned char _data[8];
    };
    
    
    template<typename PromiseType>
    struct Coroutine {
        void (*_resume)();
        void (*_destroy)();
        PromiseType promise;
        
        
    };
    
};
 */

#endif /* gc_hpp */
