// EXPECT-ERROR: método no const
struct C { int v; void set(int n) { v = n; } };
int f(const C& c) { C d = c; return 0; }
int g(const C& c) { C x; x.v = 1; return 0; }
int h(const C& c) { c.set(1); return 0; }
int main() { return 0; }
