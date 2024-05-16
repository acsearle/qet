//
//  ctrie.hpp
//  gch
//
//  Created by Antony Searle on 20/4/2024.
//

#ifndef ctrie_hpp
#define ctrie_hpp

#include "gc.hpp"

namespace gc {
    
    namespace _ctrie {
        
        // Todo:
        
        // To support HashSet and HashMap equally we need to store an opaque
        // thing in SNode, and access it through Hash and KeyEqual
        
        // To support more general-than-key lookup, we need to specify the
        // virtual interfaces in terms of some general Query type
        
        // SNode doesn't actually need any table-specific members, so it's a
        // benign overhead for any HashSet (or WeakHashSet) of Objects to just
        // inherit from such an SNode interface and have the nodes be directly
        // in the HashSet (so long as the Key part is immutable)
        
        // So, the fundamental interface could well be in terms of
        // SNode type
        // GetOp type -- hash and keycmp
        // SetOp type -- hash and keycmp and emplace / assign
        
        // Operations --
        // find, erase - hash and predicate
        // erase_if - hash and extra predicate
        // insert, insert_or_assign - hash and predicate and value_type
        // emplace - hash and predicate and constructor args
        //
        // In fact, just the hash will get us to either
        // - a CNode* without the hash
        // - an SNode* with the same hash
        // - an LNode* with the same hash
        
        // And from such a handle we can do whatever
        // LNode behavior dictates how multisets are handled
        // In the multiset case hash collisions are expected and should be
        // eagerly checked for rather than exhausting the hash bits first
        
        
        template<typename Key, typename T, typename Hash, typename KeyEqual>
        struct Ctrie : gc::Object {
            
            using key_type = Key;
            using mapped_type = T;
            using value_type = std::pair<const Key, T>;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using hasher = Hash;
            
            struct ANode; // Any       : Object
            struct BNode; // Branch    : Any
            struct MNode; // Main      : Any
            struct INode; // Indirect  : Branch
            struct SNode; // Singleton : Branch
            struct CNode; // Ctrie     : Main
            struct LNode; // List      : Main
            struct TNode; // Tomb      : Main
            
            enum Tag {
                NOTFOUND = -1,
                RESTART = 0,
                OK = 1,
            };
            
            struct Result {
                Tag tag;
                union {
                    T value;
                };
            };
            
            static Result Ok(T value) {
                return Result{OK, value};
            }
            
            static Result Restart() {
                return Result{RESTART};
            }
            
            static Result NotFound() {
                return Result{NOTFOUND};
            }
            
            struct ANode : Object {
                virtual void debug(int lev) const = 0;
            };
            
            struct BNode : ANode {
                virtual Result _find(INode* i, Key k, int lev, INode* parent,
                                     CNode* cn, std::uint64_t flag, int pos) const = 0;
                virtual Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent,
                                                 CNode* cn, std::uint64_t flag, int pos) const = 0;
                virtual Result _erase(INode* i, Key k, int lev, INode* parent,
                                      CNode* cn, std::uint64_t flag, int pos) const  = 0;
                virtual BNode* _resurrect() const = 0;
                virtual MNode* _contract(CNode* parent, int lev) const = 0;
            }; // struct BNode
            
            struct MNode : ANode {
                
                virtual Result _find(INode* i, Key k, int lev, INode* parent) const = 0;
                virtual Result _insert_or_assign(INode* i, Key k, T value, int lev, INode* parent) const = 0;
                virtual Result _erase(INode* i, Key k, int lev, INode* parent) const = 0;
                virtual void _erase2(INode* i, Key k, int lev, INode* parent) const {}
                virtual BNode* _resurrect(INode* parent) const { return parent; }
                virtual void vcleanA(INode* i, int lev) const {}
                virtual bool vcleanParentA(INode* p, INode* i, std::size_t hc, int lev,
                                           MNode* m) const { return true; }
                virtual bool vcleanParentB(INode* p, INode* i, std::size_t hc, int lev,
                                           MNode* m,
                                           CNode* cn, std::uint64_t flag, int pos) const { return true; }
            }; // struct MNode
            
