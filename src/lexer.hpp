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
    std::string_view::size_type start = 0;
    std::string_view::size_type end = 0;

    bool isAdjacent(const Token &rhs) const { return end == rhs.start; }
  };

  Lexer(std::string_view data) : m_data(data), m_cursor(0) {}

  std::optional<Token> peakIf(TokenType type) {
    if (auto tok = peak(); tok && tok->type == type) {
      return tok;
    }
    return std::nullopt;
  }

  std::optional<Token> peak(int n = 0) {
    auto old = m_cursor;
    std::optional<Token> tok;
    for (int i = 0; i != n + 1; ++i) {
      tok = next();
    }
    m_cursor = old;
    return tok;
  }

  // get as much string as we can and return that portion
  std::optional<std::string_view> peakString(int n = 1) {
    auto old = m_cursor;
    int i = 0;

    while (m_cursor < m_data.size() && std::isspace(m_data[m_cursor])) {
      ++m_cursor;
    }
    auto start = m_cursor;

    while (auto tok = peakIf(TokenType::String)) {
      if (i >= n) {
        break;
      }
      ++i;
      next();
    }

    // couldn't peak n consecutive strings
    if (i < n) {
      m_cursor = old;
      return std::nullopt;
    }

    auto str = m_data.substr(start, m_cursor - start);
    m_cursor = old;
    return str;
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
      return Token{.raw = getSelection(),
                   .type = type,
                   .start = startPos,
                   .end = m_cursor};
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
          continue;
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
        if (m_cursor - startPos == 0) {
          switch (c) {
          case '(':
          case ')':
          case '-':
          case '+':
          case ',':
            ++m_cursor;
            return tryCommit();
          default:
            break;
          }
        }

        if (std::isalnum(c) || std::isspace(c)) {
          return tryCommit();
        }
        break;
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
