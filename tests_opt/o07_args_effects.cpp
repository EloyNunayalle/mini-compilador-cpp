#include <iostream>
using namespace std;
int add(int a, int b) { return a + b; }
int first(int a, int b) { return a; }
int main() {
    int i = 0;
    int r = add(i++, i++);
    int s = first(1, i++);
    int t = add(i, i = 5);
    cout << r << s << t << i << endl;
    return 0;
}
