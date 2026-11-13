// EXPECT-ERROR: requiere el constructor por defecto
struct P { int a; P(int x) : a(x) {} };
int main() { P p; return 0; }
