#pragma once

namespace my_std {

struct Square {
    int side;
    double area() const { return side * side; }
};

struct Circle {
    int radius;
    double area() const { return radius * radius * 3.14; }
};

struct Triangle {
    int height;
    int base;
    double area() const { return height * base * 0.5; }
};

}
