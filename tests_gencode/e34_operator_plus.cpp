// EXPECT-ERROR: operator=
struct P { int a; P& operator+(const P& o) { return *this; } };
int main() { return 0; }
