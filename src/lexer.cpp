#include "lexer.hpp"
#include <cctype>
#include <cmath>

namespace {
constexpr std::string_view BASE_CHARS = "0123456789abcdef";

// we hardcode these, in order to avoid ambiguity. Only result strings are localized.
constexpr char THOUSAND_DELIM = '_';
constexpr char FRACTION_DELIM = '.';
}; // namespace

void Lexer::advance(int n) {
  for (int i = 0; i != n; ++i) {
    next();
  }
}

std::optional<Lexer::Token> Lexer::next() {
  State state = State::Reset;
  size_t startPos = m_cursor;
  double n = 0;
  unsigned base = 10;
  unsigned nfrac = 0;
  int expSign = 1;
  unsigned expValue = 0;

  const auto getSelection = [&]() -> std::string_view {
    return m_data.substr(startPos, m_cursor - startPos);
  };

  const auto hasExponentDigits = [&](std::size_t pos) {
    if (pos < m_data.size() && (m_data[pos] == '+' || m_data[pos] == '-')) ++pos;
    return pos < m_data.size() && std::isdigit(m_data[pos]);
  };

  const auto makeToken = [&](TokenType type, TokenData data) {
    return Token{.raw = getSelection(), .type = type, .data = data, .start = startPos, .end = m_cursor};
  };

  auto tryCommit = [&]() -> std::optional<Token> {
    switch (state) {
    case State::NumberExponentSign:
    case State::NumberExponent:
    case State::Number:
    case State::NumberBase: {
      // n carries the digits as an integer, scaling it once here keeps the
      // mantissa exact until the very last operation
      int scale = expSign * static_cast<int>(expValue) - (nfrac ? static_cast<int>(nfrac) - 1 : 0);

      if (scale > 0) { n *= std::pow(base, scale); }
      if (scale < 0) { n /= std::pow(base, -scale); }

      return makeToken(TokenType::Number, Number{.n = n, .fromBase = base});
    }
    case State::String:
      return makeToken(TokenType::String, String{.data = getSelection()});
    case State::Operator:
      return makeToken(TokenType::Operator, Operator{});
    default:
      return std::nullopt;
    }
  };

  while (m_cursor < m_data.size()) {
    char c = m_data[m_cursor];

    switch (state) {
    case State::Reset: {
      if (std::isdigit(c)) {
        if (c == '0') {
          state = State::NumberBase;
          break;
        }
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
      if (c == FRACTION_DELIM && m_cursor + 1 < m_data.size() && std::isdigit(m_data[m_cursor + 1])) {
        state = State::Number;
        continue;
      }
      if (!std::isalnum(c)) {
        state = State::Operator;
        continue;
      }
      break;
    }
    case State::NumberBase: {
      // we always expect a '0<char>' syntax for base prefixes
      // we don't parse '0777' as octal because the leading zero
      // can create a lot of ambiguity.
      state = State::Number;
      switch (std::tolower(c)) {
      case 'x':
        base = 16;
        break;
      case 'b':
        base = 2;
        break;
      case 'o':
        base = 8;
        break;
      default:
        continue;
      }

      break;
    }
    case State::Number: {
      if (c == FRACTION_DELIM) {
        if (base != 10) { return tryCommit(); }
        if (nfrac == 0) { nfrac = 1; }
        break;
      }

      if (c == THOUSAND_DELIM) break;

      // 'e' is a hex digit in '0xE5' and euler's constant in '2e'
      if (base == 10 && (c == 'e' || c == 'E') && hasExponentDigits(m_cursor + 1)) {
        state = State::NumberExponentSign;
        break;
      }

      const auto pos = BASE_CHARS.find(std::tolower(c));

      if (pos == std::string_view::npos || pos >= base) { return tryCommit(); }

      n = n * base + pos;
      if (nfrac) { nfrac += 1; }

      break;
    }
    case State::NumberExponentSign: {
      state = State::NumberExponent;
      if (c == '-') {
        expSign = -1;
        break;
      }
      if (c == '+') break;
      continue;
    }
    case State::NumberExponent: {
      if (!std::isdigit(c)) { return tryCommit(); }
      expValue = expValue * 10 + (c - '0');
      break;
    }
    case State::Operator: {
      if (m_cursor - startPos == 0) {
        switch (c) {
        // none of these take part in a multi-char operator
        case '(':
        case ')':
        case '-':
        case '+':
        case ',':
        case '^':
        case '*':
        case '/':
        case '%':
          ++m_cursor;
          return tryCommit();
        default:
          break;
        }
      }

      if (std::isalnum(c) || std::isspace(c)) { return tryCommit(); }
      break;
    }
    case State::String: {
      if (!std::isalpha(c)) { return tryCommit(); }
      break;
    }
    }

    ++m_cursor;
  }

  return tryCommit();
}

bool Lexer::isWordSequence(std::span<const std::string_view> words) {
  auto old = m_cursor;
  size_t i = 0;

  while (auto tok = peakIf(TokenType::String)) {
    if (i == words.size()) { break; }
    if (tok->raw != words[i]) { break; }
    ++i;
    next();
  }

  m_cursor = old;
  return i == words.size();
}

std::optional<std::string_view> Lexer::peakString(int n) {
  auto old = m_cursor;
  int i = 0;

  while (m_cursor < m_data.size() && std::isspace(m_data[m_cursor])) {
    ++m_cursor;
  }
  auto start = m_cursor;

  while (auto tok = peakIf(TokenType::String)) {
    if (i >= n) { break; }
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

std::optional<Lexer::Token> Lexer::peak(int n) {
  if (m_cursor >= m_data.size()) return std::nullopt;
  auto old = m_cursor;
  std::optional<Token> tok;
  for (int i = 0; i != n + 1; ++i) {
    tok = next();
    if (!tok) break;
  }
  m_cursor = old;
  return tok;
}

std::optional<Lexer::Token> Lexer::peakIf(TokenType type) {
  if (auto tok = peak(); tok && tok->type == type) { return tok; }
  return std::nullopt;
}
