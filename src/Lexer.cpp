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

#include "spy/Lexer.h"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace spy {

Lexer::Lexer(const std::string& source, const std::string& filename)
    : m_source(source),
      m_filename(filename),
      m_pos(0),
      m_line(1),
      m_column(1),
      m_indent_level(0),
      m_at_line_start(true),
      m_in_string(false),
      m_string_quote(0) {
    m_indent_stack.push_back(0);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (m_pos < m_source.size()) {
        char c = peek();

        if (c == '\n') {
            advance();
            m_line++;
            m_column = 1;
            m_at_line_start = true;
            tokens.push_back(Token(TokenType::NEWLINE, "\\n", m_line - 1, 1));
            continue;
        }

        if (m_at_line_start) {
            int new_indent = 0;
            while (m_pos < m_source.size()) {
                char ch = peek();
                if (ch == ' ') {
                    new_indent++;
                    advance();
                } else if (ch == '\t') {
                    new_indent += 4;
                    advance();
                } else {
                    break;
                }
            }

            char next = peek();
            if (next == '#' || next == '\n' || next == '\0') {
                m_at_line_start = false;
                continue;
            }

            int current = m_indent_stack.back();
            if (new_indent > current) {
                m_indent_stack.push_back(new_indent);
                tokens.push_back(Token(TokenType::INDENT, "INDENT", m_line, m_column));
            } else if (new_indent < current) {
                while (m_indent_stack.back() > new_indent) {
                    m_indent_stack.pop_back();
                    tokens.push_back(Token(TokenType::DEDENT, "DEDENT", m_line, m_column));
                }
                if (m_indent_stack.back() != new_indent) {
                    tokens.push_back(Token(TokenType::ERROR, "indentation error", m_line, m_column));
                }
            }

            m_at_line_start = false;
            continue;
        }

        m_at_line_start = false;

        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
            continue;
        }

        if (c == '#') {
            while (m_pos < m_source.size() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '"') {
            if (peek_next() == '"' && m_pos + 2 < m_source.size() && m_source[m_pos + 2] == '"') {
                advance();
                advance();
                advance();
                while (m_pos < m_source.size()) {
                    if (peek() == '"' && peek_next() == '"' && m_pos + 2 < m_source.size() && m_source[m_pos + 2] == '"') {
                        advance();
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
                continue;
            }
            tokens.push_back(read_string('"'));
            continue;
        }

        if (c == '\'') {
            tokens.push_back(read_string('\''));
            continue;
        }

        if (is_digit(c) || (c == '.' && is_digit(peek_next()))) {
            tokens.push_back(read_number());
            continue;
        }

        if (c == '<' && peek_next() != '=' && peek_next() != '<') {
            size_t saved_pos = m_pos;
            int saved_line = m_line;
            int saved_col = m_column;
            Token tok = read_c_header();
            if (tok.type != TokenType::ERROR) {
                tokens.push_back(tok);
                continue;
            }
            m_pos = saved_pos;
            m_line = saved_line;
            m_column = saved_col;
        }

        if (is_alpha(c) || c == '_') {
            tokens.push_back(read_identifier_or_keyword());
            continue;
        }

        switch (c) {
            case '+':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::PLUS_EQ, "+=", m_line, m_column));
                else tokens.push_back(Token(TokenType::PLUS, "+", m_line, m_column));
                break;
            case '-':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::MINUS_EQ, "-=", m_line, m_column));
                else if (peek() == '>') { advance(); tokens.push_back(Token(TokenType::RARROW, "->", m_line, m_column)); }
                else tokens.push_back(Token(TokenType::MINUS, "-", m_line, m_column));
                break;
            case '*':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::STAR_EQ, "*=", m_line, m_column));
                else tokens.push_back(Token(TokenType::STAR, "*", m_line, m_column));
                break;
            case '/':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::SLASH_EQ, "/=", m_line, m_column));
                else tokens.push_back(Token(TokenType::SLASH, "/", m_line, m_column));
                break;
            case '%':
                advance();
                tokens.push_back(Token(TokenType::PERCENT, "%", m_line, m_column));
                break;
            case '@':
                advance();
                tokens.push_back(Token(TokenType::AT, "@", m_line, m_column));
                break;
            case '#':
                advance();
                tokens.push_back(Token(TokenType::HASH, "#", m_line, m_column));
                break;
            case '=':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::EQ_EQ, "==", m_line, m_column));
                else if (peek() == '>') { advance(); tokens.push_back(Token(TokenType::ARROW, "=>", m_line, m_column)); }
                else tokens.push_back(Token(TokenType::EQ, "=", m_line, m_column));
                break;
            case '!':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::BANG_EQ, "!=", m_line, m_column));
                else tokens.push_back(Token(TokenType::BANG, "!", m_line, m_column));
                break;
            case '>':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::GT_EQ, ">=", m_line, m_column));
                else if (match('>')) tokens.push_back(Token(TokenType::GT_GT, ">>", m_line, m_column));
                else tokens.push_back(Token(TokenType::GT, ">", m_line, m_column));
                break;
            case '<':
                advance();
                if (match('=')) tokens.push_back(Token(TokenType::LT_EQ, "<=", m_line, m_column));
                else if (match('<')) tokens.push_back(Token(TokenType::LT_LT, "<<", m_line, m_column));
                else tokens.push_back(Token(TokenType::LT, "<", m_line, m_column));
                break;
            case '&':
                advance();
                tokens.push_back(Token(TokenType::AMP, "&", m_line, m_column));
                break;
            case '^':
                advance();
                tokens.push_back(Token(TokenType::CARET, "^", m_line, m_column));
                break;
            case '~':
                advance();
                tokens.push_back(Token(TokenType::TILDE, "~", m_line, m_column));
                break;
            case '|':
                advance();
                if (match('>')) tokens.push_back(Token(TokenType::PIPE_GT, "|>", m_line, m_column));
                else tokens.push_back(Token(TokenType::PIPE, "|", m_line, m_column));
                break;
            case '.':
                advance();
                tokens.push_back(Token(TokenType::DOT, ".", m_line, m_column));
                break;
            case ':':
                advance();
                if (match(':')) tokens.push_back(Token(TokenType::COLON_COLON, "::", m_line, m_column));
                else tokens.push_back(Token(TokenType::COLON, ":", m_line, m_column));
                break;
            case '(':
                advance();
                tokens.push_back(Token(TokenType::LPAREN, "(", m_line, m_column));
                break;
            case ')':
                advance();
                tokens.push_back(Token(TokenType::RPAREN, ")", m_line, m_column));
                break;
            case '[':
                advance();
                tokens.push_back(Token(TokenType::LBRACKET, "[", m_line, m_column));
                break;
            case ']':
                advance();
                tokens.push_back(Token(TokenType::RBRACKET, "]", m_line, m_column));
                break;
            case '{':
                advance();
                tokens.push_back(Token(TokenType::LBRACE, "{", m_line, m_column));
                break;
            case '}':
                advance();
                tokens.push_back(Token(TokenType::RBRACE, "}", m_line, m_column));
                break;
            case ',':
                advance();
                tokens.push_back(Token(TokenType::COMMA, ",", m_line, m_column));
                break;
            case ';':
                advance();
                tokens.push_back(Token(TokenType::SEMICOLON, ";", m_line, m_column));
                break;
            default:
                tokens.push_back(Token(TokenType::ERROR, std::string("unexpected '") + c + "'", m_line, m_column));
                advance();
                break;
        }
    }

    while (m_indent_stack.back() > 0) {
        m_indent_stack.pop_back();
        tokens.push_back(Token(TokenType::DEDENT, "DEDENT", m_line, m_column));
    }

    tokens.push_back(Token(TokenType::EOF_TOKEN, "EOF", m_line, m_column));
    return tokens;
}

