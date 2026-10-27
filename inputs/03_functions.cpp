#include <iostream>
using namespace std;

int mcd(int a, int b) {
    if (b <= 0) {
        return a;
    }
    return mcd(b, a % b);
}

int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int factorial(int n) {
    int res = 1;
    for (int i = 2; i <= n; i++) {
        res = res * i;
    }
    return res;
}

int main() {
    int g = mcd(24, 52);
    int f = fibonacci(6);
    int fact = factorial(5);
    return g + f + fact;
}
