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

    // base of all garbage collection participants
    struct Object;
    
    // base adapter for nodes without gc fields
    template<typename> struct Leaf;
    
    // nonresizeable gc array, as in T[]
    template<typename> struct Array;
    
    // type for gc pointer fields of gc objects
    // provides write barrier and atomic access for collector
    // provides correct defaults for a field written by one mutator thread and
    // simultaneously (only) read by the collector thread
    template<typename> struct StrongPtr;
    
    // Atomic<StrongPtr> provides the finer control over memory ordering
    // required when the pointer is written by multiple mutator threads
    template<typename> struct Atomic;

    // workloop for the collector
    [[noreturn]] void collect();

    namespace this_thread {
        
        // indicates a thread will participate in garbage collection
        // - the thread may allocate gc::objects
        // - the thread must periodically call handshake
        // - the thread must mark any roots between handshakes
        void enter();
        
        // indicates a thread will no longer participate in garbage collection
        // - the thread cannot allocate
        // - the thread will not handshake
        // - the thread will not mark roots
        void leave();
        // if the thread "holds" gc objects across a suspension, they must be
        // scanned by some other mechanism, such as handing them over to
        // another thread
        
        // called periodically
        // - synchronizes state with collector
        //   - allocations since last handshake
        //   - what colors to shade and alloc
        //   - dirty flag (did we shade any objects WHITE -> GRAY since last handshake?)
        void handshake();
        
    }

    // internal garbage collection interface

    struct Channel;
    struct CollectionContext;
    struct Global;
    struct Local;
    struct ScanContext;
    struct ShadeContext;
    struct SweepContext;

    void* alloc(std::size_t count);
    void free(Object*); // <-- delete is convenient...
    void LOG(const char* format, ...);
    
    // must shade involved objects whenever mutating the graph
    // - StrongPtr automates this for common use cases
    void shade(const Object* object);
    void shade(const Object* object, ShadeContext& context);

    enum class Color : std::intptr_t {
        // WHITE = 0 or 1, not (yet) reached
        // BLACK = 1 or 0, reached and (will be) scanned
        GRAY = 2, //       reached but not yet scanned
        RED  = 3, //       dying but still weak-reachable
    };
    
    const char* ColorCString(Color); // thread local interpretation of values
    

    
    
    
    
    struct Object {
        
    protected:
        
        mutable std::atomic<Color> color;
                
        Object();
        Object(Object const&);
        Object& operator=(Object const&) = delete;
        Object& operator=(Object&&) = delete;
        
        // commonly overridden methods
        virtual ~Object() = default;
        virtual void _gc_debug() const;
        virtual void _gc_scan(ScanContext&) const = 0;
        virtual std::size_t _gc_bytes() const = 0;

        // rarely overridden methods
        virtual void _gc_shade(ShadeContext&) const;
        virtual void _gc_scan_into(ScanContext&) const;
        [[nodiscard]] virtual Color _gc_sweep(SweepContext&);
        virtual void _gc_shade_weak(ShadeContext& context) const;
        virtual void _gc_scan_weak(ScanContext& context) const;
        
        // friends that access protected methods
        friend struct ScanContext;
        friend void shade(const Object*, ShadeContext&);
        friend void collect();

    }; // struct Object
    

    // Provide implementations for leaf objects (which have no GC fields)
        
    template<typename  T>
    struct Leaf : T {
        
        static_assert(std::is_convertible_v<T*, Object*>);
        
    protected:
        
        virtual ~Leaf() override = default;
        virtual void _gc_shade(ShadeContext&) const override final;
        virtual void _gc_scan(ScanContext&) const override final;
        virtual void _gc_scan_into(ScanContext&) const override final;
        
    };

        
    // Array provides a dynamic but non-resizable array of T, the GC equivalent
    // of "new T[count]"
    //
    // This is slightly clumsy in several respects and more for internal use;
    // compare ObjectTable and ObjectArray?
    //
    // TODO: should this be an adapter like Leaf<T> ?
    //
    // TODO: generic implementation of _gc_scan means we need some kind of
    // free function ADL scan of each element, with no-op default?
    
    template<typename T>
    struct Array final : Object {

        std::size_t _capacity;
        T _data[0]; // <-- flexible array member

        static Array* make(std::size_t count);

        virtual ~Array() override;
        virtual void _gc_debug() const override;
        virtual void _gc_scan(ScanContext&) const override;
        virtual std::size_t _gc_bytes() const override;
                
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        
        reference operator[](size_type pos) { return _data[pos]; }
        const_reference operator[](size_type pos) const { return _data[pos]; }
        
        reference front() { return _data[0]; }
        const_reference front() const { return _data[0]; }
        
        reference back() { return _data[_capacity - 1]; }
        const_reference back() const { return _data[_capacity - 1]; }
        
        pointer data() { return _data; }
        const_pointer data() const { return _data; }
        
        iterator begin() { return _data; }
        const_iterator begin() const { return _data; }
        const_iterator cbegin() const { return _data; }

        iterator end() { return _data + _capacity; }
        const_iterator end() const { return _data + _capacity; }
        const_iterator cend() const { return _data + _capacity; }
        
        size_type size() const { return _capacity; }

    };
        
    

    template<typename T>
    struct Atomic<StrongPtr<T>> {
        
        std::atomic<T*> inner;
        
        Atomic() = default;
        Atomic(Atomic const&) = delete;
        Atomic(Atomic&&) = delete;
        ~Atomic() = default;
        Atomic& operator=(Atomic const&) = delete;
        Atomic& operator=(Atomic&&) = delete;
        
        explicit Atomic(std::nullptr_t);
        explicit Atomic(T*);
        
        void store(T* desired, std::memory_order order);
        T* load(std::memory_order order) const;
        T* exchange(T* desired, std::memory_order order);
        bool compare_exchange_weak(T*& expected, T* desired, std::memory_order success, std::memory_order failure);
        bool compare_exchange_strong(T*& expected, T* desired, std::memory_order success, std::memory_order failure);
        
    }; // Atomic<StrongPtr<T>>
    

    template<typename T>
    struct StrongPtr {
        
        Atomic<StrongPtr<T>> inner;
        
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
        
        T* get() const;
                
    }; // StrongPtr<T>



    struct Global {
        
        // public sequential state

        std::mutex mutex;
        std::condition_variable condition_variable;
        
        Color WHITE = Color{0};
        Color ALLOC = Color{0};
        
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
        Color WHITE = Color{-1};
        Color ALLOC = Color{-1};
        deque<Object*> infants;
    };

    
    // Thread local state
    struct Local {
        Color WHITE = Color{-1};
        Color BLACK() const { return Color{static_cast<std::intptr_t>(WHITE)^1}; }
        Color ALLOC = Color{-1};
        int depth = 0;
        bool dirty = false;
        deque<Object*> allocations;
        deque<Object*> roots;
        Channel* channel = nullptr;
        std::int64_t bytes_allocated;
        std::int64_t bytes_freed;
    };
    
    // Context passed to gc operations to avoid, for example, repeated atomic
    // loads from global.white
    
    struct CollectionContext {
        Color WHITE;
        Color BLACK() const { return Color{static_cast<std::intptr_t>(WHITE)^1}; }
    };

    struct ShadeContext : CollectionContext {
    };
    
    struct ScanContext : CollectionContext {
        
        void push(const Object*const& field) {
            if (field) {
                field->_gc_scan_into(*this);
            }
        }

        template<typename T> 
        void push(const StrongPtr<T>& field) {
            push(field.inner.load(std::memory_order::acquire));
        }
        
        template<typename T> 
        void push(const Atomic<StrongPtr<T>>& field) {
            push(field.load(std::memory_order::acquire));
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
        ShadeContext context;
        context.WHITE = local.WHITE;
        shade(object, context);
    }
    
    // TODO: pass in some AllocContext explicitly?
    
    inline Object::Object() 
    : color(local.ALLOC) {
        assert(local.depth); // <-- catch allocations that are not inside a mutator state
        local.allocations.push_back(this);
    }
    
    inline Object::Object(const Object& other) : Object() {}

    inline void Object::_gc_shade(ShadeContext& context) const {
        Color expected = context.WHITE;
        if (color.compare_exchange_strong(expected,
                                          Color::GRAY,
                                          std::memory_order::relaxed,
                                          std::memory_order::relaxed)) {
            // TODO: make this part of ShadeContext to avoid TLS lookup?
            local.dirty = true;
        }
    }
    
    inline void Object::_gc_scan_into(ScanContext& context) const {
        Color expected = context.WHITE;
        if (color.compare_exchange_strong(expected,
                                          context.BLACK(),
                                          std::memory_order::relaxed,
                                          std::memory_order::relaxed)) {
            // WHITE -> BLACK; schedule for scanning
            context._stack.push(this);
        } else {
            assert(expected == context.BLACK() || expected == Color::GRAY);
        }
    }

    inline Color Object::_gc_sweep(SweepContext& context) {
        Color color = this->color.load(std::memory_order::relaxed);
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
    
    // Leaf<T> provides efficient implementation for no GC fields case
          
    template<typename T>
    void Leaf<T>::_gc_shade(ShadeContext& context) const {
        Color expected = context.WHITE;
        this->color.compare_exchange_strong(expected,
                                            context.BLACK(),
                                            std::memory_order::relaxed,
                                            std::memory_order::relaxed);
    }

    template<typename T>
    void Leaf<T>::_gc_scan(ScanContext& context) const {
    }
    
    template<typename T>
    void Leaf<T>::_gc_scan_into(ScanContext& context) const {
        Color expected = context.WHITE;
        this->color.compare_exchange_strong(expected,
                                            context.BLACK(),
                                            std::memory_order::relaxed,
                                            std::memory_order::relaxed);
    }

        
    template<typename T>
    Atomic<StrongPtr<T>>::Atomic(T* desired)
    : inner(desired) {
        shade(desired);
    }
    
    template<typename T>
    T* Atomic<StrongPtr<T>>::load(std::memory_order order) const {
        return inner.load(order);
    }
    
    template<typename T> 
    void Atomic<StrongPtr<T>>::store(T* desired, std::memory_order order) {
        shade(desired);
        T* old = inner.exchange(desired, order);
        shade(old);
    }

    template<typename T>
    T* Atomic<StrongPtr<T>>::exchange(T* desired, std::memory_order order) {
        shade(desired);
        T* old = inner.exchange(desired, order);
        shade(old);
        return old;
    }

    template<typename T>
    bool Atomic<StrongPtr<T>>::compare_exchange_strong(T*& expected,
                                                       T* desired,
                                                       std::memory_order success,
                                                       std::memory_order failure) {
        return (inner.compare_exchange_strong(expected, desired, success, failure)
                && (shade(expected), shade(desired), true));
    }
    
    template<typename T>
    bool Atomic<StrongPtr<T>>::compare_exchange_weak(T*& expected,
                                                     T* desired,
                                                     std::memory_order success,
                                                     std::memory_order failure) {
        return (inner.compare_exchange_weak(expected, desired, success, failure)
                && (shade(expected), shade(desired), true));
    }

    
    
    template<typename T>
    StrongPtr<T>::StrongPtr(StrongPtr const& other)
    : inner(other.ptr.load(std::memory_order::relaxed)) {
    }

    template<typename T>
    StrongPtr<T>::StrongPtr(StrongPtr&& other)
    : inner(other.ptr.load(std::memory_order::relaxed)) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(StrongPtr<T> const& other) {
        inner.store(other.inner.load(std::memory_order::relaxed), std::memory_order::release);
        return *this;
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(StrongPtr<T>&& other) {
        inner.store(other.inner.load(std::memory_order::relaxed), std::memory_order::release);
        return *this;
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(const StrongPtr& other) const {
        return inner.load(std::memory_order::relaxed) == other.ptr.load(std::memory_order::relaxed);
    }
    
    template<typename T>
    StrongPtr<T>::StrongPtr(std::nullptr_t)
    : inner(nullptr) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(std::nullptr_t) {
        inner.store(nullptr, std::memory_order::relaxed);
        return *this;
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(std::nullptr_t) const {
        return inner.load(std::memory_order::relaxed) == nullptr;
    }
    
    template<typename T>
    StrongPtr<T>::StrongPtr(T* object)
    : inner(object) {
    }
    
    template<typename T>
    StrongPtr<T>& StrongPtr<T>::operator=(T* other) {
        inner.store(other, std::memory_order::release);
        return *this;
    }
    
    template<typename T>
    StrongPtr<T>::operator T*() const {
        return inner.load(std::memory_order::relaxed);
    }
    
    template<typename T>
    bool StrongPtr<T>::operator==(T* other) const {
        return inner.load(std::memory_order::relaxed) == other;
    }
    
    template<typename T>
    StrongPtr<T>::operator bool() const {
        return inner.load(std::memory_order::relaxed);
    }
    
    template<typename T>
    T* StrongPtr<T>::operator->() const {
        T* a = inner.load(std::memory_order::relaxed);
        assert(a);
        return a;
    }

    template<typename T>
    T& StrongPtr<T>::operator*() const {
        T* a = inner.load(std::memory_order::relaxed);
        assert(a);
        return *a;
    }

    template<typename T>
    bool StrongPtr<T>::operator!() const {
        return !inner.load(std::memory_order::relaxed);
    }
    
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
        Array<T>* p = new (alloc(sizeof(Array<T>) + sizeof(T) * n)) Array<T>;
        p->_capacity = n;
        std::uninitialized_value_construct_n(p->_data, p->_capacity);
        return p;
    }

    template<typename T>
    void Array<T>::_gc_scan(ScanContext& context) const {
        for (const T& x : *this)
            x.scan(context);
    }
    
    template<typename T>
    std::size_t Array<T>::_gc_bytes() const {
        return sizeof(Array<T>) + sizeof(T) * _capacity;
    }

    template<typename T>
    void Array<T>::_gc_debug() const {
        printf("%p %s Array<T>{%zu, ...}\n", this, gc::ColorCString(color.load(std::memory_order::relaxed)), _capacity);
    }

    template<typename T>
    void scan(const std::vector<T>&, gc::ScanContext& context);

} // namespace gc

namespace gc {
    
    inline void* alloc(std::size_t count) {
        local.bytes_allocated += count;
        return malloc(count);
    }
    
    template<typename T>
    void scan(const std::vector<T>& v, gc::ScanContext& context) {
        for (auto&& x : v)
            scan(x, context);
    }
    
    inline void Object::_gc_debug() const {
        printf("%p %s %s\n",
               this,
               ColorCString(color.load(std::memory_order_relaxed)),
               typeid(*this).name());
    }
    
} // namespace gc


#endif /* gc_hpp */
