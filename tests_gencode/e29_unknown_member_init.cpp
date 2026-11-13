// EXPECT-ERROR: no es un campo
struct P { int a; P() : b(1) {} };
int main() { return 0; }
