// EXPECT-ERROR: por valor no soportado
#include <iostream>
using namespace std;
struct R { int a; };
int f(R r) { return r.a; }
int main() { return 0; }
