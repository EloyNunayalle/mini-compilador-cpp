// EXPECT-ERROR: es privado
#include <iostream>
using namespace std;
class A { int sec() { return 1; } public: int pub() { return sec(); } };
int main() { A a; return a.sec(); }
