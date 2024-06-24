/*  *** option_ptr monadic smart pointer ***  

    An `option_ptr` works like `std::unique_ptr` but does not by default
    overload dereferencing operations such as * and ->, although these
    are still available under conditional compilation.  The idea is to
    prevent problems caused by dereferencing a unique_pointer after a
    move.  The heap value pointed to by an option_ptr should only be
    accessed through combinators such as bind/map/match.

    Like unique_ptr, option_ptr always points to heap.  It is not
    intended as a replacement for the built-in std::optional type.
    Unlike unique_ptr, the constructor that take a raw pointer is
    private and can only be invoked from friend functions `Some` and
    `Nothing`.  Some is similar to make_unique.  The move semantics of
    option_ptr is similar to that of unique_ptr.

    A specialization for arrays is also defined.

    The unchecked pointer operators * and -> become available if source
    is compiled under the `g++ -D UNCHECKED_DEREF` option.

    The included sample program `bst4.cpp` provides an implementation of
    binary search trees using option_ptr.

*/
#include<functional>
#include<iostream>
#include<string>
#include<cstring>
using namespace std;

template<typename T>
struct pointer_deleter {
  static void destruct(T* p) { delete p; }
};
template<typename T>
struct pointer_deleter<T[]> {
  static void destruct(T* p) { delete[] p; }
};

template<class TY, class deleter = pointer_deleter<TY>>
class option_ptr {
protected:
  TY* ptr; // internal pointer to something of type TY

  //template<typename TU>
  void connect(option_ptr<TY,deleter>& R, TY* pr) { // called from subclass
    R.ptr = pr;
  }
private:
  option_ptr(TY* p) {ptr=p;}  // private constructors discourage raw pointers
  
public:
  constexpr option_ptr(): ptr{nullptr} {}  // null is only used internally
  ~option_ptr() { if (ptr) deleter::destruct(ptr); }  // destructor
  void drop() { if (ptr) deleter::destruct(ptr);  ptr=nullptr; } 
  operator bool() const { return ptr!=nullptr; } // overload bool operator

  //template<class TU,class deleter2>
  //friend class option_ptr<TU[],deleter2>;
  
  // friend functions that are part of API
  template<class TU, class... Args>
  friend option_ptr<TU> Some(Args&&... args);
  template<class TU>
  friend option_ptr<TU> Nothing();
  friend ostream& operator <<(ostream& out, option_ptr&& r) {
    if (r.ptr) { out << "Some(" << *r.ptr << ")"; }
    else { out << "None"; }
    return out;
  }// overload << for R-value references
  friend ostream& operator <<(ostream& out, option_ptr& r)  {
    out << move(r);  // this move does not invoke move constructor
    return out;
  }// overload << for L-value references

  // one-time constant for this type
  static const option_ptr<TY> None;
  

  /////////////// Move Semantics:

  option_ptr<TY>& operator=(option_ptr<TY>&& other) { 
    if (ptr) deleter::destruct(ptr); // prevent memory leaks before assignment
    ptr = other.ptr;  // takes over "ownership" of heap value
    other.ptr = nullptr; // ownership is unqiue
    return *this;
  }// move assignment operator

  option_ptr(option_ptr<TY>&& other) {
    if (ptr) deleter::destruct(ptr);
    ptr = other.ptr;    
    other.ptr = nullptr;
  }// move constructor


  ///////////////// Monadic operations without move:
  
  template<class TU>
  option_ptr<TU> bind(function<option_ptr<TU>(TY&)> f) {
    if (ptr) return f(*ptr);
    else return option_ptr<TU>();
    //else return move(*(option_ptr<TU>*)this); // not a good idea
  }//bind

  template<class TU>
  option_ptr<TU> map(function<TU(TY&)> f) {
    if (ptr) {
      TU result = f(*ptr);
      TU* pr = new TU(result);
      return option_ptr<TU>(pr);
    }
    else return option_ptr<TU>();
  }// a.map(f) == a.bind([&](auto x){return Some(f(x));})

  void map_do(function<void(TY&)> f) {
    if (ptr) f(*ptr);
  }

  template<typename TU>
  TU match(function<TU(TY&)> somefun, function<TU()> nonefun) {
    if (ptr) return somefun(*ptr); else return nonefun();
  }//match

  // explicit template instantiation - only in namespace scope
  //template  <> bool match<bool>(function<bool(TY&)>, function<bool()>);
  
  void match_do(function<void(TY&)> some, function<void()> none) {
    if (ptr) some(*ptr); else none();
  }//match do

  // get_or does not move, returns reference
  TY& get_or(TY& default_val) {
    if (ptr) return *ptr; else return default_val;
  }

  // map with function returning same type
  option_ptr<TY>& mutate(function<TY(TY&)> f) {
    if (ptr) { *ptr = f(*ptr); }
    return *this;
  }//map

