#include <iostream>
using namespace std;

int main() {
    int x = 0;
    if (x == 0)
        if (x < 5)
            x = x + 1;
        else
            x = x + 2;
    cout << x << endl;

    int k = 0;
    while (k < 10) {
        k++;
        if (k == 3) continue;
        if (k == 8) break;
        cout << k << " ";
    }
    cout << endl;

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (j > i) break;
            cout << i * j << " ";
        }
        cout << endl;
    }

    int n = 5;
    do {
        n--;
        cout << n;
    } while (n > 0);
    cout << endl;

    int total = 0;
    for (int i = 0, j = 10; i < j; i++, j--) {
        total = total + i + j;
    }
    cout << total << endl;

    int cnt = 0;
    for (;;) {
        cnt++;
        if (cnt >= 4) break;
    }
    cout << cnt << endl;

    int *p = &cnt;
    if (p) cout << "ptr ok" << endl;
    if (!p) cout << "no" << endl; else cout << "else ok" << endl;
    double dd = 0.5;
    while (dd) {
        dd = 0;
    }
    cout << "fin" << endl;
    return 0;
}
