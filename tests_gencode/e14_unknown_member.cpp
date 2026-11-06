// EXPECT-ERROR: no existe en
#include <iostream>
using namespace std;
struct R { int a; };
int main() { R r; r.zzz = 1; return 0; }
