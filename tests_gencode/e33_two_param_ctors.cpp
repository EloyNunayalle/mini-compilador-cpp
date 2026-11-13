// EXPECT-ERROR: Solo se permite un constructor con parámetros
struct P { int a; P(int x) : a(x) {} P(int x, int y) : a(x) {} };
int main() { return 0; }
