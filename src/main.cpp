#include "Greeter.h"
#include <iostream>

int main(int, char**) {
    auto greeter = speaker::Greeter();
    std::cout << greeter.SayHello();
}
