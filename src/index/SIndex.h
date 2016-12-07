
#ifndef SINDEX_H
#define SINDEX_H
#define HASH_RES_t size_t
#define DEFAULT_CHUNK_SIZE 32
#define DEFAULT_LIST_SIZE 8

#define INSERT_INTO_MMAP 1
#define DELETE_FROM_MMAP -1
#define  FORCE_INLINE __attribute__((always_inline))
#include "allocator/Store.h"

#include <unordered_set>

template<typename K, typename ENTRY>
class Index {
public:

    virtual void add(ENTRY* obj, uint8_t tid) = 0;

    virtual void add(ENTRY* obj, const HASH_RES_t h, uint8_t tid) = 0;



    virtual void del(const ENTRY* obj, uint8_t tid) = 0;

    virtual void del(const ENTRY* obj, const HASH_RES_t h, uint8_t tid) = 0;


    virtual size_t count() = 0;

    virtual ~Index() {
    }
};

template<typename K, typename ENTRY, typename IDX_FN /* = GenericIndexFn<T>*/, bool is_unique = false >
class HashIndex : public Index<K, ENTRY> {
public:
    std::string name;

    typedef struct __IdxNode {
        HASH_RES_t hash;
        ENTRY *obj;
        struct __IdxNode* nxt;
    } IdxNode; //  the linked list is maintained 'compactly': if a IdxNode has a nxt, it is full.
    IdxNode* buckets_;
    size_t size_;
private:
    Store<IdxNode> nodes_;

    size_t count_, threshold_;
    double load_factor_;
    const ENTRY* zero;

    void resize_(size_t new_size, uint8_t tid) {
        IdxNode *old = buckets_, *n, *na, *nw, *d;
        HASH_RES_t h;
        size_t sz = size_;
        buckets_ = new IdxNode[new_size];
        memset(buckets_, 0, sizeof (IdxNode) * new_size);
        size_ = new_size;
        threshold_ = size_ * load_factor_;
        for (size_t b = 0; b < sz; ++b) {
            n = &old[b];
            bool pooled = false;
            do {
                if (n->obj) { //add_(n->obj); // does not resize the bucket array, does not maintain count
                    h = n->hash;
                    na = &buckets_[h % size_];
                    if (na->obj) {

                        nw = nodes_.add(); //memset(nw, 0, sizeof(IdxNode)); // add a node
                        //in addition to adding nw, we also change the place of nw with na
                        //to preserve the order of elements, as it is required for
                        //non-unique hash maps
                        nw->hash = na->hash;
                        nw->obj = na->obj;
                        na->hash = h;
                        na->obj = n->obj;
                        nw->nxt = na->nxt;
                        na->nxt = nw;
                    } else { // space left in last IdxNode
                        na->hash = h;
                        na->obj = n->obj; //na->nxt=nullptr;
                    }
                }
                if (pooled) {
                    d = n;
                    n = n->nxt;
                    nodes_.remove(d, tid);
                } else n = n->nxt;
                pooled = true;
            } while (n);
        }

        if (old) delete[] old;
    }

public:

    size_t findMaxPerBucket() {
        size_t max = 0;
        for (size_t i = 0; i < size_; i++) {
            size_t buck_count = 0;
            IdxNode* n = &buckets_[i];
            while (n != nullptr) {

                ++buck_count;
                if (is_unique)
                    n = n->nxt;
                else {
                    size_t h = n->hash;
                    const K& key = n->obj->key;
                    //                    std::cout<<"first key is "<<key<<std::endl;
                    while ((n = n->nxt) && (h == n->hash) && IDX_FN::equals(key, n->obj->key)) {
                        //                        std::cout<<n->obj->key<<std::endl;
                    }
                    //                    exit(1);
                }
            }
            if (buck_count > max) {
                max = buck_count;
            }

        }
        return max;
    }

