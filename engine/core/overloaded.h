#pragma once

namespace dc::core {

// Набор лямбд как один посетитель для std::visit:
// std::visit(Overloaded{[](const A&) {...}, [](const B&) {...}, [](const auto&) {}}, variant);
template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

}  // namespace dc::core
