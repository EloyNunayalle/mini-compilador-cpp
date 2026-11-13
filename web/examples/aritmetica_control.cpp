#include <iostream>
using namespace std;

int mcd(int a, int b) {
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

int fibonacci(int n) {
    int a = 0;
    int b = 1;
    for (int i = 0; i < n; i = i + 1) {
        int t = a + b;
        a = b;
        b = t;
    }
    return a;
}

bool es_primo(int n) {
    if (n < 2) {
        return false;
    }
    int d = 2;
    while (d * d <= n) {
        if (n % d == 0) {
            return false;
        }
        d = d + 1;
    }
    return true;
}

int main() {
    cout << "mcd(48, 18) = " << mcd(48, 18) << "\n";
    cout << "fibonacci: ";
    for (int i = 0; i < 10; i = i + 1) {
        cout << fibonacci(i) << " ";
    }
    cout << "\n";
    cout << "primos hasta 30: ";
    for (int n = 1; n <= 30; n = n + 1) {
        if (es_primo(n)) {
            cout << n << " ";
        }
    }
    cout << "\n";
    double x = 7.5;
    double y = 2.0;
    cout << x << " / " << y << " = " << x / y << "\n";
    cout << "17 / 5 = " << 17 / 5 << ", 17 % 5 = " << 17 % 5 << "\n";
    return 0;
}