            struct INode : BNode {
                mutable Atomic<StrongPtr<MNode>> main;
                explicit INode(MNode* desired) : main(desired) {}
                virtual void debug(int lev) const override {
                    auto p =  main.load(ACQUIRE);
                    printf("INode(%lx): ",this->color.load(RELAXED));
                    p->debug(lev);
                }
                virtual void scan(ScanContext& context) const override {
                    context.push(main);
                }
                virtual Result _find(INode* i, Key k, int lev, INode* parent,
                                     CNode* cn, std::uint64_t flag, int pos) const override {
                    return Ctrie::_find(this, k, lev + 6, i);
                }
                virtual Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent,
                                                 CNode* cn, std::uint64_t flag, int pos) const override {
                    return Ctrie::_insert_or_assign(this, k, v, lev + 6, i);
                }
                virtual Result _erase(INode* i, Key k, int lev, INode* parent,
                                      CNode* cn, std::uint64_t flag, int pos) const override{
                    return Ctrie::_erase(this, k, lev + 6, i);
                }
                virtual BNode* _resurrect() const override {
                    return this->main.load(ACQUIRE)->_resurrect(this);
                }
                virtual MNode* _contract(CNode* cn, int lev) const override {
                    return cn;
                }
                
                
            }; // struct INode
            
            struct SNode : BNode {
                Key key;
                T value;
                SNode(Key k, T v) : key(k), value(v) {}
                virtual void shade(ShadeContext& context) const override {
                    Color expected = context.WHITE;
                    color.compare_exchange_strong(expected,
                                                  context.BLACK(),
                                                  RELAXED,
                                                  RELAXED);
                }
                virtual void debug(int lev) const override {
                    printf("SNode(%lx)\n", this->color.load(RELAXED));
                }
                virtual Result _find(INode* i, Key k, int lev, INode* parent,
                                     CNode* cn, std::uint64_t flag, int pos) const override {
                    if (this->key == k) {
                        return Ok(value);
                    } else {
                        return NotFound();
                    }
                }
                virtual Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent,
                                                 CNode* cn, std::uint64_t flag, int pos) const override {
                    SNode* nsn = new SNode(k, v);
                    CNode* ncn;
                    if (this->key != k) {
                        INode* nin = new INode(CNode::make(this, nsn, lev + 6));
                        ncn = cn->updated(pos, nin);
                    } else {
                        ncn = cn->updated(pos, nsn);
                    }
                    MNode* expected = cn;
                    if (i->main.compare_exchange_strong(expected,
                                                        ncn,
                                                        RELEASE,
                                                        RELAXED)) {
                        return Ok(value);
                    } else {
                        return Restart();
                    }
                }
                
                virtual Result _erase(INode* i, Key k, int lev, INode* parent,
                                      CNode* cn, std::uint64_t flag, int pos) const override  {
                    if (this->key != k)
                        return NotFound();
                    CNode* ncn = cn->removed(pos, flag);
                    MNode* cntr = toContracted(ncn, lev);
                    MNode* expected = cn;
                    if (i->main.compare_exchange_strong(expected,
                                                        cntr,
                                                        RELEASE,
                                                        RELAXED)) {
                        return Ok(value);
                    } else {
                        return Restart();
                    }
                }
                
