// EXPECT-ERROR: parámetro por referencia
#include <iostream>
using namespace std;
void f(int &a) { a = 1; }
int main() { f(3); return 0; }
