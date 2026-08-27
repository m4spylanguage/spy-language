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

#ifndef SPY_PARSER_H
#define SPY_PARSER_H

#include "Token.h"
#include "AST.h"
#include <vector>

namespace spy {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    ASTPtr parse();
    ASTPtr parse_expression();

private:
    std::vector<Token> m_tokens;
    size_t m_pos;

    const Token& peek() const;
    const Token& peek_next() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool check_string(const std::string& value) const;
    bool match_string(const std::string& value);
    const Token& expect(TokenType type, const std::string& message);
    const Token& expect_string(const std::string& value, const std::string& message);

    ASTPtr parse_statement();
    ASTPtr parse_print();
    ASTPtr parse_let();
    ASTPtr parse_fn();
    ASTPtr parse_fn(const std::string& decorator);
    ASTPtr parse_return();
    ASTPtr parse_if();
    ASTPtr parse_while();
    ASTPtr parse_import();
    ASTPtr parse_match();
    ASTPtr parse_for();
    ASTPtr parse_class();
    ASTPtr parse_enum();
    ASTPtr parse_struct();
    ASTPtr parse_extern_fn();
    ASTPtr parse_try();
    ASTPtr parse_type();
    std::vector<ASTPtr> parse_block();
    ASTPtr parse_and_or();
    ASTPtr parse_comparison();
    ASTPtr parse_bitwise();
    ASTPtr parse_add_sub();
    ASTPtr parse_mul_div();
    ASTPtr parse_unary();
    ASTPtr parse_primary();
    std::vector<ASTPtr> parse_argument_list();
};

} // namespace spy

#endif // SPY_PARSER_H
