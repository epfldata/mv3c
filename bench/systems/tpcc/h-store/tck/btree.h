/*****************************
 * REWORKED                  *
 *****************************
 * Comments:
 * - Had to remove crap with 'depth'. The B+Tree behaved as always perfectly balanced!
 * - Issues with reclaiming memory / reducing the tree / rebalancing are not solved.
 * - Deletions are apparently not used anywhere in the rest of the code
 */

#if !defined BPLUSTREE_HPP_227824
#define BPLUSTREE_HPP_227824

// This is required for glibc to define std::posix_memalign
#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 600)
#define _XOPEN_SOURCE 600
#endif
#include <stdlib.h>

#include <assert.h>
#include <boost/static_assert.hpp>
#include <boost/pool/object_pool.hpp>

// DEBUG
#include <iostream>
using std::cout;
using std::endl;

#if defined DEBUG_ASSERT
#  warning "DEBUG_ASSERT overloaded"
#else
#  if defined DEBUG
#    define DEBUG_ASSERT(expr) assert(expr)
#  else
#    define DEBUG_ASSERT(expr)
#  endif
#endif

template <typename KEY, typename VALUE, unsigned N, unsigned M,
  unsigned INNER_NODE_PADDING=0, unsigned LEAF_NODE_PADDING=0,
  unsigned NODE_ALIGNMENT=64>
class BPlusTree {
public:
  BOOST_STATIC_ASSERT(N>2); // N must be greater than two to make the split of two inner nodes sensible.
  BOOST_STATIC_ASSERT(M>0); // Leaf nodes must be able to hold at least one element

  BPlusTree() : root(new_leaf_node()) {} // Builds a new empty tree.
  ~BPlusTree() {} // Memory deallocation is done automatically when innerPool and leafPool are destroyed.

  // Inserts a pair (key, value). If there is a previous pair with
  // the same key, the old value is overwritten with the new one.
  void insert(KEY key, VALUE value) {
    InsertionResult result;
    bool was_split = (root->type == NODE_LEAF) ?
      leaf_insert(reinterpret_cast<LeafNode*>(root), key, value, &result) : // The root is a leaf node
      inner_insert(reinterpret_cast<InnerNode*>(root), key, value, &result); // The root is an inner node
    if (was_split) { // The old root was splitted in two parts. We have to create a new root pointing to them
      InnerNode* proxy = new_inner_node();
      proxy->num_keys= 1;
      proxy->keys[0]= result.key;
      proxy->children[0]= result.left;
      proxy->children[1]= result.right;
      root = proxy;
    }
  }

  // Looks for the given key and return true if found; copies the associated value in 'value'.
  bool find(const KEY& key, VALUE* value=NULL) const {
    InnerNode* inner;
    register unsigned index;
    register Node* node = root;
    while(node->type!=NODE_LEAF) {
      inner = reinterpret_cast<InnerNode*>(node);
      index = inner_position_for(key, inner->keys, inner->num_keys);
      node = inner->children[index];
    }
    LeafNode* leaf = reinterpret_cast<LeafNode*>(node);
    index = leaf_position_for(key, leaf->keys, leaf->num_keys);
    if(leaf->keys[index] == key) {
      if(value!=NULL) *value=leaf->values[index];
      return true;
    } else {
      return false;
    }
  }

  // Lookup and delete the given key. Return true if it is found.
  // Associated value is set to NULL. Leaks memory but not used. "Fix later."
  bool del(const KEY& key) const {
    InnerNode* inner;
    register unsigned index;
    register Node* node = root;
    while(node->type!=NODE_LEAF) {
      inner = reinterpret_cast<InnerNode*>(node);
      index = inner_position_for(key, inner->keys, inner->num_keys);
      node = inner->children[index];
    }
    LeafNode* leaf = reinterpret_cast<LeafNode*>(node);
    index = leaf_position_for(key, leaf->keys, leaf->num_keys);
    if (leaf->keys[index] == key) {
      // XXX: free(leaf->values[index]);
      leaf->values[index] = NULL;
      return true;
    } else {
      return false;
    }
  }

