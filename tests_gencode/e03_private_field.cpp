// EXPECT-ERROR: es privado
#include <iostream>
using namespace std;
class A { private: int x; public: int g() { return x; } };
int main() { A a; a.x = 1; return 0; }
