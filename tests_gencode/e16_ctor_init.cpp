// EXPECT-ERROR: Inicializadores de campo no soportados
#include <iostream>
using namespace std;
struct R { int a = 5; };
int main() { return 0; }