  ////////////// Monadic operations with move:

  TY take_or(TY default_val) {
    if (ptr) {
      TY x = move(*ptr);
      deleter::destruct(ptr);
      ptr = nullptr;
      return x;
    }
    else return default_val;
  }//take

  // map_move moves value into new option_ptr
  template<class TU>
  option_ptr<TU> map_move(function<TU(TY)> f) {
    if (ptr) {
      TU result = f(*ptr);
      TU* pr = new TU(result);
      deleter::destruct(ptr);
      ptr = nullptr;
      return option_ptr<TU>(pr);
    }
    else return option_ptr<TU>();
  }//map_move

#ifdef UNCHECKED_DEREF
  TY& operator *() { return *ptr; }
  TY* operator ->() { return ptr; }
#endif
  
  // Special operations - use with care
  void memset_with(int c, size_t n) {
    if (ptr) memset(ptr,c,n);
  }
  void memcpy_from(option_ptr<TY>& other, size_t n) {
    if (ptr && other.ptr) memcpy(ptr,other.ptr,n);
  }

}; // option_ptr class


// Some:  (monadic unit), works like make_unique
template<class TY, class... Args>
option_ptr<TY> Some(Args&&... args) {
  return option_ptr<TY>(new TY(std::forward<Args>(args)...));
  // forward retains original l/r-value status of args
}// Some

template<class TY>
option_ptr<TY> Nothing() {
  return option_ptr<TY>();
}//None

template<class TY>
constexpr option_ptr<TY> None = move(option_ptr<TY>());



////////////////////////////////////////////////////////////////////////////
///////////////// Specialization for Arrays, 
/* 
 For the array verion of option_ptr, we deviated from the implementation
 of unique_ptr<T[]> even more, by treating the array as a unqiue monad
 with its own operations including map/reduce.  However, the [i] operator
 is overloaded in the same way, with no runtime overhead to check for
 the validity of i, and so it can be used unsafely.
*/

template<class TY,class deleter>
class option_ptr<TY[],deleter> : public option_ptr<TY,pointer_deleter<TY[]>>
{
private:
  unsigned int len;
  option_ptr(unsigned int n): len{n} { this->ptr = new TY[n]; }
public:
  option_ptr(): len{0} { this->ptr = nullptr; }
  //~option_ptr() { if (this->ptr) delete[] this->ptr; }
  
  template<class TU>
  friend option_ptr<TU[]> Some_array(unsigned int n);
  
  TY& operator [] (unsigned int i) { return this->ptr[i]; } // unchecked

  option_ptr<TY> operator() (unsigned int i) { // checked
    if (this->ptr && (i<len)) return Some<TY>(this->ptr[i]);  //copy!
    // option_ptr<TY> R;
    //  this->connect(R,this->ptr+i);
    //  return R;
    //}
    return Nothing<TY>();
}

  unsigned int size() { return len; }

  // monadic operations specific to arrays:

  option_ptr<TY[]>& foreach(function<void(TY&)> fun) {
    for(int i=0;i<len;i++) fun(this->ptr[i]);
    return *this;
  }
  
  template<class TU>
  option_ptr<TU[]> Map(function<TU(TY&)> f) {
    if (!this->ptr) return Nothing<TU[]>();
    option_ptr<TU[]> R(len);
    for(int i=0;i<len;i++) R.ptr[i] = f(this->ptr[i]);
    return R;
  }//map

  // apply left-associative function with default value id
  TY reduce(function<TY(TY&,TY&)> f, TY id) {
    if (len==0) return id;
    TY ax = move(this->ptr[0]);
    for(int i=1;i<len;i++) ax = f(ax,this->ptr[i]); // no move on R-value
    return ax;
  }//reduce

  template<class TU>
  option_ptr<TU[]> Map_move(function<TU(TY)> f) {
    if (!this->ptr) return Nothing<TU[]>();
    option_ptr<TU[]> R(len);
    for(int i=0;i<len;i++) R.ptr[i] = f(move(this->ptr[i]));
    deleter::destruct(this->ptr);
    len = 0;
    return R;
  }//map

  option_ptr<TY[]>& reverse() {
    for(int i=0;i<len/2;i++) std::swap(this->ptr[i],this->ptr[len-1-i]);
    return *this;
  }//reverse

  bool swap(unsigned int i, unsigned int k) {
    if (i<len && k<len) {
      std::swap(this->ptr[i], this->ptr[k]);
      return true;
    }
    else return false;
  }//swap

  bool set(unsigned int i, TY x) {   // checked set
    if (i<len) {
      this->ptr[i] = move(x);
      return true;
    }
    else return false;
  }

  // returns index of first occurrence of x, or .size() if not found
  unsigned int find(TY& x) {
    for(unsigned int i=0;i<len;i++)
      if (x == this->ptr[i]) return i;
    return len;
  }

