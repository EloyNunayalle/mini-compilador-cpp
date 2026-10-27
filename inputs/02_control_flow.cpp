#include <iostream>
using namespace std;

int main() {
    int x = 0;
    if (x == 0)
        if (x < 5)
            x = x + 1;
        else
            x = x + 2;

    while (x < 10) {
        x++;
        if (x == 5) continue;
        if (x == 8) break;
    }

    for (int i = 0; i < 5; i++) {
        x = x + i;
    }

    do {
        x--;
    } while (x > 0);

    return x;
}
