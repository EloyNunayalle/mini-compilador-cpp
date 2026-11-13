#include <iostream>
#include <string>
using namespace std;
struct In { int a = 3; string s = "def"; };
struct Out { int x; In in; double d = 1.5; int arr[3]; };
const int N = 4;
int main() {
    In i1;
    In i2 = {7};
    In i3 = {8, "ocho"};
    Out o = {1};
    Out p = {2, {5, "cinco"}, 2.5, {1, 2, 3}};
    int tabla[N * 2];
    tabla[7] = 1;
    cout << i1.a << i1.s.c_str() << i2.a << i2.s.c_str() << i3.a << i3.s.c_str() << endl;
    cout << o.x << o.in.a << o.in.s.c_str() << o.d << o.arr[1] << endl;
    cout << p.x << p.in.a << p.in.s.c_str() << p.d << p.arr[2] << tabla[7] << endl;
    return 0;
}