  option_ptr<TY[]> join(option_ptr<TY> other) {  // with move
    unsigned int n = len + other.size();
    if (n==0) return Nothing<TY[]>();
    option_ptr<TY[]> J(n);
    for(auto i=0;i<len;i++) J[i] = move (this->ptr[i]);
    for(auto i=len;i<n;i++) J[i] = move (other[i-len]);
    return J;
  }
  
  // move semantics must be redefined because of type (and len)
  option_ptr<TY[]>& operator=(option_ptr<TY[]>&& other) { 
    if (this->ptr) deleter::destruct(this->ptr);
    this->ptr = other.ptr;
    len = other.len;
    other.ptr = nullptr;
    other.len = 0;
    return *this;
  }// move assignment operator

  option_ptr(option_ptr<TY[]>&& other) {
    if (this->ptr) deleter::destruct(this->ptr);
    this->ptr = other.ptr;
    len = other.len;    
    other.ptr = nullptr;    
    other.len = 0;    
  }// move constructor

};//option_ptr<TY[]>


template<class TY>
option_ptr<TY[]> Some_array(unsigned int n) {
  return option_ptr<TY[]>(n);
}


/* DEMO

void arraydemo() {
  option_ptr<int[]> C = Some_array<int>(10);
  option_ptr<int[]> A = Some_array<int>(20);
  A = move(C);

  for(int i=0;i<10;i++) A[i] = i*i;
  for(int i=0;i<A.size();i++) cout << A[i] << ", ";  cout << endl;
  A.map_do([](int& x){cout << "I did it\n";});
  A(3).map_do([](int& x) { cout << "got " << x << endl; }); // inherited
  A(13).map_do([](int& x) { cout << "got " << x << endl; });
  option_ptr<int[]> B = move(A);
  A(4).map_do([](int& x) { cout << "A got " << x << endl; });
  B(4).map_do([](int& x) { cout << "B got " << x << endl; });

  option_ptr<int[]> B2 = B.Map<int>([](int& x) {return x*10;});
  for(int i=0;i<B2.size();i++) cout << B2[i] << ", ";  cout << endl;
  auto sum =
    B2
    .reverse()
    .reduce([](int& x, int& y) {return x-y;}, 0);
    
  cout << "sum: " << sum << endl;

  option_ptr<option_ptr<int>[]> D = Some_array<option_ptr<int>>(2);
  D[1] = Some<int>(55);
  //D(1).map_do([](auto& x){});  // won't compile: requires std::copyable
  cout << "\nend of arraydemo\n";
}


//some functions that return option_ptr
option_ptr<double> safediv(double x, int y) {
  if (y==0) {
    cerr << "ERROR: division of " << x << " by zero\n";
    return Nothing<double>();
  }
  else return Some<double>(x/y);
}//safediv

option_ptr<int> parseint(string n) {  //convert stoi to locally catch exception
  option_ptr<int> answer = Nothing<int>();
  try {
    answer = Some<int>(stoi(n));
  }
  catch(const invalid_argument& ia) {
    cerr << "ERROR: "<< n << " cannot be parsed to an integer\n";
  }
  return answer;
}//parseint

option_ptr<int> largest(int A[], int length) {
  if (length<1) return Nothing<int>();
  int ax = A[0];
  for(int i=1;i<length;i++) { if (A[i]>ax) ax=A[i]; }
  return Some<int>(ax);
}//largest

//// for testing:
int main(int argc, char* argv[]) {
  arraydemo();
  option_ptr<int> input;
  option_ptr<int> number;
  if (argc>1) input = parseint(argv[1]);   // no move required for R-values
  number = move(input);
  cout << "input: " << input << endl;
  cout << "moved to number: " << number << endl;
  
  number
    .mutate([](auto& x){return x-5;})
    .map<int>([](int& x)->int {return x-5;})
    .bind<double>([](int& x)->option_ptr<double> {return safediv(100,x);})
    .mutate([](auto& x){return x*x;})
    .match_do(
              [](auto& x){cout<<"result="<<x<<endl;},   //some case
              [](){cout<<"no result\n";}                //none case
     ); // some explicit types not required if using g++ instead of clang++

  number = Some<int>(50);
  int x = number.match<int>([](int& y){return y/2;}, [](){return 0;});
  cout << "x is " << x << endl;

  bool odd = number.match<bool>([](int& y){return y%2==1;},[](){return false;});
  cout << "odd: " << odd << endl;
  
  int A[5] = {3,2,7,4,1};
  cout << "largest: " << largest(A,5) << endl;
  cout << "largest: " << largest(A,0) << endl;

  auto num2 = Some<int>(3);

#ifdef UNCHECKED_DEREF
  cout << *num2 << "  :: (unchecked deref)\n";
#endif
  
  num2.memcpy_from(num2,sizeof(int));
  cout << "num2: " << num2 << endl;
  
  return 0;
}//main
*/
