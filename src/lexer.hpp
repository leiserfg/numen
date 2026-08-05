#pragma once
#include <any>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

class Lexer {
public:
  struct Number {
    double n;
    unsigned fromBase = 10;
  };

  enum class OperatorType { Add, Subtract, Multiply, Divide, Pow };
  enum class State { Reset, Number, Operator, NumberBase, String };
  enum class TokenType { String, Number, Operator };

  struct String {
    std::string_view data;
  };
  struct Operator {
    char c;
  };

  using TokenData = std::variant<Number, String, Operator>;

  struct Token {
    std::string_view raw;
    TokenType type;
    TokenData data;
    std::string_view::size_type start = 0;
    std::string_view::size_type end = 0;

    bool isAdjacent(const Token &rhs) const { return end == rhs.start; }

    const Number *asNumber() const { return std::get_if<Number>(&data); }
    const Number *asNumber() { return std::get_if<Number>(&data); }
  };

  template <typename... Ts> std::optional<std::tuple<Ts...>> peakForward() {
    return [&]<std::size_t... I>(std::index_sequence<I...>) -> std::optional<std::tuple<Ts...>> {
      std::array<std::optional<Lexer::Token>, sizeof...(Ts)> p{peak(I)...};
      bool ok = std::apply([](auto... q) { return (... && (q && std::holds_alternative<Ts>(q->data))); }, p);
      if (!ok) return std::nullopt;
      return std::tuple<Ts...>{std::get<Ts>(p[I]->data)...};
    }(std::index_sequence_for<Ts...>{});
  }

  Lexer(std::string_view data) : m_data(data), m_cursor(0) {}

  std::optional<Token> peakIf(TokenType type);
  std::optional<Token> peak(int n = 0);
  // get as much string as we can and return that portion
  std::optional<std::string_view> peakString(int n = 1);

  // e.g can check whether "to the power of" is next
  bool isWordSequence(std::span<const std::string_view> words);
  std::optional<Token> next();

private:
  std::string_view m_data;
  std::string_view::size_type m_cursor;
};
