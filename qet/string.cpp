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
        
        
        void clean(const INode* i, int lev);
        void cleanParent(const INode* p, const INode* i, std::size_t hc, int lev);
        const MNode* entomb(const SNode* sn);
        std::pair<Result, const SNode*> ilookup(const INode* i, Query q, int lev, const INode* parent);
        std::pair<Result, const SNode*> iinsert(const INode* i, Query q, int lev, const INode* parent);
        std::pair<Result, const SNode*> iremove(const INode* i, const SNode* k, int lev, const INode* parent);
        const BNode* resurrect(const BNode* m);
        const MNode* toCompressed(const CNode* cn, int lev);
        const MNode* toContracted(const CNode* cn, int lev);
        
        struct MNode : ANode {
            virtual std::pair<Result, const SNode*> _emplace(const INode* i, Query q, int lev, const INode* parent) const = 0;
            virtual std::pair<Result, const SNode*> _erase(const INode* i, const SNode* k, int lev, const INode* parent) const = 0;
            virtual void _erase2(const INode* i, const SNode* k, int lev, const INode* parent) const {};
            virtual const BNode* _resurrect(const INode* parent) const;
            virtual void vcleanA(const INode* i, int lev) const {}
            virtual bool vcleanParentA(const INode* p, const INode* i, std::size_t hc, int lev,
                                       const MNode* m) const { return true; }
            virtual bool vcleanParentB(const INode* p, const INode* i, std::size_t hc, int lev,
                                       const MNode* m,
                                       const CNode* cn, std::uint64_t flag, int pos) const { return true; }
            virtual void debug() const override = 0;
            virtual void debug(int lev) const override = 0;
        }; // struct MNode
        
        struct INode : BNode {
            explicit INode(const MNode* desired);
            virtual void debug(int lev) const override;
            virtual void scan(ScanContext& context) const override ;
            virtual std::pair<Result, const SNode*> _emplace(const INode* i, Query q, int lev, const INode* parent,
                                                             const CNode* cn, std::uint64_t flag, int pos) const override;
            virtual std::pair<Result, const SNode*> _erase(const INode* i, const SNode* k, int lev, const INode* parent,
                                                           const CNode* cn, std::uint64_t flag, int pos) const override;
            virtual const BNode* _resurrect() const override;
            virtual const MNode* _contract(const CNode* cn, int lev) const override;
            mutable Atomic<StrongPtr<const MNode>> main;
            virtual void debug() const override { printf("%p gc::_string::INode\n", this); }
        }; // struct INode
        
        struct TNode : MNode {
            virtual void debug(int lev) const override;
            virtual void scan(ScanContext& context) const override;
            virtual std::pair<Result, const SNode*> _emplace(const INode* i, Query q, int lev, const INode* parent) const override;
            virtual std::pair<Result, const SNode*> _erase(const INode* i, const SNode* k, int lev, const INode* parent) const override;
            virtual void _erase2(const INode* i, const SNode* k, int lev, const INode* parent) const override;
            virtual bool vcleanParentB(const INode* p, const INode* i, std::size_t hc, int lev,
                                       const MNode* m,
                                       const CNode* cn, std::uint64_t flag, int pos) const override;
            virtual const BNode* _resurrect(const INode* parent) const override;
            const SNode* sn;
            virtual void debug() const override { printf("%p gc::_string::TNode\n", this); }
        }; // struct TNode
        
        struct LNode : MNode {
            virtual void debug(int lev) const override;
            virtual void scan(ScanContext& context) const override;
            std::pair<Result, const SNode*> lookup(Query q) const;
            const MNode* inserted(Query q) const;
            std::pair<const LNode*, const SNode*> removed(const SNode* k) const;
            virtual std::pair<Result, const SNode*> _emplace(const INode* i, Query q, int lev, const INode* parent) const override;
            virtual std::pair<Result, const SNode*> _erase(const INode* i, const SNode* k, int lev, const INode* parent) const override;
            const SNode* sn;
            const LNode* next;
            virtual void debug() const override { printf("%p gc::_string::LNode\n", this); }
        }; // struct LNode
        
        
        struct CNode : MNode {
            static std::pair<std::uint64_t, int> flagpos(std::size_t hash, int lev, std::uint64_t bmp);
            static const CNode* make(const SNode* sn1, const SNode* sn2, int lev);
            CNode();
            virtual void debug(int lev) const override;
            virtual void scan(ScanContext& context) const override;
            const CNode* inserted(std::uint64_t flag, int pos, const BNode* child) const;
            const CNode* updated(int pos, const BNode* child) const;
            const CNode* removed(int pos, std::uint64_t flag) const;
            virtual std::pair<Result, const SNode*> _emplace(const INode* i, Query q, int lev, const INode* parent) const override;
            virtual std::pair<Result, const SNode*> _erase(const INode* i, const SNode* k, int lev, const INode* parent) const override;
            virtual void vcleanA(const INode* i, int lev) const override;
            virtual bool vcleanParentA(const INode* p, const INode* i, std::size_t hc, int lev,
                                       const MNode* m) const override ;
            virtual void debug() const override;
            std::uint64_t bmp;
            const BNode* array[0];
        }; // struct CNode
        
        
        struct Ctrie : Object {
            
            Ctrie();
            
            void debug();
            
            virtual void scan(ScanContext& context) const override;
            const SNode* emplace(Query q);
            const SNode* remove(const SNode* k);
            
            INode* root;
            
        }; // struct Ctrie
        
        
        Ctrie* global_string_ctrie = nullptr;
        
        
        const BNode* resurrect(const BNode* m) {
            return m->_resurrect();
        }
        
        const MNode* toCompressed(const CNode* cn, int lev) {
            int num = __builtin_popcountll(cn->bmp);
            CNode* ncn = new (extra_val_t{sizeof(BNode*) * num}) CNode;
            ncn->bmp = cn->bmp;
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != num; ++i) {
                ncn->array[i] = resurrect(cn->array[i]);
                ncn->array[i]->shade_weak(context);
            }
            return toContracted(ncn, lev);
        }
        
        const MNode* toContracted(const CNode* cn, int lev) {
            int num = __builtin_popcountll(cn->bmp);
            if (lev == 0 || num > 1)
                return cn;
            return cn->array[0]->_contract(cn, lev);
        }
        
        void clean(const INode* i, int lev) {
            i->main.load(ACQUIRE)->vcleanA(i, lev);
        }
        
        void cleanParent(const INode* p, const INode* i, std::size_t hc, int lev) {
            for (;;) {
                const MNode* m = i->main.load(ACQUIRE); // <-- TODO we only redo this if it is a TNode and therefore final
                const MNode* pm = p->main.load(ACQUIRE); // <-- TODO get this from the failed CAS
                if (pm->vcleanParentA(p, i, hc, lev, m))
                    return;
            }
        }
        
        const MNode* entomb(const SNode* sn) {
            TNode* tn = new TNode;
            tn->sn = sn;
            // gc::shade(sn);
            return tn;
        }
        
        
        
        
        
        const BNode* MNode::_resurrect(const INode* parent) const { return parent; };
        
        
        
        INode::INode(const MNode* desired) : main(desired) {}
        
        void INode::debug(int lev) const {
            auto p =  main.load(ACQUIRE);
            printf("INode(%lx): ",this->color.load(RELAXED));
            p->debug(lev);
            
        }
        
        void INode::scan(ScanContext& context) const {
            context.push(main);
        }
        
        std::pair<Result, const SNode*> INode::_emplace(const INode* i, Query q, int lev, const INode* parent,
                                                        const CNode* cn, std::uint64_t flag, int pos) const {
            return iinsert(this, q, lev + 6, i);
        }
        
        std::pair<Result, const SNode*> INode::_erase(const INode* i, const SNode* k, int lev, const INode* parent,
                                                      const CNode* cn, std::uint64_t flag, int pos) const {
            return iremove(this, k, lev + 6, i);
        }
        
        const BNode* INode::_resurrect() const {
            return this->main.load(ACQUIRE)->_resurrect(this);
        }
        
        const MNode* INode::_contract(const CNode* cn, int lev) const {
            return cn;
        }
        
        
        const SNode* SNode::make(Query q) {
            return global_string_ctrie->emplace(q);
        }
        
        
        SNode::SNode(Query q)
        : _hash(q.hash)
        , _size(q.view.size()) {
            std::memcpy(_data, q.view.data(), _size);
        }
        
        void SNode::debug(int lev) const {
            printf("SNode(%lx,\"%.*s\") %ld %ld\n",
                   this->color.load(RELAXED),
                   (int) _size,
                   _data,
                   _hash & 63,
                   (_hash >> 6) & 63);
        }
        
        void SNode::shade(ShadeContext& context) const {
            Color expected = context.WHITE;
            color.compare_exchange_strong(expected,
                                          context.BLACK(),
                                          RELAXED,
                                          RELAXED);
        }
        
        void SNode::shade_weak(ShadeContext& context) const {
            // no-op; SNode supports weak references
        }
        
        void SNode::scan(ScanContext& context) const {
            // no-op; SNode has no members
            __builtin_trap();
        }
        
        void SNode::scan_weak(ScanContext& context) const {
            // no-op; SNode supports weak references
        }
        
        Color SNode::sweep(SweepContext& context) {
            Color expected = context.WHITE;
            
            // Race to color the object RED before a mutator colors it BLACK
            this->color.compare_exchange_strong(expected, RED, RELAXED, RELAXED);
            
            if (expected == context.WHITE) {
                printf("Turned a string RED\n");
                // WHITE -> RED
                // We won the race, now race any losing mutators to remove this
                // by identity while they attempt to replace it with a new
                // equivalent node
                global_string_ctrie->remove(this);
                return RED;
            } else if (expected == (context.BLACK())) {
                printf("Preserved a BLACK string\n");
                return expected;
            } else if (expected == RED) {
                // Second sweep, no mutators can see us
                printf("Deleting a RED string\n");
                
                delete this;
                return context.WHITE;
            } else {
                abort();
            }
        }
        
        std::pair<Result, const SNode*> SNode::_emplace(const INode* i, Query q, int lev, const INode* parent,
                                                        const CNode* cn, std::uint64_t flag, int pos) const {
            const SNode* sn = this;
            bool equivalent = sn->_hash == q.hash && sn->view() == q.view;
            if (equivalent) {
                Color expected = local.WHITE;
                Color desired = expected ^ 1;
                // Attempt upgrade
                this->color.compare_exchange_strong(expected, desired, RELAXED, RELAXED);
                assert(expected != GRAY);
                if (expected != RED) {
                    return {OK, this};
                }
            }
            // We must install a new node
            const SNode* nsn = new (extra_val_t{q.view.size()}) SNode(q);
            const CNode* ncn = cn->updated(pos,
                                           (equivalent
                                            ? (const BNode*) nsn
                                            : (const BNode*) new INode(CNode::make(sn, nsn, lev + 6))));
            const MNode* expected = cn;
            if (i->main.compare_exchange_strong(expected,
                                                ncn,
                                                RELEASE,
                                                RELAXED)) {
                return {OK, nsn};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        
        
        std::pair<Result, const SNode*> SNode::_erase(const INode* i, const SNode* k, int lev, const INode* parent,
                                                      const CNode* cn, std::uint64_t flag, int pos) const {
            if (this != k)
                return std::pair(OK, nullptr);
            const CNode* ncn = cn->removed(pos, flag);
            const MNode* cntr = toContracted(ncn, lev);
            const MNode* expected = cn;
            if (i->main.compare_exchange_strong(expected,
                                                cntr,
                                                RELEASE,
                                                RELAXED)) {
                return {OK, this};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        const BNode* SNode::_resurrect() const {
            return this;
        }
        
        const MNode* SNode::_contract(const CNode* cn, int lev) const {
            const SNode* sn = this;
            return entomb(sn);
        }
        
        
        
        void SNode::enter() {
            assert(global_string_ctrie == nullptr);
            global_string_ctrie = new Ctrie;
            // TODO: global roots should be a thread-safe set?
            global.roots.push_back(global_string_ctrie);
        }
        
        void SNode::leave() {
            assert(global_string_ctrie != nullptr);
            // TODO: global roots should be a thread-safe set?
            // TODO: global.roots.erase(global_string_ctrie)
            global_string_ctrie = nullptr;
        }
        
        
        
        void TNode::debug(int lev) const {
            printf("TNode(%lx): ", this->color.load(RELAXED));
            sn->debug(lev);
        }
        
        void TNode::scan(ScanContext& context) const {
            context.push(sn);
        }
        
        std::pair<Result, const SNode*> TNode::_emplace(const INode* i, Query q, int lev, const INode* parent) const {
            clean(parent, lev - 6);
            return {RESTART, nullptr};
        }
        
        std::pair<Result, const SNode*> TNode::_erase(const INode* i, const SNode* k, int lev, const INode* parent) const {
            clean(parent, lev - 6);
            return {RESTART, nullptr};
        }
        
        const BNode* TNode::_resurrect(const INode* parent) const {
            return sn;
        }
        
        bool TNode::vcleanParentB(const INode* p, const INode* i, std::size_t hc, int lev,
                                  const MNode* m,
                                  const CNode* cn, std::uint64_t flag, int pos) const {
            const CNode* ncn = cn->updated(pos, this->sn);
            const MNode* expected = cn;
            const MNode* desired = toContracted(ncn, lev);
            return p->main.compare_exchange_strong(expected,
                                                   desired,
                                                   RELEASE,
                                                   RELAXED);
        }
        
        void TNode::_erase2(const INode* i, const SNode* k, int lev, const INode* parent) const {
            cleanParent(parent, i, k->_hash, lev - 6);
        }
        
        
        
        void LNode::debug(int lev) const {
            printf("LNode(%lx,%p): ", this->color.load(RELAXED), sn);
            if (next)
                next->debug(lev);
            else
                printf("\n");
        }
        
        void LNode::scan(ScanContext& context) const {
            context.push(sn);
            context.push(next);
        }
        
        std::pair<Result, const SNode*> LNode::lookup(Query q) const {
            const LNode* ln = this;
            for (;;) {
                if (!ln)
                    return {OK, nullptr};
                if (ln->sn->view() == q.view)
                    return {OK, ln->sn};
                ln = ln->next;
            }
        }
        
        const MNode* LNode::inserted(Query q) const {
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
                            d->sn = new (extra_val_t{q.view.size()}) SNode(q);
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
                    b->sn = new (extra_val_t{q.view.size()}) SNode(q);
                    b->next = this;
                    gc::shade(b->next);
                    return b;
                }
            }
        }
        
        std::pair<const LNode*, const SNode*> LNode::removed(const SNode* k) const {
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
        
        
        std::pair<Result, const SNode*> LNode::_emplace(const INode* i, Query q, int lev, const INode* parent) const {
            // printf("LNode %lx,%p iinsert\n", this->color.load(RELAXED), this);
            const MNode* expected = this;
            if (i->main.compare_exchange_strong(expected,
                                                inserted(q),
                                                RELEASE,
                                                RELAXED)) {
                return {OK, nullptr};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        std::pair<Result, const SNode*> LNode::_erase(const INode* i, const SNode* k, int lev, const INode* parent) const {
            const LNode* ln = this;
            auto [nln, v] = ln->removed(k);
            assert(nln && nln->sn);
            const MNode* expected = ln;
            const MNode* desired = nln->next ? nln : entomb(nln->sn);
            if (i->main.compare_exchange_strong(expected,
                                                desired,
                                                RELEASE,
                                                RELAXED)) {
                return {OK, v};
            } else {
                return {RESTART, nullptr};
            }
        }
        
        
        void CNode::debug() const {
            printf("%p gc::_string::CNode{%llx}", this, bmp);
        }
        
        void CNode::debug(int lev) const {
            lev += 6;
            printf("CNode(%lx,%#llx):\n", this->color.load(RELAXED), bmp);
            int j = 0;
            for (int i = 0; i != 64; ++i) {
                std::uint64_t flag = std::uint64_t{1} << i;
                if (bmp & flag) {
                    printf("%*s[%d]: ", lev, "", i);
                    array[j]->debug(lev);
                    j++;
                }
            }
        }
        
        void CNode::scan(ScanContext& context) const {
            int num = __builtin_popcountll(this->bmp);
            for (int i = 0; i != num; ++i) {
                this->array[i]->scan_weak(context);
            }
        }
        
        std::pair<std::uint64_t, int> CNode::flagpos(std::size_t hash, int lev, std::uint64_t bmp) {
            auto a = (hash >> lev) & 63;
            std::uint64_t flag = std::uint64_t{1} << a;
            int pos = __builtin_popcountll(bmp & (flag - 1));
            return std::pair(flag, pos);
        }
        
        const CNode* CNode::inserted(std::uint64_t flag, int pos, const BNode* child) const {
            //printf("CNode inserted\n");
            auto n = __builtin_popcountll(bmp);
            CNode* b = new (extra_val_t{sizeof(BNode*) * (n + 1)}) CNode;
            assert(!(this->bmp & flag));
            b->bmp = this->bmp | flag;
            std::memcpy(b->array, this->array, sizeof(BNode*) * pos);
            b->array[pos] = child;
            std::memcpy(b->array + pos + 1, this->array + pos, sizeof(BNode*) * (n - pos));
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n + 1; ++i)
                b->array[i]->shade_weak(context);
            return b;
        }
        
        const CNode* CNode::updated(int pos, const BNode* child) const {
            //printf("CNode updated\n");
            auto n = __builtin_popcountll(bmp);
            CNode* b = new (extra_val_t{sizeof(BNode*) * n}) CNode;
            b->bmp = this->bmp;
            std::memcpy(b->array, this->array, sizeof(BNode*) * n);
            b->array[pos] = child;
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n; ++i)
                b->array[i]->shade_weak(context);
            return b;
        }
        
        const CNode* CNode::removed(int pos, std::uint64_t flag) const {
            assert(this->bmp & flag);
            assert(__builtin_popcountll((flag - 1) & this->bmp) == pos);
            auto n = __builtin_popcountll(bmp);
            assert(pos < n);
            CNode* b = new (extra_val_t{sizeof(BNode*) * (n - 1)}) CNode;
            b->bmp = this->bmp ^ flag;
            std::memcpy(b->array, this->array, sizeof(BNode*) * pos);
            std::memcpy(b->array + pos, this->array + pos + 1, sizeof(BNode*) * (n - 1 - pos));
            ShadeContext context;
            context.WHITE = local.WHITE;
            for (int i = 0; i != n - 1; ++i)
                b->array[i]->shade_weak(context);
            return b;
        }
        
        CNode::CNode() : bmp{0} {}
        
        const CNode* CNode::make(const SNode* sn1, const SNode* sn2, int lev) {
            assert(sn1->view() != sn2->view());
            // distinct keys but potentially the same hash
            auto a1 = (sn1->_hash >> lev) & 63;
            auto a2 = (sn2->_hash >> lev) & 63;
            //printf("a1 a2 %ld %ld\n", a1, a2);
            std::uint64_t flag1 = std::uint64_t{1} << a1;
            if (a1 != a2) {
                // different hash at lev
                std::uint64_t flag2 = std::uint64_t{1} << a2;
                CNode* c = new (extra_val_t{sizeof(BNode) * 2}) CNode;
                c->bmp = flag1 | flag2;
                int pos1 = a1 > a2;
                int pos2 = a2 > a1;
                c->array[pos1] = sn1;
                c->array[pos2] = sn2;
                return c;
            } else {
                // same hash at lev
                CNode* c = new (extra_val_t{sizeof(BNode)}) CNode;
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
        
        std::pair<Result, const SNode*> CNode::_emplace(const INode* i, Query q, int lev, const INode* parent) const {
            const CNode* cn = this;
            auto [flag, pos] = flagpos(q.hash, lev, cn->bmp);
            if (!(flag & cn->bmp)) {
                const MNode* expected = this;
                const SNode* sn =  new (extra_val_t{q.view.size()}) SNode(q);
                const MNode* desired = inserted(flag, pos, sn);
                if (i->main.compare_exchange_strong(expected, desired, RELEASE, RELAXED)) {
                    return {OK, sn};
                } else {
                    return {RESTART, nullptr};
                }
            } else {
                return array[pos]->_emplace(i, q, lev, parent, cn, flag, pos);
            }
        }
        
        std::pair<Result, const SNode*> CNode::_erase(const INode* i, const SNode* k, int lev, const INode* parent) const {
            auto [flag, pos] = flagpos(k->_hash, lev, bmp);
            if (!(flag & bmp)) {
                return {OK, nullptr};
            }
            const BNode* sub = array[pos];
            assert(sub);
            auto [res, value] = sub->_erase(i, k, lev, parent, this, flag, pos);
            if (res == OK) {
                i->main.load(ACQUIRE)->_erase2(i, k, lev, parent);
            }
            return {res, value};
        }
        
        void CNode::vcleanA(const INode* i, int lev) const {
            const CNode* m = this;
            const MNode* expected = m;
            const MNode* desired = toCompressed(m, lev);
            i->main.compare_exchange_strong(expected, desired, RELEASE, RELAXED);
        }
        
        bool CNode::vcleanParentA(const INode* p, const INode* i, std::size_t hc, int lev,
                                  const MNode* m) const {
            const CNode* cn = this;
            auto [flag, pos] = flagpos(hc, lev, this->bmp);
            if (!(flag & bmp))
                return true;
            const BNode* sub = this->array[pos];
            if (sub != i)
                return true;
            return m->vcleanParentB(p, i, hc, lev, m, cn, flag, pos);
        }
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        std::pair<Result, const SNode*> iinsert(const INode* i, Query q, int lev, const INode* parent) {
            return i->main.load(ACQUIRE)->_emplace(i, q, lev, parent);
        }
        
        
        
        
        
        std::pair<Result, const SNode*> iremove(const INode* i, const SNode* k, int lev, const INode* parent) {
            return i->main.load(ACQUIRE)->_erase(i, k, lev, parent);
        }
        
        
        
        
        
        
        
        Ctrie::Ctrie()
        : root(new INode(new CNode)) {
        }
        
        void Ctrie::debug() {
            printf("%p: Ctrie\n", this);
            root->debug(0);
        }
        
        void Ctrie::scan(ScanContext& context) const {
            context.push(root);
        }
        
        const SNode* Ctrie::emplace(Query q) {
            for (;;) {
                INode* r = root;
                auto [res, v2] = iinsert(r, q, 0, nullptr);
                if (res == RESTART)
                    continue;
                assert(v2);
                return v2;
            }
        }
        
        const SNode* Ctrie::remove(const SNode* k) {
            for (;;) {
                INode* r = root;
                auto [res, v] = iremove(r, k, 0, nullptr);
                if (res == RESTART)
                    continue;
                return v;
            }
        }
        
        void SNode::debug() const {
            printf("%p gc::String{%zx,%zd,\"%.*s\"}\n", this, _hash, _size, (int)_size, _data);
        }

    } // namespace _string
    
} // namespace gc
