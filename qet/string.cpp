//
//  string.cpp
//  gch
//
//  Created by Antony Searle on 20/4/2024.
//

#include "string.hpp"

namespace gc {

    // Concurrent hash array mapped trie
    //
    // https://en.wikipedia.org/wiki/Ctrie
    //
    // Prokopec, A., Bronson N., Bagwell P., Odersky M. (2012) Concurrent Tries
    //     with Efficient Non-Blocking Snapshots
    
    // A concurrent map with good worst-case performance
    //
    // Use case is weak set of interned strings
    
    namespace _string {
        
        
        void clean(INode* i, int lev);
        void cleanParent(INode* p, INode* i, std::size_t hc, int lev);
        MNode* entomb(SNode* sn);
        std::pair<Result, SNode*> ilookup(INode* i, Query q, int lev, INode* parent);
        std::pair<Result, SNode*> iinsert(INode* i, Query q, int lev, INode* parent);
        std::pair<Result, SNode*> iremove(INode* i, SNode* k, int lev, INode* parent);
        BNode* resurrect(BNode* m);
        MNode* toCompressed(CNode* cn, int lev);
        MNode* toContracted(CNode* cn, int lev);
        
        struct MNode : ANode {
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent) = 0;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent) = 0;
            virtual void _erase2(INode* i, SNode* k, int lev, INode* parent) {};
            virtual BNode* _resurrect(INode* parent);
            virtual void vcleanA(INode* i, int lev) {}
            virtual bool vcleanParentA(INode* p, INode* i, std::size_t hc, int lev,
                                       MNode* m) { return true; }
            virtual bool vcleanParentB(INode* p, INode* i, std::size_t hc, int lev,
                                       MNode* m,
                                       CNode* cn, std::uint64_t flag, int pos) { return true; }
            virtual void _gc_debug() const override = 0;
        }; // struct MNode
        
        struct INode : BNode {
            explicit INode(MNode* desired);
            virtual void _gc_scan(ScanContext& context) const override ;
            virtual std::size_t _gc_bytes() const override;
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent,
                                                             CNode* cn, std::uint64_t flag, int pos) override;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent,
                                                           CNode* cn, std::uint64_t flag, int pos) override;
            virtual BNode* _resurrect() override;
            virtual MNode* _contract(CNode* cn, int lev) override;
            mutable Atomic<StrongPtr<MNode>> main;
            virtual void _gc_debug() const override;
        }; // struct INode
        
        struct TNode : MNode {
            virtual void _gc_scan(ScanContext& context) const override;
            virtual std::size_t _gc_bytes() const override;
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent) override;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent) override;
            virtual void _erase2(INode* i, SNode* k, int lev, INode* parent) override;
            virtual bool vcleanParentB(INode* p, INode* i, std::size_t hc, int lev,
                                       MNode* m,
                                       CNode* cn, std::uint64_t flag, int pos) override;
            virtual BNode* _resurrect(INode* parent) override;
            SNode* sn;
            virtual void _gc_debug() const override;
        }; // struct TNode
        
        struct LNode : MNode {
            virtual void _gc_scan(ScanContext& context) const override;
            virtual std::size_t _gc_bytes() const override;
            std::pair<Result, SNode*> lookup(Query q);
            MNode* inserted(Query q);
            std::pair<LNode*, SNode*> removed(SNode* k);
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent) override;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent) override;
            SNode* sn;
            LNode* next;
            virtual void _gc_debug() const override;
        }; // struct LNode
        
        
        struct CNode : MNode {
            static std::pair<std::uint64_t, int> flagpos(std::size_t hash, int lev, std::uint64_t bmp);
            static CNode* make(SNode* sn1, SNode* sn2, int lev);
            static CNode* make(std::size_t count);
            CNode();
            virtual void _gc_debug() const override;
            virtual void _gc_scan(ScanContext& context) const override;
            virtual std::size_t _gc_bytes() const override;

            CNode* inserted(std::uint64_t flag, int pos, BNode* child) const;
            CNode* updated(int pos, BNode* child) const;
            CNode* removed(int pos, std::uint64_t flag) const;
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent) override;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent) override;
            virtual void vcleanA(INode* i, int lev) override;
            virtual bool vcleanParentA(INode* p, INode* i, std::size_t hc, int lev,
                                       MNode* m) override ;
            std::uint64_t bmp;
            BNode* array[0];
                        
        }; // struct CNode
        
        
        struct Ctrie : Object {
            
            Ctrie();
            
            void debug();
            
            virtual void _gc_scan(ScanContext& context) const override;
            virtual std::size_t _gc_bytes() const override;
            virtual void _gc_debug() const override;

            SNode* emplace(Query q);
            SNode* remove(SNode* k);
            
            INode* root;
            
        }; // struct Ctrie
        
        
        Ctrie* global_string_ctrie = nullptr;
        
        
        BNode* resurrect(BNode* m) {
            return m->_resurrect();
        }
        
        MNode* toCompressed(CNode* cn, int lev) {
            int num = __builtin_popcountll(cn->bmp);
            CNode* ncn = CNode::make(num); // new (extra_val_t{sizeof(BNode*) * num}) CNode;
            ncn->bmp = cn->bmp;
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != num; ++i) {
                ncn->array[i] = resurrect(cn->array[i]);
                ncn->array[i]->_gc_shade_weak(context);
            }
            return toContracted(ncn, lev);
        }
        
        MNode* toContracted(CNode* cn, int lev) {
            int num = __builtin_popcountll(cn->bmp);
            if (lev == 0 || num > 1)
                return cn;
            return cn->array[0]->_contract(cn, lev);
        }
        
        void clean(INode* i, int lev) {
            i->main.load(std::memory_order::acquire)->vcleanA(i, lev);
        }
        
        void cleanParent(INode* p, INode* i, std::size_t hc, int lev) {
            for (;;) {
                MNode* m = i->main.load(std::memory_order::acquire); // <-- TODO we only redo this if it is a TNode and therefore final
                MNode* pm = p->main.load(std::memory_order::acquire); // <-- TODO get this from the failed CAS
                if (pm->vcleanParentA(p, i, hc, lev, m))
                    return;
            }
        }
        
        MNode* entomb(SNode* sn) {
            TNode* tn = new TNode;
            tn->sn = sn;
            // gc::shade(sn);
            return tn;
        }
        
        
        
        
        
        BNode* MNode::_resurrect(INode* parent) { return parent; };
        
        
        
        INode::INode(MNode* desired) : main(desired) {}
        
        void INode::_gc_scan(ScanContext& context) const {
            context.push(main);
        }
        
        std::pair<Result, SNode*> INode::_emplace(INode* i, Query q, int lev, INode* parent,
                                                        CNode* cn, std::uint64_t flag, int pos) {
            return iinsert(this, q, lev + 6, i);
        }
        
        std::pair<Result, SNode*> INode::_erase(INode* i, SNode* k, int lev, INode* parent,
                                                      CNode* cn, std::uint64_t flag, int pos) {
            return iremove(this, k, lev + 6, i);
        }
        
        BNode* INode::_resurrect() {
            return this->main.load(std::memory_order::acquire)->_resurrect(this);
        }
        
        MNode* INode::_contract(CNode* cn, int lev) {
            return cn;
        }
        
        
        SNode* SNode::make(Query q) {
            return global_string_ctrie->emplace(q);
        }
        
        SNode* SNode::make(const char * data, std::size_t size) {
            return make(Query(std::string_view(data, size)));
        }
        
        SNode* SNode::_make(Query q) {
            return new(alloc(sizeof(SNode) + q.view.size() + 1)) SNode(q);
        }
        
        
        SNode::SNode(Query q)
        : _hash(q.hash)
        , _size(q.view.size()) {
            std::memcpy(_data, q.view.data(), _size);
            _data[_size] = '\0';
        }
                        
        void SNode::_gc_shade_weak(ShadeContext& context) const {
            // no-op; SNode supports weak references
        }
        
        void SNode::_gc_scan_weak(ScanContext& context) const {
            // no-op; SNode supports weak references
        }
        
        Color SNode::_gc_sweep(SweepContext& context) {
            Color expected = context.WHITE;
            this->color.compare_exchange_strong(expected, 
                                                Color::RED,
                                                std::memory_order::relaxed,
                                                std::memory_order::relaxed);
            if (expected == context.WHITE) {
                printf("%p WHITE -> RED gc::String{%zx,%zd,\"%.*s\"}\n",
                       this,
                       _hash,
                       _size,
                       (int)_size,
                       _data);
                global_string_ctrie->remove(this);
                return Color::RED;
            } else if (expected == (context.BLACK())) {
                return expected;
            } else if (expected == Color::RED) {
                printf("%p RED -> X gc::String{%zx,%zd,\"%.*s\"}\n",
                       this,
                       _hash,
                       _size,
                       (int)_size,
                       _data);
                delete this;
                return context.WHITE;
            } else {
                abort();
            }
        }
        
        std::pair<Result, SNode*> SNode::_emplace(INode* i, Query q, int lev, INode* parent,
                                                        CNode* cn, std::uint64_t flag, int pos) {
            SNode* sn = this;
            bool equivalent = sn->_hash == q.hash && sn->view() == q.view;
            if (equivalent) {
                Color expected = local.WHITE;
                Color desired{static_cast<std::intptr_t>(expected) ^ 1};
                // Attempt upgrade
                this->color.compare_exchange_strong(expected, desired, std::memory_order::relaxed, std::memory_order::relaxed);
                assert(expected != Color::GRAY);
                if (expected != Color::RED) {
                    return {OK, this};
                }
            }
            // We must install a new node
            SNode* nsn = SNode::_make(q); // new (extra_val_t{q.view.size()+1}) SNode(q);
            CNode* ncn = cn->updated(pos,
                                           (equivalent
                                            ? (BNode*) nsn
                                            : (BNode*) new INode(CNode::make(sn, nsn, lev + 6))));
            MNode* expected = cn;
            if (i->main.compare_exchange_strong(expected,
                                                ncn,
                                                std::memory_order::release,
                                                std::memory_order::relaxed)) {
                return {OK, nsn};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        
        
        std::pair<Result, SNode*> SNode::_erase(INode* i, SNode* k, int lev, INode* parent,
                                                      CNode* cn, std::uint64_t flag, int pos) {
            if (this != k)
                return std::pair(OK, nullptr);
            CNode* ncn = cn->removed(pos, flag);
            MNode* cntr = toContracted(ncn, lev);
            MNode* expected = cn;
            if (i->main.compare_exchange_strong(expected,
                                                cntr,
                                                std::memory_order::release,
                                                std::memory_order::relaxed)) {
                return {OK, this};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        BNode* SNode::_resurrect() {
            return this;
        }
        
        MNode* SNode::_contract(CNode* cn, int lev) {
            SNode* sn = this;
            return entomb(sn);
        }
        
        
        
        void SNode::enter() {
            assert(global_string_ctrie == nullptr);
            global_string_ctrie = new Ctrie;
            // TODO: global roots should be a thread-safe set
            global.roots.push_back(global_string_ctrie);
        }
        
        void SNode::leave() {
            assert(global_string_ctrie != nullptr);
            // TODO: global roots should be a thread-safe set
            // TODO: global.roots.erase(global_string_ctrie)
            global_string_ctrie = nullptr;
        }
        
        
        
        void TNode::_gc_scan(ScanContext& context) const {
            context.push(sn);
        }
        
        std::pair<Result, SNode*> TNode::_emplace(INode* i, Query q, int lev, INode* parent) {
            clean(parent, lev - 6);
            return {RESTART, nullptr};
        }
        
        std::pair<Result, SNode*> TNode::_erase(INode* i, SNode* k, int lev, INode* parent) {
            clean(parent, lev - 6);
            return {RESTART, nullptr};
        }
        
        BNode* TNode::_resurrect(INode* parent) {
            return sn;
        }
        
        bool TNode::vcleanParentB(INode* p, INode* i, std::size_t hc, int lev,
                                  MNode* m,
                                  CNode* cn, std::uint64_t flag, int pos) {
            CNode* ncn = cn->updated(pos, this->sn);
            MNode* expected = cn;
            MNode* desired = toContracted(ncn, lev);
            return p->main.compare_exchange_strong(expected,
                                                   desired,
                                                   std::memory_order::release,
                                                   std::memory_order::relaxed);
        }
        
        void TNode::_erase2(INode* i, SNode* k, int lev, INode* parent) {
            cleanParent(parent, i, k->_hash, lev - 6);
        }
        
        void LNode::_gc_scan(ScanContext& context) const {
            context.push(sn);
            context.push(next);
        }
        
        std::pair<Result, SNode*> LNode::lookup(Query q) {
            const LNode* ln = this;
            for (;;) {
                if (!ln)
                    return {OK, nullptr};
                if (ln->sn->view() == q.view)
                    return {OK, ln->sn};
                ln = ln->next;
            }
        }
        
        MNode* LNode::inserted(Query q) {
            const LNode* a = this;
            for (;;) {
                if (a->sn->view() == q.view) {
                    // The key exists in the list, so we must copy the
                    // first part, replace that node, and point to the
                    // second part
                    const LNode* b = this;
                    LNode* c = new LNode;
                    LNode* d = c;
                    for (;;) {
                        if (b != a) {
                            d->sn = b->sn;
                            // gc::shade(d->sn);
                            b = b->next;
                            LNode* e = new LNode;
                            d->next = e;
                            d = e;
                        } else {
                            d->sn = SNode::_make(q); //new (extra_val_t{q.view.size()+1}) SNode(q);
                            d->next = b->next;
                            gc::shade(d->next);
                            return c;
                        }
                    }
                } else {
                    a = a->next;
                }
                if (a == nullptr) {
                    // We did not find the same key, so just prepend it
                    LNode* b = new LNode;
                    b->sn = SNode::_make(q); //new (extra_val_t{q.view.size()+1}) SNode(q);
                    b->next = this;
                    gc::shade(b->next);
                    return b;
                }
            }
        }
        
        std::pair<LNode*, SNode*> LNode::removed(SNode* k) {
            if (this->sn == k)
                return {this->next, this->sn};
            const LNode* a = this->next;
            for (;;) {
                if (a == nullptr) {
                    // Not found at all
                    return {this, nullptr};
                }
                if (a->sn != k) {
                    // Not found yet
                    a = a->next;
                } else {
                    // Found inside the list
                    const LNode* b = this;
                    LNode* c = new LNode; // make new head
                    LNode* d = c;
                    for (;;) {
                        d->sn = b->sn; // copy over node
                                       // gc::shade(d->sn);
                        b = b->next;
                        if (b == a) {
                            // we've reached the node we are erasing, skip
                            // over it
                            d->next = a->next;
                            gc::shade(d->next);
                            return {c, a->sn};
                        }
                        LNode* e = new LNode;
                        d->next = e;
                        d = e;
                    }
                }
            }
        }
        
        
        std::pair<Result, SNode*> LNode::_emplace(INode* i, Query q, int lev, INode* parent) {
            // printf("LNode %lx,%p iinsert\n", this->color.load(RELAXED), this);
            MNode* expected = this;
            if (i->main.compare_exchange_strong(expected,
                                                inserted(q),
                                                std::memory_order::release,
                                                std::memory_order::relaxed)) {
                return {OK, nullptr};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        std::pair<Result, SNode*> LNode::_erase(INode* i, SNode* k, int lev, INode* parent) {
            LNode* ln = this;
            auto [nln, v] = ln->removed(k);
            assert(nln && nln->sn);
            MNode* expected = ln;
            MNode* desired = nln->next ? nln : entomb(nln->sn);
            if (i->main.compare_exchange_strong(expected,
                                                desired,
                                                std::memory_order::release,
                                                std::memory_order::relaxed)) {
                return {OK, v};
            } else {
                return {RESTART, nullptr};
            }
        }
        
                
        void CNode::_gc_scan(ScanContext& context) const {
            int num = __builtin_popcountll(this->bmp);
            for (int i = 0; i != num; ++i) {
                this->array[i]->_gc_scan_weak(context);
            }
        }
        
        std::pair<std::uint64_t, int> CNode::flagpos(std::size_t hash, int lev, std::uint64_t bmp) {
            auto a = (hash >> lev) & 63;
            std::uint64_t flag = std::uint64_t{1} << a;
            int pos = __builtin_popcountll(bmp & (flag - 1));
            return std::pair(flag, pos);
        }
        
        CNode* CNode::inserted(std::uint64_t flag, int pos, BNode* child) const {
            //printf("CNode inserted\n");
            auto n = __builtin_popcountll(bmp);
            CNode* b = CNode::make(n+1); // new (extra_val_t{sizeof(BNode*) * (n + 1)}) CNode;
            assert(!(this->bmp & flag));
            b->bmp = this->bmp | flag;
            std::memcpy(b->array, this->array, sizeof(BNode*) * pos);
            b->array[pos] = child;
            std::memcpy(b->array + pos + 1, this->array + pos, sizeof(BNode*) * (n - pos));
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n + 1; ++i)
                b->array[i]->_gc_shade_weak(context);
            return b;
        }
        
        CNode* CNode::updated(int pos, BNode* child) const {
            //printf("CNode updated\n");
            auto n = __builtin_popcountll(bmp);
            CNode* b = CNode::make(n); //new (extra_val_t{sizeof(BNode*) * n}) CNode;
            b->bmp = this->bmp;
            std::memcpy(b->array, this->array, sizeof(BNode*) * n);
            b->array[pos] = child;
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n; ++i)
                b->array[i]->_gc_shade_weak(context);
            return b;
        }
        
        CNode* CNode::removed(int pos, std::uint64_t flag) const {
            assert(this->bmp & flag);
            assert(__builtin_popcountll((flag - 1) & this->bmp) == pos);
            auto n = __builtin_popcountll(bmp);
            assert(pos < n);
            CNode* b = CNode::make(n-1); //new (extra_val_t{sizeof(BNode*) * (n - 1)}) CNode;
            b->bmp = this->bmp ^ flag;
            std::memcpy(b->array, this->array, sizeof(BNode*) * pos);
            std::memcpy(b->array + pos, this->array + pos + 1, sizeof(BNode*) * (n - 1 - pos));
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n - 1; ++i)
                b->array[i]->_gc_shade_weak(context);
            return b;
        }
        
        CNode::CNode() : bmp{0} {}
        
        CNode* CNode::make(SNode* sn1, SNode* sn2, int lev) {
            assert(sn1->view() != sn2->view());
            // distinct keys but potentially the same hash
            auto a1 = (sn1->_hash >> lev) & 63;
            auto a2 = (sn2->_hash >> lev) & 63;
            //printf("a1 a2 %ld %ld\n", a1, a2);
            std::uint64_t flag1 = std::uint64_t{1} << a1;
            if (a1 != a2) {
                // different hash at lev
                std::uint64_t flag2 = std::uint64_t{1} << a2;
                CNode* c = CNode::make(2); //new (extra_val_t{sizeof(BNode) * 2}) CNode;
                c->bmp = flag1 | flag2;
                int pos1 = a1 > a2;
                int pos2 = a2 > a1;
                c->array[pos1] = sn1;
                c->array[pos2] = sn2;
                return c;
            } else {
                // same hash at lev
                CNode* c = CNode::make(1); // new (extra_val_t{sizeof(BNode)}) CNode;
                c->bmp = flag1;
                if (lev + 6 < 64) {
                    c->array[0] = new INode(make(sn1, sn2, lev + 6));
                } else {
                    // true hash collision,
                    // TODO: should we check for this immediately before we
                    // make the chain of CNodes?  I think it's benign to have
                    // LNodes at any level so long as they all share the hash
                    // value; we may need to add a CNode::make flavor with
                    // INodes->LNodes though?
                    LNode* d = new LNode;
                    d->sn = sn1;
                    d->next = nullptr;
                    LNode* e = new LNode;
                    e->sn = sn2;
                    e->next = d;
                    c->array[0] = new INode(e);
                }
                return c;
            }
        }
        
        CNode* CNode::make(std::size_t count) {
            return new(alloc(sizeof(CNode) + sizeof(BNode*) * count)) CNode;
        }

        
        std::pair<Result, SNode*> CNode::_emplace(INode* i, Query q, int lev, INode* parent) {
            CNode* cn = this;
            auto [flag, pos] = flagpos(q.hash, lev, cn->bmp);
            if (!(flag & cn->bmp)) {
                MNode* expected = this;
                SNode* sn = SNode::_make(q); //new (extra_val_t{q.view.size()+1}) SNode(q);
                MNode* desired = inserted(flag, pos, sn);
                if (i->main.compare_exchange_strong(expected, desired, std::memory_order::release, std::memory_order::relaxed)) {
                    return {OK, sn};
                } else {
                    return {RESTART, nullptr};
                }
            } else {
                return array[pos]->_emplace(i, q, lev, parent, cn, flag, pos);
            }
        }
        
        std::pair<Result, SNode*> CNode::_erase(INode* i, SNode* k, int lev, INode* parent) {
            auto [flag, pos] = flagpos(k->_hash, lev, bmp);
            if (!(flag & bmp)) {
                return {OK, nullptr};
            }
            BNode* sub = array[pos];
            assert(sub);
            auto [res, value] = sub->_erase(i, k, lev, parent, this, flag, pos);
            if (res == OK) {
                i->main.load(std::memory_order::acquire)->_erase2(i, k, lev, parent);
            }
            return {res, value};
        }
        
        void CNode::vcleanA(INode* i, int lev) {
            CNode* m = this;
            MNode* expected = m;
            MNode* desired = toCompressed(m, lev);
            i->main.compare_exchange_strong(expected, 
                                            desired,
                                            std::memory_order::release,
                                            std::memory_order::relaxed);
        }
        
        bool CNode::vcleanParentA(INode* p, INode* i, std::size_t hc, int lev,
                                  MNode* m) {
            CNode* cn = this;
            auto [flag, pos] = flagpos(hc, lev, this->bmp);
            if (!(flag & bmp))
                return true;
            BNode* sub = this->array[pos];
            if (sub != i)
                return true;
            return m->vcleanParentB(p, i, hc, lev, m, cn, flag, pos);
        }
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        std::pair<Result, SNode*> iinsert(INode* i, Query q, int lev, INode* parent) {
            return i->main.load(std::memory_order::acquire)->_emplace(i, q, lev, parent);
        }
        
        
        
        
        
        std::pair<Result, SNode*> iremove(INode* i, SNode* k, int lev, INode* parent) {
            return i->main.load(std::memory_order::acquire)->_erase(i, k, lev, parent);
        }
        
        
        
        
        
        
        
        Ctrie::Ctrie()
        : root(new INode(new CNode)) {
        }
        
        void Ctrie::debug() {
            printf("%p %s gc::_string::Ctrie{}\n", this, ColorCString(color.load(std::memory_order::relaxed)));
        }
        
        void Ctrie::_gc_scan(ScanContext& context) const {
            gc::LOG("Ctrie::scan");
            context.push(root);
        }
        
        SNode* Ctrie::emplace(Query q) {
            for (;;) {
                INode* r = root;
                auto [res, v2] = iinsert(r, q, 0, nullptr);
                if (res == RESTART)
                    continue;
                assert(v2);
                return v2;
            }
        }
        
        SNode* Ctrie::remove(SNode* k) {
            for (;;) {
                INode* r = root;
                auto [res, v] = iremove(r, k, 0, nullptr);
                if (res == RESTART)
                    continue;
                return v;
            }
        }
                
        std::size_t CNode::_gc_bytes() const {
            return sizeof(CNode) + sizeof(BNode*) * __builtin_popcountll(bmp);
        }
        
        std::size_t Ctrie::_gc_bytes() const {
            return sizeof(Ctrie);
        }

        std::size_t INode::_gc_bytes() const {
            return sizeof(INode);
        }

        std::size_t LNode::_gc_bytes() const {
            return sizeof(LNode);
        }
        
        std::size_t SNode::_gc_bytes() const {
            return sizeof(SNode) + _size + 1;
        }

        std::size_t TNode::_gc_bytes() const {
            return sizeof(TNode);
        }

        
                
        void CNode::_gc_debug() const {
            printf("%p %s gc::_string::CNode{%llx, ...}\n",
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)), bmp);
        }

        void Ctrie::_gc_debug() const {
            printf("%p %s gc::_string::Ctrie\n",
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)));
        }
        
        void INode::_gc_debug() const {
            printf("%p %s gc::_string::INode\n",
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)));
        }
        
        void LNode::_gc_debug() const {
            printf("%p %s gc::_string::LNode\n",
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)));
        }

        void SNode::_gc_debug() const {
            printf("%p %s gc::String{%zx,%zd,\"%.*s\"}\n", 
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)),
                   _hash,
                   _size,
                   (int)_size,
                   _data);
        }
        
        SNode::~SNode() {
            printf("%p RED -> ~ gc::String{%zx,%zd,\"%.*s\"}\n",
                   this,
                   _hash,
                   _size,
                   (int)_size,
                   _data);       
        }

        void TNode::_gc_debug() const {
            printf("%p %s gc::_string::TNode\n",
                   this,
                   ColorCString(color.load(std::memory_order::relaxed)));
        }


        

    } // namespace _string
    
} // namespace gc
