#pragma once
#include <cctype>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

class Lexer {
public:
  struct Number {
    std::string_view n;
  };

  enum class OperatorType { Add, Subtract, Multiply, Divide, Pow };
  enum class State { Reset, Number, Operator, String };

  enum class TokenType { String, Number, Operator };

  struct String {
    std::string_view data;
  };
  struct Operator {
    char c;
  };

  struct Token {
    std::string_view raw;
    TokenType type;
    std::variant<Number, String, Operator> data;
  };

  Lexer(std::string_view data) : m_data(data), m_cursor(0) {}

  std::optional<Token> peakIf(TokenType type) {
    if (auto tok = peak(); tok && tok->type == type) {
      return tok;
    }
    return std::nullopt;
  }

  std::optional<Token> peak() {
    auto old = m_cursor;
    auto tok = next();
    m_cursor = old;
    return tok;
  }

  // e.g can check whether "to the power of" is next
  bool isWordSequence(std::span<const std::string_view> words) {
    auto old = m_cursor;
    size_t i = 0;

    while (auto tok = peakIf(TokenType::String)) {
      if (i == words.size()) {
        break;
      }
      if (tok->raw != words[i]) {
        break;
      }
      ++i;
      next();
    }

    m_cursor = old;
    return i == words.size();
  }

  std::optional<Token> next() {
    State state = State::Reset;
    size_t startPos = m_cursor;

    const auto getSelection = [&]() -> std::string_view {
      return m_data.substr(startPos, m_cursor - startPos);
    };

    const auto makeToken = [&](TokenType type) {
      return Token{getSelection(), type};
    };

    auto tryCommit = [&]() -> std::optional<Token> {
      switch (state) {
      case State::Number:
        return makeToken(TokenType::Number);
      case State::String:
        return makeToken(TokenType::String);
      case State::Operator:
        return makeToken(TokenType::Operator);
      default:
        return std::nullopt;
      }
    };

    while (m_cursor < m_data.size()) {
      char c = m_data[m_cursor];

      switch (state) {
      case State::Reset: {
        if (std::isdigit(c)) {
          state = State::Number;
          continue;
        }
        if (std::isspace(c)) {
          startPos += 1;
          break;
        }
        if (std::isalpha(c)) {
          state = State::String;
          continue;
        }
        if (!std::isalnum(c)) {
          state = State::Operator;
          break;
        }
        break;
      }
      case State::Number: {
        // TODO: we probably want to only allow one '.' for a valid digit of
        // course. And we will also want to localize it at some point.
        if (!std::isdigit(c) && c != '.') {
          return tryCommit();
        }
        break;
      }
        // TODO: we will probably want multichar operators...
      case State::Operator: {
        return tryCommit();
      }
      case State::String: {
        if (!std::isalpha(c)) {
          return tryCommit();
        }
        break;
      }
      }

      ++m_cursor;
    }

    return tryCommit();
  }

private:
  std::string_view m_data;
  std::string_view::size_type m_cursor;
};
