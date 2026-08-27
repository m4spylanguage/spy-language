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

#include "spy/Token.h"
#include <unordered_map>

namespace spy {

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::INT_LIT:       return "INT";
        case TokenType::FLOAT_LIT:     return "FLOAT";
        case TokenType::STRING_LIT:    return "STRING";
        case TokenType::BOOL_LIT:      return "BOOL";
        case TokenType::NONE_LIT:      return "NONE";
        case TokenType::IDENT:         return "IDENT";
        case TokenType::KW_LET:        return "let";
        case TokenType::KW_CONST:      return "const";
        case TokenType::KW_MUT:        return "mut";
        case TokenType::KW_FN:         return "fn";
        case TokenType::KW_RETURN:     return "return";
        case TokenType::KW_IF:         return "if";
        case TokenType::KW_ELSE:       return "else";
        case TokenType::KW_ELIF:       return "elif";
        case TokenType::KW_WHILE:      return "while";
        case TokenType::KW_FOR:        return "for";
        case TokenType::KW_IN:         return "in";
        case TokenType::KW_MATCH:      return "match";
        case TokenType::KW_TRY:        return "try";
        case TokenType::KW_EXCEPT:     return "except";
        case TokenType::KW_CASE:       return "case";
        case TokenType::KW_CLASS:      return "class";
        case TokenType::KW_ENUM:       return "enum";
        case TokenType::KW_EXTENDS:    return "extends";
        case TokenType::KW_SUPER:      return "super";
        case TokenType::KW_IMPORT:     return "import";
        case TokenType::KW_ASYNC:      return "async";
        case TokenType::KW_AWAIT:      return "await";
        case TokenType::KW_BREAK:      return "break";
        case TokenType::KW_CONTINUE:   return "continue";
        case TokenType::KW_PASS:       return "pass";
        case TokenType::KW_AND:        return "and";
        case TokenType::KW_OR:         return "or";
        case TokenType::KW_NOT:        return "not";
        case TokenType::KW_IS:         return "is";
        case TokenType::KW_ASSERT:     return "assert";
        case TokenType::KW_FROM:       return "from";
        case TokenType::KW_GLOBAL:     return "global";
        case TokenType::KW_YIELD:      return "yield";
        case TokenType::KW_STRUCT:     return "struct";
        case TokenType::KW_SIZEOF:     return "sizeof";
        case TokenType::KW_ALLOC:      return "alloc";
        case TokenType::KW_FREE:       return "free";
        case TokenType::KW_REALLOC:    return "realloc";
        case TokenType::KW_ASM:        return "asm";
        case TokenType::KW_EXTERN:     return "extern";
        case TokenType::KW_AS:         return "as";
        case TokenType::KW_VOLATILE:   return "volatile";
        case TokenType::TYPE_I8:       return "i8";
        case TokenType::TYPE_I16:      return "i16";
        case TokenType::TYPE_I32:      return "i32";
        case TokenType::TYPE_I64:      return "i64";
        case TokenType::TYPE_U8:       return "u8";
        case TokenType::TYPE_U16:      return "u16";
        case TokenType::TYPE_U32:      return "u32";
        case TokenType::TYPE_U64:      return "u64";
        case TokenType::TYPE_F32:      return "f32";
        case TokenType::TYPE_F64:      return "f64";
        case TokenType::TYPE_BOOL:     return "bool";
        case TokenType::TYPE_VOID:     return "void";
        case TokenType::TYPE_CHAR:     return "char";
        case TokenType::TYPE_USIZE:    return "usize";
        case TokenType::PLUS:          return "+";
        case TokenType::MINUS:         return "-";
        case TokenType::STAR:          return "*";
        case TokenType::SLASH:         return "/";
        case TokenType::PERCENT:       return "%";
        case TokenType::EQ:            return "=";
        case TokenType::EQ_EQ:         return "==";
        case TokenType::BANG_EQ:       return "!=";
        case TokenType::GT:            return ">";
        case TokenType::LT:            return "<";
        case TokenType::GT_EQ:         return ">=";
        case TokenType::LT_EQ:         return "<=";
        case TokenType::BANG:          return "!";
        case TokenType::PIPE:          return "|";
        case TokenType::PIPE_GT:       return "|>";
        case TokenType::ARROW:         return "=>";
        case TokenType::RARROW:        return "->";
        case TokenType::DOT:           return ".";
        case TokenType::COLON_COLON:   return "::";
        case TokenType::PLUS_EQ:       return "+=";
        case TokenType::MINUS_EQ:      return "-=";
        case TokenType::STAR_EQ:       return "*=";
        case TokenType::SLASH_EQ:      return "/=";
        case TokenType::AT:            return "@";
        case TokenType::AMP:           return "&";
        case TokenType::PIPE_BIT:      return "|";
        case TokenType::CARET:         return "^";
        case TokenType::TILDE:         return "~";
        case TokenType::LT_LT:         return "<<";
        case TokenType::GT_GT:         return ">>";
        case TokenType::HASH:          return "#";
        case TokenType::LPAREN:        return "(";
        case TokenType::RPAREN:        return ")";
        case TokenType::LBRACKET:      return "[";
        case TokenType::RBRACKET:      return "]";
        case TokenType::LBRACE:        return "{";
        case TokenType::RBRACE:        return "}";
        case TokenType::COLON:         return ":";
        case TokenType::COMMA:         return ",";
        case TokenType::SEMICOLON:     return ";";
        case TokenType::NEWLINE:       return "NEWLINE";
        case TokenType::INDENT:        return "INDENT";
        case TokenType::DEDENT:        return "DEDENT";
        case TokenType::EOF_TOKEN:     return "EOF";
        case TokenType::C_HEADER:      return "C_HEADER";
        case TokenType::ERROR:         return "ERROR";
    }
    return "UNKNOWN";
}

bool Token::is_keyword() const {
    return (type >= TokenType::KW_LET && type <= TokenType::KW_VOLATILE) ||
           (type >= TokenType::TYPE_I8 && type <= TokenType::TYPE_USIZE);
}

bool Token::is_operator() const {
    return type >= TokenType::PLUS && type <= TokenType::SLASH_EQ;
}

bool Token::is_literal() const {
    return type >= TokenType::INT_LIT && type <= TokenType::NONE_LIT;
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token(" << token_type_name(token.type);
    if (!token.value.empty()) {
        os << ", \"" << token.value << "\"";
    }
    os << ", line=" << token.line << ", col=" << token.column << ")";
    return os;
}

} // namespace spy
