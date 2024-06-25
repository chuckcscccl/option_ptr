## Monadic Smart Pointer **`option_ptr`** for C++

An **[option_ptr](https://github.com/chuckcscccl/option_ptr/blob/main/option_ptr.cpp)** works like `std::unique_ptr` but does not by default overload
dereferencing operations such as `*` and `->`, although these are still
available under conditional compilation.  The idea is to prevent
problems caused by dereferencing a unique pointer after a move.  The
heap value pointed to by an option_ptr should only be accessed through
combinators such as bind/map/match.

Like `std::unique_ptr`, `option_ptr` can only point to heap.  It is
not intended as a replacement for the built-in `std::optional` type.
Unlike `unique_ptr`, the constructor that take a raw pointer is
private and can only be invoked from friend functions `Some` and
`Nothing`.  Some is similar to make_unique.  The move semantics of
option_ptr is similar to that of unique_ptr.

A specialization for arrays is also defined, with functional-style
operations such as map/reduce, making array their own "monad".

The unchecked pointer operators * and -> become available if source
is compiled under the `g++ -D UNCHECKED_DEREF` option.

The following code demonstrates usage of `option_ptr`

```
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

void arraydemo();

//// for testing:
int main(int argc, char* argv[]) {
  option_ptr<int> input;
  option_ptr<int> number;
  if (argc>1) input = parseint(argv[1]);   // no move required for R-values
  number = move(input);
  cout << "input: " << input << endl;
  cout << "moved to number: " << number << endl;
  
  number
    .mutate([](auto& x){return x-5;})  // in-place mutation of value
    .map<int>([](int& x)->int {return x-5;}) // maps to new option_ptr
    .bind<double>([](int& x)->option_ptr<double> {return safediv(100,x);})
    .mutate([](auto& x){return x*x;})
     // match_do takes 2 void functions, for Some and None cases    
    .match_do(  
              [](auto& x){cout<<"result="<<x<<endl;},   //some case
              [](){cout<<"no result\n";}                //none case
     ); // some explicit types not required if using g++ instead of clang++

  number = Some<int>(50);
  // match takes two functions that return values
  int x = number.match<int>([](int& y){return y/2;},
                            [](){return 0;});
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
  arraydemo();
  return 0;
}//main


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
```


Futhermore, the included sample program [bst4.cpp](https://github.com/chuckcscccl/option_ptr/blob/main/bst4.cpp) provides an implementation of
binary search trees using option_ptr.