    HashIndex(std::string n, size_t size, size_t storesize, double load_factor, uint8_t numT) : nodes_(storesize, n + " Index", numT), zero(nullptr) {
        load_factor_ = load_factor;
        size_ = 0;
        count_ = 0;
        buckets_ = nullptr;
        resize_(size);
        name = n;

    }

    HashIndex(std::string n = "N/A", size_t size = DEFAULT_CHUNK_SIZE, double load_factor = .75, uint8_t numT) : nodes_(size / 10, n + " Index", numT), zero(nullptr) {
        load_factor_ = load_factor;
        size_ = 0;
        count_ = 0;
        buckets_ = nullptr;
        resize_(size);
        name = n;

    }

    ~HashIndex() {
        if (buckets_ != nullptr) delete[] buckets_;
    }

    ENTRY& operator[](const K& key) {
        return *get(key);
    }

    // retrieves the first element equivalent to the key or nullptr if not found

    inline ENTRY* get(const K& key) const {
        HASH_RES_t h = IDX_FN::hash(key);
        IdxNode* n = &buckets_[h % size_];
        do {
            if (n->obj && h == n->hash && IDX_FN::equals(key, n->obj->key)) return n->obj;
        } while ((n = n->nxt));
        return nullptr;
    }

    inline ENTRY* get(const K& key, const HASH_RES_t h) const {
        IdxNode* n = &buckets_[h % size_];
        do {
            if (n->obj && h == n->hash && IDX_FN::equals(key, n->obj->key)) return n->obj;
        } while ((n = n->nxt));
        return nullptr;
    }

    // inserts regardless of whether element exists already

    FORCE_INLINE virtual void add(ENTRY* obj, uint8_t tid) {
        HASH_RES_t h = IDX_FN::hash(obj->key);
        add(obj, h, tid);
    }

    FORCE_INLINE virtual void add(ENTRY* obj, const HASH_RES_t h, uint8_t tid) {
        if (count_ > threshold_) {
            std::cerr << name << "  Index resize count=" << count_ << "  size=" << size_ << std::endl;
            exit(1);
            //resize_(size_ << 1);
        }
        size_t b = h % size_;
        IdxNode* n = &buckets_[b];
        IdxNode* nw;

        if (is_unique) {
            ++count_;
            if (n->obj) {
                nw = nodes_.add(tid); //memset(nw, 0, sizeof(IdxNode)); // add a node
                nw->hash = h;
                nw->obj = obj;
                nw->nxt = n->nxt;
                n->nxt = nw;
            } else { // space left in last IdxNode
                n->hash = h;
                n->obj = obj; //n->nxt=nullptr;
            }
        } else {
            // ++count_;
            if (!n->obj) { // space left in last IdxNode
                ++count_;
                n->hash = h;
                n->obj = obj; //n->nxt=nullptr;
                return;
            }
            do {
                if (h == n->hash && IDX_FN::equals(obj->key, n->obj->key)) {
                    nw = nodes_.add(tid); //memset(nw, 0, sizeof(IdxNode)); // add a node
                    nw->hash = h;
                    nw->obj = obj;
                    nw->nxt = n->nxt;
                    n->nxt = nw;
                    return;
                }/*else {
          //go ahead, and look for an element in the same slice
          //or reach the end of linked list of IdxNodes
        }*/
            } while ((n = n->nxt));
            // if(!n) {
            ++count_;
            n = &buckets_[b];
            nw = nodes_.add(tid); //memset(nw, 0, sizeof(IdxNode)); // add a node
            //in addition to adding nw, we also change the place of nw with n
            //to preserve the order of elements, as it is required for
            //non-unique hash maps (as the first element might be the start of)
            //a chain of non-unique elements belonging to the same slice
            nw->hash = n->hash;
            nw->obj = n->obj;
            n->hash = h;
            n->obj = obj;
            nw->nxt = n->nxt;
            n->nxt = nw;
            // return;
            // }
        }
    }

