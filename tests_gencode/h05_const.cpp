#include <iostream>
#include <string>
using namespace std;
struct C {
    const int id;
    int v;
    C(int i) : id(i), v(0) {}
    int get() const { return id + v; }
    void set(int n) { v = n; }
};
const int G = 5;
int suma(const int* p, int n) { int s = 0; for (int i = 0; i < n; i++) s = s + p[i]; return s; }
int leer(const C& c) { return c.get(); }
void inc(int* const p) { *p = *p + 1; }
int main() {
    const int a = 3;
    int arr[G];
    const int fijo[3] = {1, 2, 3};
    for (int i = 0; i < G; i++) arr[i] = i;
    C c(7);
    c.set(2);
    const C cc(1);
    cout << a << suma(arr, G) << suma(fijo, 3) << leer(c) << cc.get() << endl;
    int z = 1;
    inc(&z);
    const int* pc = &z;
    int* const pk = &z;
    *pk = 9;
    cout << *pc << endl;
    const string s = "hola";
    cout << s.size() << s.c_str() << endl;
    return 0;
}
