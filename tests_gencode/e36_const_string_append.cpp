// EXPECT-ERROR: el objeto es const
#include <string>
using namespace std;
int main() { const string s = "a"; s.append("b"); return 0; }