    // deletes an existing elements (equality by pointer comparison)

    FORCE_INLINE virtual void del(const ENTRY* obj, uint8_t tid) {
        HASH_RES_t h = IDX_FN::hash(obj->key);
        del(obj, h, tid);
    }

    FORCE_INLINE virtual void del(const ENTRY* obj, const HASH_RES_t h, uint8_t tid) {
        IdxNode *n = &buckets_[h % size_];
        IdxNode *prev = nullptr, *next; // previous and next pointers
        do {
            next = n->nxt;
            if (/*n->obj &&*/ n->obj == obj) { //we only need a pointer comparison, as all objects are stored in the pool
                if (prev) { //it is an element in the linked list (and not in the bucket itself)
                    prev->nxt = next;
                    // n->nxt = nullptr;
                    // n->obj = nullptr;
                    nodes_.remove(n, tid);
                } else if (next) { //it is the elements in the bucket, and there are other elements in linked list
                    n->obj = next->obj;
                    n->hash = next->hash;
                    n->nxt = next->nxt;
                    nodes_.remove(next, tid);
                    next = n;
                } else { //it is the only element in the bucket
                    n->obj = nullptr;
                }
                if (is_unique || !((prev && prev->obj && (h == prev->hash) && IDX_FN::equals(obj->key, prev->obj->key)) ||
                        (next && next->obj && (h == next->hash) && IDX_FN::equals(obj->key, next->obj->key)))) --count_;
                return;
            }
            prev = n;
        } while ((n = next));
    }

    inline void slice(const K& key, std::function<void (ENTRY*) > f) {
        HASH_RES_t h = IDX_FN::hash(key);
        IdxNode* n = &(buckets_[h % size_]);

        do {

            if (n->obj && h == n->hash && IDX_FN::equals(key, n->obj->key)) {

                do {
                    f(n->obj);
                } while ((n = n->nxt) && h == n->hash && IDX_FN::equals(key, n->obj->key));


                return;
            }
        } while ((n = n->nxt));
    }

    FORCE_INLINE virtual size_t count() {
        return count_;
    }

    class iterator {
    private:
        IdxNode *n;
        HASH_RES_t h;
        K key;
    public:

        iterator(IdxNode* ptr, HASH_RES_t hash, const K& k) : n(ptr), h(hash), key(k) {
        }

        iterator() : n(nullptr) {
        }

        ENTRY* incr_get() {
            if ((n = n->nxt) && h == n->hash && IDX_FN::equals(key, n->obj->key))
                return n->obj;
            else
                return nullptr;
        }

        ENTRY* get() {
            if (n)
                return n->obj;
            else
                return nullptr;
        };

        ~iterator() {
        }
    };

    inline iterator slice(const K& key) {
        HASH_RES_t h = IDX_FN::hash(key);
        IdxNode* n = &(buckets_[h % size_]);

        do {
            if (n->obj && h == n->hash && IDX_FN::equals(key, n->obj->key)) {
                return iterator(n, h, key);
            }
        } while ((n = n->nxt));
        return iterator();
    }
};

template<typename K, typename ENTRY, typename IDX_FN /* = GenericIndexFn<T>*/>
class TreeIndex : public Index<K, ENTRY> {
public:
    std::string name;

    typedef struct __IdxTreeNode {
        unsigned char height;
        ENTRY *obj;
        struct __IdxTreeNode *parent, *left, *right;
    } IdxEquivNode;

    typedef struct __IdxNode {
        HASH_RES_t hash;
        IdxEquivNode* equivNodes;
        struct __IdxNode* nxt;
    } IdxNode; //  the linked list is maintained 'compactly': if a IdxNode has a nxt, it is full.
    IdxNode* buckets_;
    size_t size_;
private:
    const bool is_unique = false;
    Store<IdxEquivNode> equiv_nodes_;
    Store<IdxNode> nodes_;

