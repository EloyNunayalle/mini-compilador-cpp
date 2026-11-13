#include <iostream>
#include <string>
using namespace std;
struct O {
    int v;
    string s;
    O() : v(1), s("x") { cout << "O()" << endl; }
    O(int a) : v(a), s("y") { cout << "O(" << a << ")" << endl; }
    ~O() { cout << "~O" << v << endl; }
};
struct Plain { int a; int b; };
int main() {
    O* a = new O;
    O* b = new O(5);
    O* arr = new O[3];
    arr[1].v = 42;
    cout << a->v << b->v << arr[1].v << a->s.c_str() << endl;
    delete a;
    delete b;
    delete[] arr;
    int* n = new int(7);
    int* z = new int();
    cout << *n << *z << endl;
    delete n;
    delete z;
    Plain* pp = new Plain();
    cout << pp->a << pp->b << endl;
    delete pp;
    string* ps = new string[2];
    ps[0].append("q");
    cout << ps[0].c_str() << endl;
    delete[] ps;
    return 0;
}
