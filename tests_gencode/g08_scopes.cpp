#include <iostream>
using namespace std;

int main() {
    int x = 1;
    {
        int x = 2;
        {
            int x = 3;
            cout << x;
        }
        cout << x;
    }
    cout << x << endl;
    for (int i = 0; i < 2; i++) {
        int y = i * 10;
        cout << y;
    }
    for (int i = 5; i < 7; i++) {
        int y = i;
        cout << y;
    }
    cout << endl;
    int a = 1, b = 2, c = a + b;
    cout << a << b << c << endl;
    double d = 1.5, e = d * 2;
    cout << d << " " << e << endl;
    return 0;
}
