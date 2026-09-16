#pragma once
#include <array>
namespace splat {
bool EnableGamepad(bool enabled);
std::array<double,4> ReadGamepad();
}