char Lexer::peek() const {
    if (m_pos >= m_source.size()) return '\0';
    return m_source[m_pos];
}

char Lexer::peek_next() const {
    if (m_pos + 1 >= m_source.size()) return '\0';
    return m_source[m_pos + 1];
}

char Lexer::advance() {
    char c = m_source[m_pos++];
    m_column++;
    return c;
}

bool Lexer::match(char expected) {
    if (m_pos >= m_source.size()) return false;
    if (m_source[m_pos] != expected) return false;
    advance();
    return true;
}

Token Lexer::read_string(char quote) {
    int start_line = m_line;
    int start_col = m_column;
    advance();

    std::string value;
    while (m_pos < m_source.size() && peek() != quote) {
        if (peek() == '\\') {
            advance();
            if (m_pos >= m_source.size()) break;
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                default: value += escaped; break;
            }
        } else {
            char ch = advance();
            value += ch;
            if (ch == '\n') {
                m_line++;
                m_column = 1;
            }
        }
    }

    if (m_pos >= m_source.size()) {
        return Token(TokenType::ERROR, "unterminated string", start_line, start_col);
    }

    advance();
    return Token(TokenType::STRING_LIT, value, start_line, start_col);
}

Token Lexer::read_number() {
    int start_line = m_line;
    int start_col = m_column;
    std::string value;
    bool is_float = false;

    if (m_pos < m_source.size() && peek() == '0' && m_pos + 1 < m_source.size() && (m_source[m_pos + 1] == 'x' || m_source[m_pos + 1] == 'X')) {
        value += advance();
        value += advance();
        while (m_pos < m_source.size() && is_hex_digit(peek())) {
            value += advance();
        }
        return Token(TokenType::INT_LIT, value, start_line, start_col);
    }

    while (m_pos < m_source.size() && is_digit(peek())) {
        value += advance();
    }

    if (m_pos < m_source.size() && peek() == '.' && is_digit(peek_next())) {
        is_float = true;
        value += advance();
        while (m_pos < m_source.size() && is_digit(peek())) {
            value += advance();
        }
    }

    return Token(is_float ? TokenType::FLOAT_LIT : TokenType::INT_LIT, value, start_line, start_col);
}