    size_t count_, threshold_;
    double load_factor_;

    void resize_(size_t new_size, uint8_t tid) {
        IdxNode *old = buckets_, *n, *na, *nw, *d;
        HASH_RES_t h;
        size_t sz = size_;
        buckets_ = new IdxNode[new_size];
        memset(buckets_, 0, sizeof (IdxNode) * new_size);
        size_ = new_size;
        threshold_ = size_ * load_factor_;
        for (size_t b = 0; b < sz; ++b) {
            n = &old[b];
            bool pooled = false;
            do {
                if (n->equivNodes) { //add_(n->obj); // does not resize the bucket array, does not maintain count
                    h = n->hash;
                    na = &buckets_[h % size_];
                    if (na->equivNodes) {

                        nw = nodes_.add(tid); //memset(nw, 0, sizeof(IdxNode)); // add a node
                        nw->hash = h;
                        nw->equivNodes = n->equivNodes;
                        nw->nxt = na->nxt;
                        na->nxt = nw;
                    } else { // space left in last IdxNode
                        na->hash = h;
                        na->equivNodes = n->equivNodes; //na->nxt=nullptr;
                    }
                }
                if (pooled) {
                    d = n;
                    n = n->nxt;
                    nodes_.remove(d, tid);
                } else n = n->nxt;
                pooled = true;
            } while (n);
        }

        if (old) delete[] old;
    }

    FORCE_INLINE unsigned char height(IdxEquivNode* p) {
        return p ? p->height : 0;
    }

    FORCE_INLINE int bfactor(IdxEquivNode* p) {
        return height(p->right) - height(p->left);
    }

    FORCE_INLINE void fixheight(IdxEquivNode* p) {
        unsigned char hl = height(p->left);
        unsigned char hr = height(p->right);
        p->height = (hl > hr ? hl : hr) + 1;
    }

    FORCE_INLINE IdxEquivNode* rotateright(IdxEquivNode* p) {
        IdxEquivNode* q = p->left;
        p->left = q->right;
        q->right = p;

        q->parent = p->parent;
        p->parent = q;
        if (p->left) p->left->parent = p;

        fixheight(p);
        fixheight(q);
        return q;
    }

    FORCE_INLINE IdxEquivNode* rotateleft(IdxEquivNode* q) {
        IdxEquivNode* p = q->right;
        q->right = p->left;
        p->left = q;

        p->parent = q->parent;
        q->parent = p;
        if (q->right) q->right->parent = q;

        fixheight(q);
        fixheight(p);
        return p;
    }

    FORCE_INLINE IdxEquivNode* balance(IdxEquivNode* p) // balancing the p node
    {
        fixheight(p);
        if (bfactor(p) == 2) {
            if (bfactor(p->right) < 0)
                p->right = rotateright(p->right);
            return rotateleft(p);
        }
        if (bfactor(p) == -2) {
            if (bfactor(p->left) > 0)
                p->left = rotateleft(p->left);
            return rotateright(p);
        }
        return p; // balancing is not required
    }

    void printTreePreorder(IdxEquivNode* p, int indent = 0) {
        if (p != NULL) {
            if (indent) {
                for (size_t i = 0; i < indent; ++i)
                    std::cout << ' ';
            }
            std::cout << "(" << p->obj->key << ", " << p->obj->val << ")\n ";
            if (p->left) printTreePreorder(p->left, indent + 4);
            if (p->right) printTreePreorder(p->right, indent + 4);
        }
    }

