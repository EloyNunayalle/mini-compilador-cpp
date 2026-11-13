// EXPECT-ERROR: debe declararse como
struct P { int a; void operator=(const P& o) {} };
int main() { return 0; }
