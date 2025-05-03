#include "Greeter.h"

namespace speaker {

Greeter::Greeter() = default;

std::string Greeter::SayHello()
{
    return { "Hello world!" };
}

} // namespace speaker