                virtual BNode* _resurrect() const override {
                    return this;
                }
                virtual MNode* _contract(CNode* cn, int lev) const override {
                    SNode* sn = this;
                    return entomb(sn);
                }
                
            }; // struct SNode
            
            struct TNode : MNode {
                SNode* sn;
                
                virtual void debug(int lev) const override {
                    printf("TNode(%lx): ", this->color.load(RELAXED));
                    sn->debug(lev);
                }
                virtual void scan(ScanContext& context) const override {
                    context.push(sn);
                }
                virtual Result _find(INode* i, Key k, int lev, INode* parent) const override {
                    clean(parent, lev - 6);
                    return Restart();
                }
                virtual Result _insert_or_assign(INode* i, Key k, T value, int lev, INode* parent) const override {
                    clean(parent, lev - 6);
                    return Restart();
                }
                virtual Result _erase(INode* i, Key k, int lev, INode* parent) const override {
                    clean(parent, lev - 6);
                    return Restart();
                }
                virtual void _erase2(INode* i, Key k, int lev, INode* parent) const override {
                    cleanParent(parent, i, Hash{}(k), lev - 6);
                }
                virtual bool vcleanParentB(INode* p, INode* i, std::size_t hc, int lev,
                                           MNode* m,
                                           CNode* cn, std::uint64_t flag, int pos) const override {
                    CNode* ncn = cn->updated(pos, this->sn);
                    MNode* expected = cn;
                    MNode* desired = toContracted(ncn, lev);
                    return p->main.compare_exchange_strong(expected,
                                                           desired,
                                                           RELEASE,
                                                           RELAXED);
                }
                virtual BNode* _resurrect(INode* parent) const override {
                    return sn;
                }
            }; // struct TNode
            
            struct LNode : MNode {
                SNode* sn;
                const LNode* next;
                virtual void debug(int lev) const override {
                    printf("LNode(%lx,%p): ", this->color.load(RELAXED), sn);
                    if (next)
                        next->debug(lev);
                    else
                        printf("\n");
                }
                virtual void scan(ScanContext& context) const override {
                    context.push(sn);
                    context.push(next);
                }
                Result lookup(Key k) const {
                    const LNode* ln = this;
                    for (;;) {
                        if (!ln)
                            return NotFound();
                        if (ln->sn->key == k)
                            return Ok(ln->sn->value);
                        ln = ln->next;
                    }
                }
                MNode* inserted(Key k, T v) const  {
                    const LNode* a = this;
                    for (;;) {
                        if (a->sn->key == k) {
                            // The key exists in the list, so we must copy the
                            // first part, replace that node, and point to the
                            // second part
                            const LNode* b = this;
                            LNode* c = new LNode;
                            LNode* d = c;
                            for (;;) {
                                if (b != a) {
                                    d->sn = b->sn;
                                    gc::shade(d->sn);
                                    b = b->next;
                                    LNode* e = new LNode;
                                    d->next = e;
                                    d = e;
                                } else {
                                    d->sn = new SNode(k, v);
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
                            b->sn = new SNode(k, v);
                            b->next = this;
                            gc::shade(b->next);
                            return b;
                        }
                    }
                }
                std::pair<const LNode*, T> removed(Key k) const {
                    if (this->sn->key == k)
                        return {this->next, this->sn->value};
                    const LNode* a = this->next;
                    for (;;) {
                        if (a == nullptr) {
                            // Not found at all
                            return {this, nullptr};
                        }
                        if (a->sn->key != k) {
                            // Not found yet
                            a = a->next;
                        } else {
                            // Found inside the list
                            const LNode* b = this;
                            LNode* c = new LNode; // make new head
                            LNode* d = c;
                            for (;;) {
                                d->sn = b->sn; // copy over node
                                gc::shade(d->sn);
                                b = b->next;
                                if (b == a) {
                                    // we've reached the node we are erasing, skip
                                    // over it
                                    d->next = a->next;
                                    gc::shade(d->next);
                                    return {c, a->sn->value};
                                }
                                LNode* e = new LNode;
                                d->next = e;
                                d = e;
                            }
                        }
                    }
                }
                
                virtual Result _find(INode* i, Key k, int lev, INode* parent) const override {
                    return lookup(k);
                }
                virtual Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent) const override {
                    // printf("LNode %lx,%p iinsert\n", this->color.load(RELAXED), this);
                    MNode* expected = this;
                    if (i->main.compare_exchange_strong(expected,
                                                        inserted(k, v),
                                                        RELEASE,
                                                        RELAXED)) {
                        return NotFound(); // but, success
                    } else {
                        return Restart();
                    }
                }
                virtual Result _erase(INode* i, Key k, int lev, INode* parent) const override {
                    const LNode* ln = this;
                    auto [nln, v] = ln->removed(k);
                    assert(nln && nln->sn);
                    MNode* expected = ln;
                    MNode* desired = nln->next ? nln : entomb(nln->sn);
                    if (i->main.compare_exchange_strong(expected,
                                                        desired,
                                                        RELEASE,
                                                        RELAXED)) {
                        return Ok(v);
                    } else {
                        return Restart();
                    }
                }
            }; // struct LNode
            
            struct CNode : MNode {
                
                std::uint64_t bmp;
                BNode* array[0];
                
                static std::pair<std::uint64_t, int> flagpos(std::size_t hash, int lev, std::uint64_t bmp) {
                    auto a = (hash >> lev) & 63;
                    std::uint64_t flag = std::uint64_t{1} << a;
                    int pos = __builtin_popcountll(bmp & (flag - 1));
                    return std::pair(flag, pos);
                }
                
                static CNode* make(SNode* sn1, SNode* sn2, int lev) {
                    assert(sn1->key != sn2->key);
                    // distinct keys but potentially the same hash
                    auto a1 = (Hash{}(sn1->key) >> lev) & 63;
                    auto a2 = (Hash{}(sn2->key) >> lev) & 63;
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
                        // TODO: check for exact hash collision immediately and
                        // install an LNode instead
                        // But: if this happens enough to matter then we have
                        // a bad hash function
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
                
                CNode() : bmp{0} {}
                
                virtual void debug(int lev) const override {
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
                virtual void scan(ScanContext& context) const override  {
                    int num = __builtin_popcountll(this->bmp);
                    for (int i = 0; i != num; ++i) {
                        this->array[i]->scan_weak(context);
                    }
                }
                
                CNode* inserted(std::uint64_t flag, int pos, BNode* child) const {
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
                        b->array[i]->shade(context);
                    return b;
                }
                
                CNode* updated(int pos, BNode* child) const {
                    //printf("CNode updated\n");
                    auto n = __builtin_popcountll(bmp);
                    CNode* b = new (extra_val_t{sizeof(BNode*) * n}) CNode;
                    b->bmp = this->bmp;
                    std::memcpy(b->array, this->array, sizeof(BNode*) * n);
                    b->array[pos] = child;
                    ShadeContext context;
                    context.WHITE = local.WHITE;
                    for (int i = 0; i != n; ++i)
                        b->array[i]->shade(context);
                    return b;
                }
                
                CNode* removed(int pos, std::uint64_t flag) const {
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
                
                virtual Result _find(INode* i, Key k, int lev, INode* parent) const override {
                    CNode* cn = this;
                    auto [flag, pos] = flagpos(Hash{}(k), lev, cn->bmp);
                    if (!(flag & cn->bmp)) {
                        return {OK, nullptr};
                    } else {
                        return array[pos]->_find(i, k, lev, parent, cn, flag, pos);
                    }
                }
                
                virtual Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent) const override {
                    CNode* cn = this;
                    auto [flag, pos] = flagpos(Hash{}(k), lev, cn->bmp);
                    if (!(flag & cn->bmp)) {
                        MNode* expected = this;
                        SNode* sn =  new SNode(k, v);
                        MNode* desired = inserted(flag, pos, sn);
                        if (i->main.compare_exchange_strong(expected, desired, RELEASE, RELAXED)) {
                            return Ok(sn->value);
                        } else {
                            return Restart();
                        }
                    } else {
                        return array[pos]->_insert_or_assign(i, k, v, lev, parent, cn, flag, pos);
                    }
                }
                
                virtual Result _erase(INode* i, Key k, int lev, INode* parent) const override {
                    auto [flag, pos] = flagpos(Hash{}(k), lev, bmp);
                    if (!(flag & bmp)) {
                        return NotFound();
                    }
                    BNode* sub = array[pos];
                    assert(sub);
                    Result result = sub->_erase(i, k, lev, parent, this, flag, pos);
                    if (result.tag == OK) {
                        i->main.load(ACQUIRE)->_erase2(i, k, lev, parent);
                    }
                    return result;
                }
                
                virtual void vcleanA(INode* i, int lev) const override {
                    CNode* m = this;
                    MNode* expected = m;
                    MNode* desired = toCompressed(m, lev);
                    i->main.compare_exchange_strong(expected, desired, RELEASE, RELAXED);
                }
                
                virtual bool vcleanParentA(INode* p, INode* i, std::size_t hc, int lev,
                                           MNode* m) const override {
                    CNode* cn = this;
                    auto [flag, pos] = flagpos(hc, lev, this->bmp);
                    if (!(flag & bmp))
                        return true;
                    BNode* sub = this->array[pos];
                    if (sub != i)
                        return true;
                    return m->vcleanParentB(p, i, hc, lev, m, cn, flag, pos);
                }
                
            }; // struct CNode
            
            static BNode* resurrect(BNode* m) {
                return m->_resurrect();
            }
            
            static MNode* toCompressed(CNode* cn, int lev) {
                int num = __builtin_popcountll(cn->bmp);
                CNode* ncn = new (extra_val_t{sizeof(BNode*) * num}) CNode;
                ncn->bmp = cn->bmp;
                ShadeContext context;
                context.WHITE = local.WHITE;
                for (int i = 0; i != num; ++i) {
                    ncn->array[i] = resurrect(cn->array[i]);
                    ncn->array[i]->shade(context);
                }
                return toContracted(ncn, lev);
            }
            
            static MNode* toContracted(CNode* cn, int lev) {
                int num = __builtin_popcountll(cn->bmp);
                if (lev == 0 || num > 1)
                    return cn;
                return cn->array[0]->_contract(cn, lev);
            }
            
            static void clean(INode* i, int lev) {
                i->main.load(ACQUIRE)->vcleanA(i, lev);
            }
            
            static void cleanParent(INode* p, INode* i, std::size_t hc, int lev) {
                for (;;) {
                    MNode* m = i->main.load(ACQUIRE); // <-- TODO we only redo this if it is a TNode and therefore final
                    MNode* pm = p->main.load(ACQUIRE); // <-- TODO get this from the failed CAS
                    if (pm->vcleanParentA(p, i, hc, lev, m))
                        return;
                }
            }
            
            static MNode* entomb(SNode* sn) {
                TNode* tn = new TNode;
                tn->sn = sn;
                gc::shade(sn);
                return tn;
            }
            
            static Result _find(INode* i, Key k, int lev, INode* parent) {
                return i->main.load(ACQUIRE)->_find(i, k, lev, parent);
            }
            
            static Result _insert_or_assign(INode* i, Key k, T v, int lev, INode* parent) {
                return i->main.load(ACQUIRE)->_insert_or_assign(i, k, v, lev, parent);
            }
            
            static Result _erase(INode* i, Key k, int lev, INode* parent) {
                return i->main.load(ACQUIRE)->_erase(i, k, lev, parent);
            }
            
            Ctrie() : root(new INode(new CNode)) {
            }
            
            void debug() {
                printf("%p: Ctrie\n", this);
                root->debug(0);
            }
            
            void _gc_scan(ScanContext& context) const {
                context.push(root);
            }
            
            T find(Key k) {
                for (;;) {
                    INode* r = root;
                    Result result = _find(r, k, 0, nullptr);
                    if (result.tag == RESTART)
                        continue;
                    return result.value;
                }
            }
            
            T insert_or_assign(Key k, T v) {
                for (;;) {
                    INode* r = root;
                    Result result = _insert_or_assign(r, k, v, 0, nullptr);
                    if (result.tag == RESTART)
                        continue;
                    return result.value;
                }
            }
            
            T erase(Key k) {
                for (;;) {
                    INode* r = root;
                    Result result = _erase(r, k, 0, nullptr);
                    if (result.tag == RESTART)
                        continue;
                    return result.value;
                }
            }
            
            INode* root;
            
            
            
            
            
            
        }; // Ctrie
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
    } // _ctrie
    
    using _ctrie::Ctrie;
    
} // namespace gc

#endif /* ctrie_hpp */
