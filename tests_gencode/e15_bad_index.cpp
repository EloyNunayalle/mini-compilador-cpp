// EXPECT-ERROR: índice del arreglo
#include <iostream>
using namespace std;
int main() { int a[3]; double d = 1.0; return a[d]; }
