// EXPECT-ERROR: espera 2 argumentos
#include <iostream>
using namespace std;
int f(int a, int b) { return a + b; }
int main() { return f(1); }