    FORCE_INLINE void insertBST(ENTRY* obj, IdxEquivNode* & root, uint8_t tid) {
        //root is not null
        IdxEquivNode* curr = root;
        while (curr != nullptr) {
            ENTRY* currObj = curr->obj;
            if (currObj == obj) return; //it's already there, we do not have to do anything
            if (IDX_FN::lessThan(obj->key, currObj->key)) {
                if (curr->left == nullptr) {
                    IdxEquivNode* nw_equiv = equiv_nodes_.add(); //memset(nw, 0, sizeof(IdxEquivNode)); // add a node
                    nw_equiv->obj = obj;
                    nw_equiv->parent = curr;
                    nw_equiv->height = 1;
                    nw_equiv->left = nw_equiv->right = nullptr;
                    curr->left = nw_equiv;

                    //re-balancing the tree
                    IdxEquivNode* par;
                    while (true) {
                        par = curr->parent;
                        if (par) {
                            if (par->right == curr) {
                                curr = balance(curr);
                                par->right = curr;
                            } else {
                                curr = balance(curr);
                                par->left = curr;
                            }
                        } else {
                            root = balance(curr);
                            return;
                        }
                        curr = par;
                    }
                }
                curr = curr->left;
            } else {
                if (curr->right == nullptr) {
                    IdxEquivNode* nw_equiv = equiv_nodes_.add(tid); //memset(nw, 0, sizeof(IdxEquivNode)); // add a node
                    nw_equiv->obj = obj;
                    nw_equiv->parent = curr;
                    nw_equiv->left = nw_equiv->right = nullptr;
                    nw_equiv->height = 1;
                    curr->right = nw_equiv;

                    //re-balancing the tree
                    IdxEquivNode* par;
                    while (true) {
                        par = curr->parent;
                        if (par) {
                            if (par->right == curr) {
                                curr = balance(curr);
                                par->right = curr;
                            } else {
                                curr = balance(curr);
                                par->left = curr;
                            }
                        } else {
                            root = balance(curr);
                            return;
                        }
                        curr = par;
                    }
                }
                curr = curr->right; //if( IDX_FN::greaterThan(currObj->key, obj->key) ) insertBST( obj, curr->right );
            }
        }
        //obj already exists
    }

    FORCE_INLINE void removeBST(const ENTRY* obj, IdxEquivNode* & root, uint8_t tid) {
        //root is not null
        IdxEquivNode* curr = root;

        while (curr != nullptr) {
            ENTRY* currObj = curr->obj;
            if (currObj == obj) { //found it
                IdxEquivNode *tmp;
                if (curr->left && curr->right) { //2 children case
                    tmp = curr;
                    curr = curr->left;
                    while (curr->right) {
                        curr = curr->right;
                    }
                    tmp->obj = curr->obj;
                }

                //1 or 0 child case
                //curr is the element to be removed
                tmp = (curr->left ? curr->left : curr->right);

                IdxEquivNode *par = curr->parent;
                if (!par)
                    root = tmp;
                else if (curr == par->right) {
                    par->right = tmp;
                } else {
                    par->left = tmp;
                }
                if (tmp) tmp->parent = par;
                curr->left = curr->right = curr->parent = nullptr;
                equiv_nodes_.remove(curr, tid);


                if (par) {
                    curr = par;
                    //re-balancing the tree
                    while (true) {
                        par = curr->parent;
                        if (par) {
                            if (par->right == curr) {
                                curr = balance(curr);
                                par->right = curr;
                            } else {
                                curr = balance(curr);
                                par->left = curr;
                            }
                        } else {
                            root = balance(curr);
                            return;
                        }
                        curr = par;
                    }
                }
                return;
            }
            if (IDX_FN::lessThan(currObj->key, obj->key)) {
                curr = curr->right;
            } else { //if( IDX_FN::greaterThan(currObj->key, obj->key) ) insertBST( obj, curr->right );
                curr = curr->left;
            }
        }
        //obj does not exist
    }


public:

