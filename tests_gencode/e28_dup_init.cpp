// EXPECT-ERROR: más de una vez
struct P { int a; P() : a(1), a(2) {} };
int main() { return 0; }
