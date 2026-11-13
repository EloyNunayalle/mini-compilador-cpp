// EXPECT-ERROR: destructor
struct P { ~P() {} ~P() {} };
int main() { return 0; }
