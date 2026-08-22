#pragma once
#include <variant>
#include <type_traits>
#include <chrono>

namespace concepts {

template <class T, class V> struct IsVariantAlternative : std::false_type {};

template <class T, class... Ts>
struct IsVariantAlternative<T, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <class T, class V>
concept VariantAlternative = IsVariantAlternative<T, V>::value;

} // namespace concepts
