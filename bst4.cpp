// Binary search trees using custom option_ptr monadic smart pointer

#include<functional>
#include<concepts>   /* requires -std=c++20 */
#include<compare>
#include<iostream>
#include<cassert>
#include "option_ptr.cpp"

template<typename T>
concept ORDERED = requires(T x) { x==x || x<x || x>x; }; //must compile

template<typename T>
int standard_cmp(T& a, T& b) {
  if (a==b) return 0;
  else if (a<b) return -1;
  else return 1;
}
template<typename T>
int reverse_cmp(T& a, T& b) { return standard_cmp(b,a); }

// for syntactic convenience (pure syntactic expansion)
#define node Node<T,cmp>
#define optnode option_ptr<Node<T,cmp>>
#define tmatch template match
#define matchbool template match<bool>
#define tmap template map

// defines Node class generic with respect to type T and cmp function.
// cmp function expected to return 0 for ==, -n for < and +m for >.
template<typename T, int (*cmp)(T&,T&)>
class Node {
private:
  T item; // value stored at node
  option_ptr<Node<T,cmp>> left;  // left and right subtrees
  option_ptr<Node<T,cmp>> right;
public:
  // static instance of empty tree (empty optional)
  static const optnode Nil;
  // default constructor
  Node() {} //: item{}, left{}, right{} {}
  // Single node constructor
  Node(T x) : item{x}, left{optnode()}, right{optnode()} {}

  bool insert(T x) {  // returns true if inserted
    int c = cmp(x,item);
    if (c<0) {// x<item
      return 
      left.matchbool([x](node& n){ return n.insert(x);},
   	             [x,this](){this->left=Some<node>(x); return true;});
    }
    else if (c>0) { // x>item
      return
      right.matchbool([x](node& n){return n.insert(x);},
                      [x,this](){this->right=Some<node>(x); return true;});
    }
    else return false;
  }//insert

  bool search(T& x) { //binary search
    int c = cmp(x,item);
    if (c==0) return true;
    else if (c<0)
      return left.matchbool([&x](node& n){return n.search(x);},
 		            [](){return false;});
    else
      return right.matchbool([&x](node& n){return n.search(x);},
		             [](){return false;});
  }//search

  // map function in_order over tree
  void map_inorder(function<void(T&)> f) {
    left.map_do([&f](node& n){n.map_inorder(f);});
    f(item);
    right.map_do([&f](node& n){n.map_inorder(f);});    
  }
  
};// Node class

// wrapper class, `ORDERED T` same as ... requires ORDERED<T>
template<ORDERED T, int (*cmp)(T&,T&) = standard_cmp<T>>
class BST {
private:
  option_ptr<Node<T,cmp>> root;
  size_t count;
  /*
  static optnode insert(optnode& current, T x) {
    return
      current.match(
         [&current,x,this](node& n){
	   if (x<n.item) n.left = insert(n.left,x);
	   else if (n.item<x) n.right = insert(n.right,x)
	   return current;
	 },
	 [](){ return Some<node>(Node(x)); });
  }//insert
  */
public:
  BST() : root{optnode()}, count{0} {} // default constructor makes empty tree

  size_t size() { return count; }

  bool insert(T x) {
      bool inserted = false; 
      root.match_do([x,&inserted](node& n) { inserted = n.insert(x);},
		    [x,this,&inserted]() {
		      this->root = Some<node>(x);
		      inserted = true;
		    });
    if (inserted) count++;
    return inserted;
  }//insert

  bool contains(T& x) {
     return root.matchbool([&x](node& n) {return n.search(x);},
  		           []() {return false;});
  }//contains
  bool contains_val(T x) { return contains(x); }

  void map_inorder(function<void(T&)> f) {
    root.map_do([&f](node& n ){n.map_inorder(f);});
  }

  // default move semantics (code is redundant, just for emphasis)
  // BST(BST<T,cmp>&& other) = default;
  // BST<T,cmp>& operator=(BST<T,cmp>&& other) = default;
  
  ////// custom move semantics
  BST(BST<T,cmp>&& other) {  // move constructor
    root = move(other.root);
    count = other.count;
    other.count = 0;
  }
  BST<T,cmp>& operator = (BST<T,cmp>&& other) {  // move assignment
    root = move(other.root);
    count = other.count;
    other.count = 0;   
  }// move semantics of BST

}; // BST wrapper class


/////////// arbitrary class for type-checking template Node class
struct Arbitrary {
  bool operator <(Arbitrary& x) { return false; }
  bool operator >(Arbitrary& x) { return false; }  
  bool operator ==(Arbitrary& x) { return true; }
  // overloads minimally satisfy requirements of ORDERED concept
};

// type checking template classes with arbitrary object
void type_check_templates() {   // not called, just compiled
  Arbitrary a1;
  BST<Arbitrary> tree;
  assert(tree.insert(a1));
  assert(tree.contains(a1));
  assert(tree.contains_val(a1));  
  assert(tree.size()==1);
}//type_check_Node


int int_reverse_cmp(int& x, int& y) { return y-x; }  // custom cmp function
// the choice of cmp function is made at COMPILE TIME, in contrast to
// java-ish languages.

// but what if I want to decide to sort in increasing or decreasing order
// at RUNTIME?  I can cheat with a global variable!
bool decreasing_float = false;
int float_cmp(double& x, double& y) {  // compares floats by rounding
  int64_t xr = (int64_t)(x*10000000 + 0.5);
  int64_t yr = (int64_t)(y*10000000 + 0.5);
  if (decreasing_float) return yr-xr; else return xr-yr;
}
// note: won't work by creating a lambda-closure because the closure
// can't be created at compile time.

int main() {
  using namespace std;
  //decreasing_float = true; // sort in decreasing order
  BST<double,float_cmp> tree;
  for(double i:{5.0,4.0,1.5,8.0,7.2,9.1,5.9,2.5}) tree.insert(i);
  cout << tree.contains_val(7.2) << endl;
  cout << tree.contains_val(6.0) << endl;  
  cout << "tree size " << tree.size() << endl;

  double sum = 0.0;
  tree.map_inorder([&sum](double& x){sum += x;});
  cout << "tree sum is " << sum << endl;

  BST<double,float_cmp> tree2 = move(tree);  // won't compile without move
  tree2.map_inorder([](double& x){cout << x << "  ";});
  cout << "\nsize of moved tree: " << tree.size() << endl;
  //tree.map_inorder([](double& x){cout << x << "  ";}); // does not crash

  
  return 0;
}//main