    //    size_t findMaxPerBucket() {
    //        size_t max = 0;
    //        for (size_t i = 0; i < size_; i++) {
    //            size_t buck_count = 0;
    //            IdxNode* n = &buckets_[i];
    //            while (n != nullptr) {
    //
    //                ++buck_count;
    //                if (is_unique)
    //                    n = n->nxt;
    //                else {
    //                    size_t h = n->hash;
    //                    const ENTRY* obj = n->obj;
    //                    //                    std::cout<<"first key is "<<key<<std::endl;
    //                    while ((n = n->nxt) && (h == n->hash) && IDX_FN::equals(obj, n->obj)) {
    //                        //                        std::cout<<n->obj->key<<std::endl;
    //                    }
    //                    //                    exit(1);
    //                }
    //            }
    //            if (buck_count > max) {
    //                max = buck_count;
    //            }
    //
    //        }
    //        return max;
    //    }

    //equivsize the size of Store that manages the memory for the equivalent elements that fall into the same bucket
    //          normally it is the total number of elements (not only distinct ones)
    //storesize the size of Store that manages the memory for HashMap index entries. The number of distinct index entries
    //          is an upper bound on this number

    TreeIndex(std::string n, size_t size, size_t storesize, size_t equivsize, double load_factor, uint8_t numT) : equiv_nodes_(equivsize, n + " IndexEq", numT), nodes_(storesize, n + " Index", numT) {
        load_factor_ = load_factor;
        size_ = 0;
        count_ = 0;
        buckets_ = nullptr;
        resize_(size, numT);
        name = n;

    }

    TreeIndex(std::string n = "N/A", size_t size = DEFAULT_CHUNK_SIZE, double load_factor = .75, uint8_t numT) : equiv_nodes_(size, n + " IndexEq", numT), nodes_(size / 10, n + " Index", numT) {
        load_factor_ = load_factor;
        size_ = 0;
        count_ = 0;
        buckets_ = nullptr;
        resize_(size, numT);
        name = n;

    }

    ~TreeIndex() {
        if (buckets_ != nullptr) delete[] buckets_;
    }

    // inserts regardless of whether element exists already

    FORCE_INLINE virtual void add(ENTRY* obj, uint8_t tid) {
        HASH_RES_t h = IDX_FN::hash(obj->key);
        add(obj, h, tid);
    }

    FORCE_INLINE virtual void add(ENTRY* obj, const HASH_RES_t h, uint8_t tid) {
        if (count_ > threshold_) {
            std::cerr << name << "  Index resize count=" << count_ << "  size=" << size_ << std::endl;
            exit(1);
            //resize_(size_ << 1);
        }
        size_t b = h % size_;
        IdxNode* n = &buckets_[b];
        IdxNode* nw;

        //        if (is_unique) {
        //            ++count_;
        //            if (n->obj) {
        //                nw = nodes_.add(); //memset(nw, 0, sizeof(IdxNode)); // add a node
        //                nw->hash = h;
        //                nw->obj = obj;
        //                nw->nxt = n->nxt;
        //                n->nxt = nw;
        //            } else { // space left in last IdxNode
        //                n->hash = h;
        //                n->obj = obj; //n->nxt=nullptr;
        //            }
        //        } else {
        // ++count_;
        if (!n->equivNodes) { // space left in last IdxNode
            ++count_;
            n->hash = h;
            n->equivNodes = equiv_nodes_.add(tid);
            n->equivNodes->obj = obj; // n->equivNodes->nxt=nullptr;
            n->equivNodes->height = 1;
            n->equivNodes->parent = n->equivNodes->left = n->equivNodes->right = nullptr;

            return;
        }
        do {
            if (h == n->hash && n->equivNodes && IDX_FN::equals(obj->key, n->equivNodes->obj->key)) {

                //insert the node in the tree, only if it is not already there
                insertBST(obj, n->equivNodes, tid);
                return;
            }/*else {
          //go ahead, and look for an element in the same slice
          //or reach the end of linked list of IdxNodes
        }*/
        } while ((n = n->nxt));
        // if(!n) {
        ++count_;
        n = &buckets_[b];
        nw = nodes_.add(tid); //memset(nw, 0, sizeof(IdxNode)); // add a node
        nw->hash = h;
        nw->equivNodes = equiv_nodes_.add(tid); //memset(nw, 0, sizeof(IdxEquivNode)); // add a node
        nw->equivNodes->obj = obj; //nw_equiv->nxt = null;
        nw->equivNodes->height = 1;
        nw->equivNodes->left = nw ->equivNodes ->right = nw->equivNodes->parent = nullptr;
        nw->nxt = n->nxt;
        n->nxt = nw;
        // return;
        // }
        //        }
    }

