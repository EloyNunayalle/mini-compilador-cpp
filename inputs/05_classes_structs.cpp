#include <iostream>
using namespace std;

struct Point {
public:
    int x;
    int y;
    int sum() {
        return x + y;
    }
};

class Rectangle {
private:
    int width;
    int height;
public:
    int getArea() {
        return width * height;
    }
};

int main() {
    Point pt;
    pt.x = 10;
    pt.y = 20;
    return pt.x + pt.y;
}
