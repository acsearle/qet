//
//  gc.cpp
//  gch
//
//  Created by Antony Searle on 10/4/2024.
//

#include <thread>
#include "gc.hpp"
#include "vm.hpp"

namespace gc {
    
    void Object::debug() const {
        printf("%p gc::Object\n", this);
    }

    void Leaf::debug() const {
        printf("%p gc::Leaf\n", this);
    }

    
    void LOG(const char* format, ...) {
#ifdef LOX_DEBUG_LOG_GC
        char buffer[256];
        pthread_getname_np(pthread_self(), buffer + 240, 16);
        const char* dirty = local.dirty ? "dirty" : "clean";
        int n = snprintf(buffer, 240, "%s/%s: ", buffer + 240, dirty);
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + n, 256 - n, format, args);
        va_end(args);
        puts(buffer);
#endif
    }
    
    struct ScanContextPrivate : ScanContext {
        
        void process() {
            while (!_stack.empty()) {
                Object const* object = _stack.top();
                _stack.pop();
                assert(object && object->color.load(RELAXED) == (BLACK()));
                object->scan(*this);
            }
        }
        
    };
    
    void enter() {

        assert(local.depth >= 0);
        if (local.depth++) {
            return;
        }
                
        // Create a new communication channel to the collector
        
        assert(local.channel == nullptr);
        Channel* channel = local.channel = new Channel;
        LOG("enters collectible state");
        {
            // Publish it to the collector's list of channels
            std::unique_lock lock{global.mutex};
            // channel->next = global.channels;
            global.entrants.push_back(channel);
            channel->WHITE = local.WHITE = global.WHITE;
            channel->ALLOC = local.ALLOC = global.ALLOC;
        }
        // Wake up the mutator
        global.condition_variable.notify_all();
    }
    
    void leave() {
        
        assert(local.depth >= 0);
        if (--local.depth) {
            return;
        }
        
        // Look up the communication channel
        
        Channel* channel = local.channel;
        assert(channel);
        bool pending;
        {
            std::unique_lock lock(channel->mutex);
            LOG("leaves collectible state", channel);
            // Was there a handshake requested?
            pending = std::exchange(channel->pending, false);
            channel->abandoned = true;
            channel->dirty = local.dirty;
            LOG("%spublishes %s, orphans", pending ? "handshakes, " : "", local.dirty ? "dirty" : "clean");
            local.dirty = false;
            if (channel->infants.empty()) {
                channel->infants.swap(local.allocations);
                assert(local.allocations.empty());
            } else {
                // we are leaving after we have acknowledged a handshake, but
                // before the collector has taken our infants
                while (!local.allocations.empty()) {
                    Object* p = local.allocations.front();
                    local.allocations.pop_front();
                    channel->infants.push_back(p);
                }
            }
            channel->request_infants = false;
        }
        // wake up the collector if it was already waiting on a handshake
        if (pending) {
            LOG("notifies collector");
            channel->condition_variable.notify_all();
        }
        local.channel = nullptr;
        
    }
    
    
    void handshake() {
        Channel* channel = local.channel;
        bool pending;
        {
            std::unique_lock lock(channel->mutex);
            pending = channel->pending;
            if (pending) {
                LOG("handshaking");
                
                // LOG("lifetime alloc %zu", allocated);
                
                // LOG("was WHITE=%lld BLACK=%lld ALLOC=%lld", local.WHITE, local.BLACK, local.ALLOC);
                
                //bool flipped_ALLOC = local.ALLOC != channels[index].configuration.ALLOC;
                
                // (Configuration&) local = channels[index].configuration;
                
                // LOG("becomes WHITE=%lld BLACK=%lld ALLOC=%lld", local.WHITE, local.BLACK, local.ALLOC);
                
                channel->dirty = local.dirty;
                LOG("publishing %s", local.dirty ? "dirty" : "clean");
                local.dirty = false;
                
                local.WHITE = channel->WHITE;
                local.ALLOC = channel->ALLOC;

                if (channel->request_infants) {
                    LOG("publishing ?? new allocations");
                    assert(channel->infants.empty());
                    channel->infants.swap(local.allocations);
                    assert(local.allocations.empty());
                }
                channel->request_infants = false;
                
                channel->pending = false;
                
            } else {
                // LOG("handshake not requested");
            }
        }
        
        if (pending) {
            LOG("notifies collector");
            channel->condition_variable.notify_all();
            std::size_t count = 0;
            for (Object* ref : local.roots) {
                ++count;
                shade(ref);
            }
            LOG("shades %ld roots", count);
        }
    }
    
    
    void collect() {
        
        pthread_setname_np("C0");
        
        gc::enter();
        
        size_t freed = 0;
        
        deque<Object*> objects;
        deque<Object*> infants;
        deque<Object*> redlist;
        deque<Object*> blacklist;
        deque<Object*> whitelist;
        
        ScanContextPrivate working;

        std::vector<Channel*> mutators, mutators2;
                
        auto accept_entrants = [&]() {
            assert(mutators2.empty());
            std::unique_lock lock{global.mutex};
            for (;;) {
                mutators.insert(mutators.end(),
                                global.entrants.begin(),
                                global.entrants.end());
                global.entrants.clear();
                LOG("mutators.size() -> %zu", mutators.size());
                return;
                // LOG("objects.size() -> %zu", objects.size());
                if (mutators.size() > 1)
                    return;
                LOG("no mutators; waiting for new entrant");
                LOG("lifetime freed %zu", freed);
                global.condition_variable.wait(lock);
            }
        };
                
        for (;;) {
            
            LOG("collection begins");

            LOG("begin transition to allocating BLACK");

            local.ALLOC = local.BLACK();
            working.WHITE = local.WHITE;
            {
                std::unique_lock lock{global.mutex};
                global.WHITE = local.WHITE;
                global.ALLOC = local.ALLOC;
            }

            // New allocations are WHITE
            // The write barrier is turning WHITE objects GRAY or BLACK
            // There are no RED objects
            
            // Handshake to ensure all mutators have seen the write to alloc
            
            {
                
                
                accept_entrants();
                
                {
                    // Request handshake and handover of infants
                    assert(mutators2.empty());
                    while (!mutators.empty()) {
                        Channel* channel = mutators.back();
                        mutators.pop_back();
                        bool abandoned = false;
                        {
                            std::unique_lock lock{channel->mutex};
                            assert(!channel->pending); // handshake fumbled?!
                            if (!channel->abandoned) {
                                channel->pending = true;
                                channel->request_infants = true;
                            } else {
                                abandoned = true;
                                assert(infants.empty());
                                infants.swap(channel->infants);
                            }
                            channel->ALLOC = local.ALLOC;
                        }
                        if (!abandoned) {
                            mutators2.push_back(channel);
                        } else {
                            delete channel;
                            objects.append(std::move(infants));
                        }
                    }

                    {
                        // scan the global roots
                        std::size_t count = 0;
                        for (Object* ref : global.roots) {
                            ++count;
                            shade(ref);
                        }
                                                
                        LOG("shaded %zd global roots\n", count);
                    }

                    // handshake ourself!
                    gc::handshake();
                    
                    // Receive acknowledgements and recent allocations
                    while (!mutators2.empty()) {
                        Channel* channel = mutators2.back();
                        mutators2.pop_back();
                        bool abandoned = false;
                        {
                            std::unique_lock lock{channel->mutex};
                            while (!channel->abandoned && channel->pending)
                                channel->condition_variable.wait(lock);
                            if (channel->abandoned)
                                abandoned = true;
                            channel->dirty = false;
                            assert(infants.empty());
                            infants.swap(channel->infants);
                        }
                        objects.append(std::move(infants));
                        if (!abandoned) {
                            mutators.push_back(channel);
                        } else {
                            delete channel;
                        }
                    }
                                        
                    // Mutators that entered before global.mutex.unlock have
                    // been handshaked
                    
                    // Mutators that entered after global.mutex.unlock
                    // synchronized with that unlock
                    
                    // Thus all mutators have seen
                    //     alloc = black;
                    
                    // All objects not in our list of objects were allocated
                    // black after alloc = black
                    
                    // The list of objects consists of
                    // - white objects
                    // - gray objects produced by the write barrier shading
                    //   recently white objects
                    // - black objects allocated between the alloc color change
                    //   and the handoff of objects
                }
              
            }

            LOG("end transition to allocating BLACK");
            
            // hack some stuff
            //markTable(&lox::gc.strings);


            // New allocations are BLACK
            // All objects allocated before the transition are in "objects"
            // The write barrier is turing WHITE objects GRAY or BLACK
            // All WHITE objects are in "objects"
            // All GRAY objects are in "objects"
            
            assert(blacklist.empty());
            assert(whitelist.empty());
            
            for (;;) {
                
                // assert(!local.dirty);
                // TODO: local.dirty means, the collector made a GRAY object,
                // or a mutator has reported making a GRAY object, and we must
                // scan for GRAY objects.
                //
                // It is possible to get here and not be dirty if the mutators
                // made no GRAY objects since we flipped BLACK to WHITE, so
                // we could skip directly to the possible-termination handshake.
                // But we did just ask them to scan their roots, so that is
                // a very niche case.
                
                do {
                    local.dirty = false; // <-- reset local dirty since we are now going to find any of the dirt that flipped this flag
                    std::size_t blacks = 0;
                    std::size_t grays = 0;
                    std::size_t whites = 0;
                    std::size_t reds = 0;
                    LOG("scanning...");
                    // for (Object* object : objects) {
                    while (!objects.empty()) {
                        //object->_gc_print();
                        Object* object = objects.front();
                        objects.pop_front();
                        assert(object);
                        //object->debug();
                        Color expected = GRAY;
                        object->color.compare_exchange_strong(expected,
                                                              local.BLACK(),
                                                              std::memory_order_relaxed,
                                                              std::memory_order_relaxed);
                        if (expected == (local.BLACK())) {
                            ++blacks;
                            blacklist.push_back(object);
                        } else if (expected == GRAY) { // GRAY -> BLACK
                            ++grays;
                            object->scan(working);
                            blacklist.push_back(object);
                            working.process();
                        } else if (expected == local.WHITE) {
                            ++whites;
                            whitelist.push_back(object);
                        } else {
                            printf("%ld\n", local.BLACK());
                            printf("%ld\n", local.WHITE);
                            printf("%ld\n", expected);
                            abort();
                        }
                    }
                    LOG("        ...scanning found BLACK=%zu, GRAY=%zu, WHITE=%zu, RED=%zu", blacks, grays, whites, reds);
                    swap(objects, whitelist);
                } while (local.dirty);
                
                assert(!local.dirty);
                
                // the collector has traced everything it knows about
                
                // handshake to see if the mutators have shaded any objects
                // GRAY since the last handshake
                
                // initiate handshakes
                
               
                
                accept_entrants();
               
                // request handshake
                                
                assert(mutators2.empty());
                while (!mutators.empty()) {
                    Channel* channel = mutators.back();
                    mutators.pop_back();
                    bool abandoned = false;
                    {
                        std::unique_lock lock{channel->mutex};
                        assert(!channel->pending);
                        if (!channel->abandoned) {
                            channel->pending = true;
                        } else {
                            abandoned = true;
                            if (channel->dirty) {
                                local.dirty = true;
                                channel->dirty = false;
                            }
                            assert(infants.empty());
                            infants.swap(channel->infants);
                        }
                    }
                    if (!abandoned) {
                        mutators2.push_back(channel);
                    } else {
                        delete channel;
                        // all of these orphans were created after the mutator
                        // observed alloc = black, so they should all be black,
                        // and we should put them directly into some place we
                        // won't keep rescanning
                        objects.append(std::move(infants));
                    }
                }
                // autoshake
                gc::handshake();
                // Receive acknowledgements and recent allocations
                while (!mutators2.empty()) {
                    Channel* channel = mutators2.back();
                    mutators2.pop_back();
                    bool abandoned = false;
                    {
                        std::unique_lock lock{channel->mutex};
                        while (!channel->abandoned && channel->pending)
                            channel->condition_variable.wait(lock);
                        if (channel->abandoned) {
                            abandoned = true;
                            assert(infants.empty());
                            infants.swap(channel->infants);
                        }
                        LOG("%p reports it was %s", channel, channel->dirty ? "dirty" : "clean");
                        if (channel->dirty) {
                            local.dirty = true;
                            channel->dirty = false;
                        }
                    }
                    if (!abandoned) {
                        mutators.push_back(channel);
                    } else {
                        delete channel;
                        objects.append(std::move(infants));
                    }
                }
                
                
                if (!local.dirty) {
                    break;
                }
                
                local.dirty = false;
                
            }
            
            // Neither the collectors nor mutators marked any nodes GRAY since
            // the last handshake.
            //
            // All remaining WHITE objects are strong-unreachable.
            //
            // The mutators and collectors race to change weak leaves from
            // WHITE to BLACK or RED
            
            {
                LOG("sweeping...");
                std::size_t blacks = 0;
                std::size_t whites = 0;
                std::size_t reds = 0;
                SweepContext context;
                context.WHITE = local.WHITE;
                while (!objects.empty()) {
                    Object* object = objects.front();
                    objects.pop_front();
                    assert(object);
                    Color after = object->sweep(context);
                    if (after == local.WHITE) {
                        // delete object;
                        ++whites;
                        ++freed;
                    } else if (after == (local.BLACK())) {
                        ++blacks;
                        blacklist.push_back(object);
                    } else if (after == RED) {
                        ++reds;
                        redlist.push_back(object);
                    }
                    
                }
                LOG("    ...sweeping found BLACK=%zu, WHITE=%zu, RED=%zu", blacks, whites, reds);
                LOG("freed %zu", whites);
            }
            
            // Only BLACK and RED objects exist
            // The mutators are allocating BLACK
            // The write barrier encounters no WHITE objects
            
            // Reinterpret BLACK as WHITE

            
            // Only WHITE and RED objects exist
            // The mutators are allocating WHITE
            // The write barrier turns some WHITE objects GRAY or BLACK
            // All colours exist
            
            {
                local.WHITE ^= 1;
                working.WHITE = local.WHITE;
                std::unique_lock lock{global.mutex};
                global.WHITE = local.WHITE;
            }

            accept_entrants();
            
            
            assert(mutators2.empty());
            while (!mutators.empty()) {
                Channel* channel = mutators.back();
                mutators.pop_back();
                bool abandoned = false;
                {
                    std::unique_lock lock{channel->mutex};
                    assert(!channel->pending);
                    if (!channel->abandoned) {
                        channel->pending = true;
                        assert(channel->infants.empty());
                    } else {
                        abandoned = true;
                        assert(infants.empty());
                        infants.swap(channel->infants);
                    }
                    channel->WHITE = local.WHITE;
                    channel->ALLOC = local.ALLOC;
                }
                if (!abandoned) {
                    mutators2.push_back(channel);
                } else {
                    delete channel;
                    // These orphans were all created with the same color value,
                    // which has changed meaning from black to white since the
                    // last handshake.  Some of them may have already been
                    // turned from white to gray or black by the write barrier.
                    objects.append(std::move(infants));
                }
            }
            // autoshake
            gc::handshake();
            // Receive acknowledgements and recent allocations
            while (!mutators2.empty()) {
                Channel* channel = mutators2.back();
                mutators2.pop_back();
                bool abandoned = false;
                {
                    std::unique_lock lock{channel->mutex};
                    LOG("%p reports it was %s", channel, channel->dirty ? "dirty" : "clean");
                    while (!channel->abandoned && channel->pending)
                        channel->condition_variable.wait(lock);
                    if (!channel->abandoned) {
                        LOG("%p acknowledges recoloring", channel);
                        assert(infants.empty());
                    } else {
                        LOG("%p leaves", channel);
                        abandoned = true;
                        assert(infants.empty());
                        infants.swap(channel->infants);
                    }
                    if (channel->dirty) {
                        local.dirty = true;
                        channel->dirty = false;
                    }
                }
                if (!abandoned) {
                    mutators.push_back(channel);
                } else {
                    delete channel;
                    objects.append(std::move(infants));
                }
            }
            
            // claim the red objects
            
            {
                std::size_t reds = 0;
                while (!redlist.empty()) {
                    Object* object = redlist.front();
                    redlist.pop_front();
                    delete object;
                    ++reds;
                    ++freed;
                }
                LOG("freed REDS %zd", reds);
            }
            
            while (!blacklist.empty()) {
                Object* object = blacklist.front();
                blacklist.pop_front();
                objects.push_back(object);
            }
            
            
            
        }
        
        leave();
        
    }
    
    
    
    
    
    

} // namespace gc
