//
//  string.hpp
//  gch
//
//  Created by Antony Searle on 20/4/2024.
//

#ifndef string_hpp
#define string_hpp

#include <cassert>
#include <string_view>

#include "gc.hpp"
#include "object.hpp"

namespace gc {
    
    namespace _string {
        
        struct Query {
            
            // a char sequence and its hash
            
            // libc++ std::hash<std::string_view> seems to be CityHash64 which
            // is perfectly adequate
            
            std::string_view view;
            std::size_t hash;
            
            bool invariant() {
                return hash == std::hash<std::string_view>()(view);
            }
            
            void debug() const {
                printf("Query{\"%.*s\",%zx}",
                       (int)view.size(), view.data(),
                       hash);
            }
            
            Query(std::string_view v, std::size_t h)
            : view(v), hash(h) {
                assert(invariant());
            }
            
            explicit Query(std::string_view v)
            : Query(v, std::hash<std::string_view>()(v)) {
            }
            
            Query()
            : Query(std::string_view()) {
            }
            
        };
        
        enum Result {
            RESTART = 0,
            OK = 1,
        };
        
        struct ANode; // Any       : Object
        struct BNode; // Branch    : Any
        struct MNode; // Main      : Any
        struct INode; // Indirect  : Branch
        struct SNode; // Singleton : Branch
        struct CNode; // Ctrie     : Main
        struct LNode; // List      : Main
        struct TNode; // Tomb      : Main
        
        struct ANode : lox::Object {
            virtual void debug() const override = 0;
            virtual void debug(int lev) const = 0;
            
            virtual void printObject() override { printf("gc::_string::ANode"); }
        };
        
        struct BNode : ANode {
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent,
                                                             CNode* cn, std::uint64_t flag, int pos) = 0;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent,
                                                           CNode* cn, std::uint64_t flag, int pos)  = 0;
            virtual BNode* _resurrect() = 0;
            virtual MNode* _contract(CNode* parent, int lev) = 0;
            virtual void debug(int lev) const override = 0;
            virtual void debug() const override = 0;
            virtual void printObject() override { printf("gc::_string::BNode"); }

        }; // struct BNode
        
        struct SNode : BNode {
            
            using Query = Query;
            
            // class methods
            
            static void enter();
            static void leave();
            
            static SNode* make(Query q);
            static SNode* make(std::string_view v);
            static SNode* make(const char*);
            static SNode* make(const char*, const char*);
            static SNode* make(const char*, std::size_t);
            static SNode* make(char);

            // instance methods
            
            bool invariant() const {
                return ((color.load(RELAXED) != GRAY) &&
                        (_hash == std::hash<std::string_view>()(view())));
            }
            
            SNode() = delete;
            ~SNode() = default;
            SNode(SNode&) = delete;
            SNode(SNode&&) = delete;
            SNode& operator=(SNode&) = delete;
            SNode& operator=(SNode&&) = delete;
            
            explicit SNode(Query);
            
            // GC methods
            
            virtual void shade(ShadeContext&) const override;
            virtual void scan(ScanContext& context) const override;
            virtual Color sweep(SweepContext&) override;
            virtual void shade_weak(ShadeContext&) const override;
            virtual void scan_weak(ScanContext&) const override;
            
            // Ctrie node methods
            
            virtual void debug(int lev) const override;
            virtual std::pair<Result, SNode*> _emplace(INode* i, Query q, int lev, INode* parent,
                                                             CNode* cn, std::uint64_t flag, int pos) override;
            virtual std::pair<Result, SNode*> _erase(INode* i, SNode* k, int lev, INode* parent,
                                                           CNode* cn, std::uint64_t flag, int pos) override;
            virtual BNode* _resurrect() override ;
            virtual MNode* _contract(CNode* cn, int lev) override;
            
            std::size_t _hash;
            std::size_t _size;
            char _data[0];
            
            virtual void debug() const override;
            virtual void printObject() override { // printf("gc::_string::SNode");
                printf("%.*s", (int) _size, _data);
            }

            
            // string methods
            
            std::string_view view() const {
                return std::string_view(_data, _size);
            }
            
            // std::unordered_map interop
            // we can use the string hash and support query by string_view
            // or we can use the pointer itself and rely on identity, which
            // requires string_view lookup to be indirected through the
            // interning process
            
            struct Hash {
                using is_transparent = void;
                std::size_t operator()(SNode const* const& k) const {
                    return k->_hash;
                }
                std::size_t operator()(Query q) const {
                    return q.hash;
                }
            }; // struct Hash
            
            struct KeyEqual {
                using is_transparent = void;
                bool operator()(SNode* a, SNode* b) const {
                    // A hash table should only be checking equality when the hashes are equal
                    assert(a->_hash == b->_hash);
                    // equivalence implies identity
                    assert((a->view() == b->view()) == (a == b));
                    return a == b;
                }
                bool operator()(SNode* a, Query b) const {
                    // A hash table should only be checking equality when the hashes are equal
                    assert(a->_hash == b.hash);
                    return a->view() == b.view;
                }
            }; // struct KeyEqual
            
        }; // struct SNode
        
    } // namespace _string
    
    // using String = _string::SNode;
    //using ObjectString = _string::SNode;
    
} // namespace gc

#endif /* string_hpp */
