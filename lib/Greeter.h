#pragma once

#include <string>

namespace speaker {

class Greeter {
public:
    explicit Greeter();
    [[nodiscard]] std::string SayHello();

private:
};

} // namespace speaker
