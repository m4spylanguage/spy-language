/*
 * Spy Programming Language Engine & Compiler
 * Copyright (C) 2026 Valuvajjala Vivek Vardhan Rao
 *
 * Author: Valuvajjala Vivek Vardhan Rao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SPY_LEXER_H
#define SPY_LEXER_H

#include "Token.h"
#include <string>
#include <vector>

namespace spy {

class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<input>");

    std::vector<Token> tokenize();

private:
    std::string m_source;
    std::string m_filename;
    size_t m_pos;
    int m_line;
    int m_column;
    int m_indent_level;
    std::vector<int> m_indent_stack;
    bool m_at_line_start;
    bool m_in_string;
    char m_string_quote;

    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);
    bool match_two(char first, char second);

    void skip_whitespace();
    void skip_comment();
    void skip_multiline_comment();

    Token read_string(char quote);
    Token read_number();
    Token read_identifier_or_keyword();
    Token read_c_header();

    Token make_token(TokenType type, const std::string& value);
    Token make_error(const std::string& message);

    void handle_indent();
    void emit_newline();

    static bool is_digit(char c);
    static bool is_hex_digit(char c);
    static bool is_alpha(char c);
    static bool is_alpha_numeric(char c);
    static TokenType check_keyword(const std::string& word);
};

} // namespace spy

#endif // SPY_LEXER_H