  // Useful when optimizing performance with cache alignment.
  unsigned sizeof_inner_node() const { return sizeof(InnerNode); }
  unsigned sizeof_leaf_node() const { return sizeof(LeafNode); }
private:
  // Used when debugging
  enum NodeType {NODE_INNER=0xDEADBEEF, NODE_LEAF=0xC0FFEE};
  struct Node { NodeType type; Node(NodeType tp):type(tp) {} };

  // Leaf nodes store pairs of keys and values.
  struct LeafNode : public Node {
    LeafNode() : Node(NODE_LEAF), num_keys(0) { memset(keys,0,sizeof(KEY)*M); }
    unsigned num_keys;
    KEY keys[M];
    VALUE values[M];
    // unsigned char _pad[LEAF_NODE_PADDING];
  };

  // Inner nodes store pointers to other nodes interleaved with keys.
  struct InnerNode : public Node {
    InnerNode() : Node(NODE_INNER), num_keys(0) { memset(keys,0,sizeof(KEY)*N); }
    unsigned num_keys;
    KEY keys[N];
    Node* children[N+1];
    // unsigned char _pad[INNER_NODE_PADDING];
  };

  // Data type returned by the private insertion methods.
  struct InsertionResult { KEY key; Node* left; Node* right; };

  // Returns the position where 'key' should be inserted in a node that has the given keys.
  // Simple linear search. Faster for small values of N or M. Binary search. It is faster when N or M is > 100.
  static unsigned leaf_position_for(const KEY& key, const KEY* keys, unsigned num_keys) {
    unsigned k=0; while(k<num_keys && keys[k]<key) ++k; return k;
  }
  static inline unsigned inner_position_for(const KEY& key, const KEY* keys, unsigned num_keys) {
    unsigned k=0; while(k<num_keys && keys[k]<=key) ++k; return k; // mind the <=
  }

  bool leaf_insert(LeafNode* node, KEY& key, VALUE& value, InsertionResult* result) {
    assert(node->num_keys <= M);
    bool was_split = false;
    // Simple linear search
    unsigned i = leaf_position_for(key, node->keys, node->num_keys);
    if (node->num_keys == M) {
      // The node was full. We must split it
      unsigned treshold= (M+1)/2;
      LeafNode* new_sibling= new_leaf_node();
      new_sibling->num_keys = node->num_keys - treshold;
      for(unsigned j=0; j<new_sibling->num_keys; ++j) {
        new_sibling->keys[j] = node->keys[treshold+j];
        new_sibling->values[j] = node->values[treshold+j];
      }
      node->num_keys= treshold;
      if (i<treshold) leaf_insert_nonfull(node, key, value, i); // Inserted element goes to left sibling
      else leaf_insert_nonfull(new_sibling, key, value, i-treshold); // Inserted element goes to right sibling
      // Notify the parent about the split
      was_split = true;
      result->key = new_sibling->keys[0];
      result->left = node;
      result->right = new_sibling;
    } else {
      // The node was not full
      leaf_insert_nonfull(node, key, value, i);
    }
    return was_split;
  }

  static void leaf_insert_nonfull(LeafNode* node, KEY& key, VALUE& value, unsigned index) {
    assert(node->num_keys < M);
    assert(index <= M);
    assert(index <= node->num_keys);
    if(index<M && node->keys[index]==key) {
      // We are inserting a duplicate value. Simply overwrite the old one
      node->values[index]= value;
    } else {
      // The key we are inserting is unique
      for(unsigned i=node->num_keys; i > index; --i) {
        node->keys[i]= node->keys[i-1];
        node->values[i]= node->values[i-1];
      }
      node->num_keys++;
      node->keys[index]= key;
      node->values[index]= value;
    }
  }

