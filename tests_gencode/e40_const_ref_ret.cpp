// EXPECT-ERROR: sin perder const
struct P { int a; int& get() const { return a; } };
int main() { return 0; }