Token Lexer::read_identifier_or_keyword() {
    int start_line = m_line;
    int start_col = m_column;
    std::string value;

    while (m_pos < m_source.size() && is_alpha_numeric(peek())) {
        value += advance();
    }

    TokenType type = check_keyword(value);
    if (type == TokenType::IDENT) {
        if (value == "True" || value == "False" || value == "true" || value == "false") {
            type = TokenType::BOOL_LIT;
        } else if (value == "None" || value == "null") {
            type = TokenType::NONE_LIT;
        }
    }

    return Token(type, value, start_line, start_col);
}

Token Lexer::read_c_header() {
    int start_line = m_line;
    int start_col = m_column;
    advance();

    std::string value;

    while (m_pos < m_source.size() && peek() != '>' && peek() != '\n' && peek() != '\r') {
        value += advance();
    }

    if (peek() != '>') {
        return Token(TokenType::ERROR, "unterminated header", start_line, start_col);
    }
    advance();

    return Token(TokenType::C_HEADER, value, start_line, start_col);
}

bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lexer::is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_alpha_numeric(char c) {
    return is_alpha(c) || is_digit(c);
}

TokenType Lexer::check_keyword(const std::string& word) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"let",      TokenType::KW_LET},
        {"const",    TokenType::KW_CONST},
        {"mut",      TokenType::KW_MUT},
        {"fn",       TokenType::KW_FN},
        {"return",   TokenType::KW_RETURN},
        {"if",       TokenType::KW_IF},
        {"else",     TokenType::KW_ELSE},
        {"elif",     TokenType::KW_ELIF},
        {"while",    TokenType::KW_WHILE},
        {"for",      TokenType::KW_FOR},
        {"in",       TokenType::KW_IN},
        {"match",    TokenType::KW_MATCH},
        {"try",      TokenType::KW_TRY},
        {"except",   TokenType::KW_EXCEPT},
        {"case",     TokenType::KW_CASE},
        {"class",    TokenType::KW_CLASS},
        {"enum",     TokenType::KW_ENUM},
        {"extends",  TokenType::KW_EXTENDS},
        {"super",    TokenType::KW_SUPER},
        {"import",   TokenType::KW_IMPORT},
        {"async",    TokenType::KW_ASYNC},
        {"await",    TokenType::KW_AWAIT},
        {"break",    TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE},
        {"pass",     TokenType::KW_PASS},
        {"and",      TokenType::KW_AND},
        {"or",       TokenType::KW_OR},
        {"not",      TokenType::KW_NOT},
        {"is",       TokenType::KW_IS},
        {"assert",   TokenType::KW_ASSERT},
        {"from",     TokenType::KW_FROM},
        {"global",   TokenType::KW_GLOBAL},
        {"yield",    TokenType::KW_YIELD},
        {"struct",   TokenType::KW_STRUCT},
        {"sizeof",   TokenType::KW_SIZEOF},
        {"alloc",    TokenType::KW_ALLOC},
        {"free",     TokenType::KW_FREE},
        {"realloc",  TokenType::KW_REALLOC},
        {"asm",      TokenType::KW_ASM},
        {"extern",   TokenType::KW_EXTERN},
        {"as",       TokenType::KW_AS},
        {"volatile", TokenType::KW_VOLATILE},
        {"i8",       TokenType::TYPE_I8},
        {"i16",      TokenType::TYPE_I16},
        {"i32",      TokenType::TYPE_I32},
        {"i64",      TokenType::TYPE_I64},
        {"u8",       TokenType::TYPE_U8},
        {"u16",      TokenType::TYPE_U16},
        {"u32",      TokenType::TYPE_U32},
        {"u64",      TokenType::TYPE_U64},
        {"f32",      TokenType::TYPE_F32},
        {"f64",      TokenType::TYPE_F64},
        {"void",     TokenType::TYPE_VOID},
        {"char",     TokenType::TYPE_CHAR},
        {"usize",    TokenType::TYPE_USIZE},
    };

    auto it = keywords.find(word);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENT;
}

} // namespace spy