  bool inner_insert(InnerNode* node, KEY& key, VALUE& value, InsertionResult* result) {
    // Early split if node is full.
    // This is not the canonical algorithm for B+ trees,
    // but it is simpler and does not break the definition.
    bool was_split = false;
    if (node->num_keys == N) {
      // Split
      unsigned treshold= (N+1)/2;
      InnerNode* new_sibling= new_inner_node();
      new_sibling->num_keys= node->num_keys -treshold;
      for(unsigned i=0; i<new_sibling->num_keys; ++i) {
        new_sibling->keys[i]= node->keys[treshold+i];
        new_sibling->children[i]=
        node->children[treshold+i];
      }
      new_sibling->children[new_sibling->num_keys] = node->children[node->num_keys];
      node->num_keys= treshold-1;
      // Set up the return variable
      was_split= true;
      result->key= node->keys[treshold-1];
      result->left= node;
      result->right= new_sibling;
      // Now insert in the appropriate sibling
      if (key < result->key) inner_insert_nonfull(node, key, value);
      else inner_insert_nonfull(new_sibling, key, value);
    } else { // No split
      inner_insert_nonfull(node, key, value);
    }
    return was_split;
  }

  void inner_insert_nonfull(InnerNode* node, KEY& key, VALUE& value) {
    assert( node->num_keys < N );
    // Simple linear search
    unsigned index = inner_position_for(key, node->keys, node->num_keys);
    InsertionResult result;
    bool was_split;
    if (node->children[index]->type == NODE_LEAF) {
      was_split = leaf_insert(reinterpret_cast<LeafNode*>(node->children[index]), key, value, &result);
    } else {
      was_split = inner_insert(reinterpret_cast<InnerNode*>(node->children[index]), key, value, &result);
    }

    if(was_split) {
      if( index == node->num_keys ) {
        // Insertion at the rightmost key
        node->keys[index]= result.key;
        node->children[index]= result.left;
        node->children[index+1]= result.right;
        node->num_keys++;
      } else {
        // Insertion not at the rightmost key
        node->children[node->num_keys+1]=
        node->children[node->num_keys];
        for(unsigned i=node->num_keys; i!=index; --i) {
          node->children[i]= node->children[i-1];
          node->keys[i]= node->keys[i-1];
        }
        node->children[index]= result.left;
        node->children[index+1]= result.right;
        node->keys[index]= result.key;
        node->num_keys++;
      }
    } // else the current node is not affected
  }

  // Custom allocator that returns aligned blocks of memory
  template <unsigned ALIGNMENT> struct AlignedMemoryAllocator {
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    static char* malloc(const size_type bytes) {
      void* result; // alternative: = std::malloc(bytes);
      if(posix_memalign(&result, ALIGNMENT, bytes) != 0) { result=0; }
      return reinterpret_cast<char*>(result);
    }
    static void free(char* const block) { std::free(block); }
  };
  typedef AlignedMemoryAllocator<NODE_ALIGNMENT> AlignedAllocator;

  // Node memory allocators. IMPORTANT NOTE: they must be declared
  // before the root to make sure that they are properly initialised
  // before being used to allocate any node.
  boost::object_pool<InnerNode, AlignedAllocator> innerPool;
  boost::object_pool<LeafNode, AlignedAllocator>  leafPool;

  // Allocate and delete nodes
  LeafNode* new_leaf_node() { return leafPool.construct(); } // new LeafNode();
  void delete_leaf_node(LeafNode* node) { leafPool.destroy(node); } // delete node;
  InnerNode* new_inner_node() { return innerPool.construct(); } // new InnerNode();
  void delete_inner_node(InnerNode* node) { innerPool.destroy(node); } // delete node;

  // Pointer to the root node. It may be a leaf or an inner node, but
  // it is never null.
  Node*  root;
};

#endif // !defined BPLUSTREE_HPP_227824