    // deletes an existing elements (equality by pointer comparison)

    FORCE_INLINE virtual void del(const ENTRY* obj, uint8_t tid) {
        HASH_RES_t h = IDX_FN::hash(obj->key);
        del(obj, h, tid);
    }

    FORCE_INLINE virtual void del(const ENTRY* obj, const HASH_RES_t h, uint8_t tid) {
        IdxNode *n = &buckets_[h % size_];
        IdxNode *prev = nullptr, *next; // previous and next pointers
        do {
            next = n->nxt;
            if (n->hash == h && n->equivNodes && IDX_FN::equals(obj->key, n->equivNodes->obj->key)) {
                removeBST(obj, n->equivNodes);

                if (!n->equivNodes) {
                    if (prev) { //it is an element in the linked list (and not in the bucket itself)
                        prev->nxt = next;
                        // n->nxt = nullptr;
                        // n->obj = nullptr;
                        nodes_.remove(n, tid);
                    } else if (next) { //it is the elements in the bucket, and there are other elements in linked list
                        n->equivNodes = next->equivNodes;
                        n->hash = next->hash;
                        n->nxt = next->nxt;
                        nodes_.remove(next, tid);
                        next = n;
                    } else { //it is the only element in the bucket
                        n->equivNodes = nullptr;
                    }
                    --count_;
                }
                return;
            }
            prev = n;
        } while ((n = next));
    }

    void printTree(const K& key) {
        std::cout << "--------------------------" << std::endl;
        HASH_RES_t h = IDX_FN::hash(key);
        IdxNode* n = &(buckets_[h % size_]);

        do {
            if (n->equivNodes && h == n->hash && IDX_FN::equals(key, n->equivNodes->obj->key)) {
                IdxEquivNode* curr = n->equivNodes;

                printTreePreorder(curr, 0);
                return;
            }
        } while ((n = n->nxt));
        std::cout << "--------------------------" << std::endl;
    }

    FORCE_INLINE virtual size_t count() {
        return count_;
    }

    class iterator {
    private:
        IdxEquivNode *n;
    public:

        iterator(IdxEquivNode* ptr) : n(ptr) {
        }

        iterator() : n(nullptr) {
        }

        ENTRY* incr_get() {
            if (!n) return nullptr;

            if (n->right) { //if current element has an element on its right
                n = n->right;
                while (n->left) n = n->left;
            } else {
                IdxEquivNode* par = n->parent;
                while (par && n == par->right) { //coming from right branch, we are done with the whole part of the tree, we should go upper in the tree
                    n = par;
                    par = n->parent;
                }
                n = par; //coming from left branch, now it's parent's turn
            }
            if (n)
                return n->obj;
            else
                return nullptr;
        }

        ENTRY* get() {
            if (n)
                return n->obj;
            else
                return nullptr;
        };

        ~iterator() {
        }
    };

    inline iterator slice(const K& key) {
        HASH_RES_t h = IDX_FN::hash(key);
        IdxNode* n = &(buckets_[h % size_]);

        do {
            if (n->equivNodes && h == n->hash && IDX_FN::equals(key, n->equivNodes->obj->key)) {
                IdxEquivNode* curr = n->equivNodes;
                while (curr->left) curr = curr->left;

                return iterator(curr);
            }
        } while ((n = n->nxt));
        return iterator();
    }
};

#endif /* SINDEX_H */

