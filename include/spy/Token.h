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

#ifndef SPY_TOKEN_H
#define SPY_TOKEN_H

#include <string>
#include <ostream>

namespace spy {

enum class TokenType {
    // Literals
    INT_LIT,
    FLOAT_LIT,
    STRING_LIT,
    BOOL_LIT,
    NONE_LIT,

    // Identifiers
    IDENT,

    // Keywords
    KW_LET,
    KW_CONST,
    KW_MUT,
    KW_FN,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_ELIF,
    KW_WHILE,
    KW_FOR,
    KW_IN,
    KW_MATCH,
    KW_TRY,
    KW_EXCEPT,
    KW_CASE,
    KW_CLASS,
    KW_ENUM,
    KW_EXTENDS,
    KW_SUPER,
    KW_IMPORT,
    KW_ASYNC,
    KW_AWAIT,
    KW_BREAK,
    KW_CONTINUE,
    KW_PASS,
    KW_AND,
    KW_OR,
    KW_NOT,
    KW_IS,
    KW_ASSERT,
    KW_FROM,
    KW_GLOBAL,
    KW_YIELD,
    KW_STRUCT,
    KW_SIZEOF,
    KW_ALLOC,
    KW_FREE,
    KW_REALLOC,
    KW_ASM,
    KW_VOLATILE,
    KW_EXTERN,
    KW_AS,

    // Type keywords
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_USIZE,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQ,
    EQ_EQ,
    BANG_EQ,
    GT,
    LT,
    GT_EQ,
    LT_EQ,
    BANG,
    PIPE,
    PIPE_GT,
    ARROW,
    RARROW,
    DOT,
    COLON_COLON,
    PLUS_EQ,
    MINUS_EQ,
    STAR_EQ,
    SLASH_EQ,
    AT,
    AMP,
    PIPE_BIT,
    CARET,
    TILDE,
    LT_LT,
    GT_GT,
    HASH,

    // Delimiters
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    COLON,
    COMMA,
    SEMICOLON,

    // Special
    NEWLINE,
    INDENT,
    DEDENT,
    EOF_TOKEN,
    C_HEADER,

    // Error
    ERROR
};

const char* token_type_name(TokenType type);

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token() : type(TokenType::ERROR), line(0), column(0) {}
    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}

    bool is_keyword() const;
    bool is_operator() const;
    bool is_literal() const;
};

std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace spy

#endif // SPY_TOKEN_H
