// EXPECT-ERROR: incompleto
#include <iostream>
using namespace std;
struct R { R inner; };
int main() { return 0; }
