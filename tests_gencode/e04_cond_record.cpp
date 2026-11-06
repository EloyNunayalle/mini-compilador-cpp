// EXPECT-ERROR: La condición debe ser booleana
#include <iostream>
using namespace std;
struct R { int a; };
int main() { R r; if (r) { return 1; } return 0; }
