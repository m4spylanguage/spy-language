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

#include "spy/Parser.h"
#include <stdexcept>
#include <sstream>

namespace spy {

Parser::Parser(const std::vector<Token>& tokens)
    : m_tokens(tokens), m_pos(0) {}

ASTPtr Parser::parse() {
    auto program = std::make_unique<Program>();

    while (!check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::NEWLINE)) {
            advance();
            continue;
        }
        program->statements.push_back(parse_statement());
    }

    return program;
}

const Token& Parser::peek() const {
    return m_tokens[m_pos];
}

const Token& Parser::peek_next() const {
    static Token eof(TokenType::EOF_TOKEN, "", 0, 0);
    if (m_pos + 1 < m_tokens.size()) return m_tokens[m_pos + 1];
    return eof;
}

const Token& Parser::advance() {
    return m_tokens[m_pos++];
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check_string(const std::string& value) const {
    return peek().value == value;
}

bool Parser::match_string(const std::string& value) {
    if (check_string(value)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    std::ostringstream oss;
    oss << "line " << peek().line << ", col " << peek().column << ": " << message;
    throw std::runtime_error(oss.str());
}

const Token& Parser::expect_string(const std::string& value, const std::string& message) {
    if (check_string(value)) {
        return advance();
    }
    std::ostringstream oss;
    oss << "line " << peek().line << ", col " << peek().column << ": " << message;
    throw std::runtime_error(oss.str());
}

ASTPtr Parser::parse_statement() {
    while (check(TokenType::NEWLINE)) {
        advance();
    }

    if (check(TokenType::EOF_TOKEN)) {
        return nullptr;
    }

    if (check_string("print")) {
        return parse_print();
    }

    if (check(TokenType::KW_LET)) {
        return parse_let();
    }

    if (check(TokenType::KW_FN)) {
        return parse_fn();
    }

    if (check(TokenType::KW_EXTERN)) {
        return parse_extern_fn();
    }

    if (check(TokenType::AT)) {
        advance();
        const Token& dec_name = expect(TokenType::IDENT, "expected decorator name");
        expect(TokenType::NEWLINE, "expected newline after decorator");
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::KW_FN)) {
            return parse_fn(dec_name.value);
        }
        throw std::runtime_error("expected 'fn' after decorator @" + dec_name.value);
    }

    if (check(TokenType::KW_ASYNC)) {
        advance();
        return parse_fn("async");
    }

    if (check(TokenType::KW_RETURN)) {
        return parse_return();
    }

    if (check(TokenType::KW_IF)) {
        return parse_if();
    }

    if (check(TokenType::KW_WHILE)) {
        return parse_while();
    }

    if (check(TokenType::KW_IMPORT)) {
        return parse_import();
    }

    if (check(TokenType::KW_FROM)) {
        const Token& tok = advance();
        const Token& mod = expect(TokenType::IDENT, "expected module name after 'from'");
        expect(TokenType::KW_IMPORT, "expected 'import' after module name");
        std::vector<std::string> names;
        names.push_back(expect(TokenType::IDENT, "expected name after 'import'").value);
        while (match(TokenType::COMMA)) {
            names.push_back(expect(TokenType::IDENT, "expected name").value);
        }
        return std::make_unique<ImportStmt>(mod.value, names, "", tok.line, tok.column);
    }

    if (check(TokenType::KW_MATCH)) {
        return parse_match();
    }

    if (check(TokenType::KW_FOR)) {
        return parse_for();
    }

    if (check(TokenType::KW_CLASS)) {
        return parse_class();
    }

    if (check(TokenType::KW_ENUM)) {
        return parse_enum();
    }

    if (check(TokenType::KW_STRUCT)) {
        return parse_struct();
    }

    if (check(TokenType::KW_TRY)) {
        return parse_try();
    }

    if (check(TokenType::KW_BREAK)) {
        const Token& tok = advance();
        return std::make_unique<BreakStmt>(tok.line, tok.column);
    }

    if (check(TokenType::KW_CONTINUE)) {
        const Token& tok = advance();
        return std::make_unique<ContinueStmt>(tok.line, tok.column);
    }

    if (check(TokenType::KW_PASS)) {
        const Token& tok = advance();
        return std::make_unique<ExprStmt>(std::make_unique<NoneExpr>(tok.line, tok.column), tok.line, tok.column);
    }

    if (check(TokenType::KW_YIELD)) {
        const Token& tok = advance();
        ASTPtr val = nullptr;
        if (!check(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
            val = parse_expression();
        }
        return std::make_unique<YieldStmt>(std::move(val), tok.line, tok.column);
    }

    if (check(TokenType::KW_ASSERT)) {
        const Token& tok = advance();
        auto cond = parse_expression();
        ASTPtr msg;
        if (match(TokenType::COMMA)) {
            msg = parse_expression();
        }
        std::vector<ASTPtr> body;
        auto err_msg = msg ? std::move(msg) : std::make_unique<StringExpr>("assertion failed", tok.line, tok.column);
        std::vector<ASTPtr> fprintf_args;
        fprintf_args.push_back(std::make_unique<StringExpr>("stderr", tok.line, tok.column));
        fprintf_args.push_back(std::make_unique<StringExpr>("%s\n", tok.line, tok.column));
        fprintf_args.push_back(std::move(err_msg));
        body.push_back(std::make_unique<ExprStmt>(
            std::make_unique<CallExpr>("fprintf", std::move(fprintf_args), tok.line, tok.column), tok.line, tok.column));
        std::vector<ASTPtr> exit_args;
        exit_args.push_back(std::make_unique<IntExpr>(1, tok.line, tok.column));
        body.push_back(std::make_unique<ExprStmt>(
            std::make_unique<CallExpr>("exit", std::move(exit_args), tok.line, tok.column), tok.line, tok.column));
        std::vector<ASTPtr> empty;
        auto neg_cond = std::make_unique<UnaryExpr>("not", std::move(cond), tok.line, tok.column);
        return std::make_unique<IfStmt>(std::move(neg_cond), std::move(body), std::vector<ASTPtr>(), false, tok.line, tok.column);
    }

    if (check(TokenType::KW_GLOBAL)) {
        const Token& tok = advance();
        const Token& name = expect(TokenType::IDENT, "expected variable name after 'global'");
        return std::make_unique<GlobalStmt>(name.value, tok.line, tok.column);
    }

    auto expr = parse_expression();

    if (check(TokenType::EQ)) {
        advance();
        auto val = parse_expression();
        return std::make_unique<AssignStmt>(std::move(expr), std::move(val), expr->line, expr->column);
    }
    if (check(TokenType::PLUS_EQ) || check(TokenType::MINUS_EQ) ||
        check(TokenType::STAR_EQ) || check(TokenType::SLASH_EQ)) {
        std::string op = advance().value;
        op = op.substr(0, 1);
        auto val = parse_expression();
        if (expr->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(expr.get());
            auto bin = std::make_unique<BinOpExpr>(op, std::make_unique<IdentExpr>(id->name, expr->line, expr->column), std::move(val), expr->line, expr->column);
            return std::make_unique<AssignStmt>(std::make_unique<IdentExpr>(id->name, expr->line, expr->column), std::move(bin), expr->line, expr->column);
        } else if (expr->type == NodeType::MEMBER_EXPR) {
            auto* mem = static_cast<MemberExpr*>(expr.get());
            auto obj_copy = std::make_unique<IdentExpr>(static_cast<IdentExpr*>(mem->object.get())->name, expr->line, expr->column);
            auto member = std::make_unique<MemberExpr>(std::move(obj_copy), mem->member, expr->line, expr->column);
            auto lhs = std::make_unique<MemberExpr>(std::make_unique<IdentExpr>(static_cast<IdentExpr*>(mem->object.get())->name, expr->line, expr->column), mem->member, expr->line, expr->column);
            auto bin = std::make_unique<BinOpExpr>(op, std::move(lhs), std::move(val), expr->line, expr->column);
            return std::make_unique<AssignStmt>(std::move(member), std::move(bin), expr->line, expr->column);
        }
    }

    return std::make_unique<ExprStmt>(std::move(expr), expr->line, expr->column);
}

ASTPtr Parser::parse_print() {
    const Token& tok = advance();
    expect(TokenType::LPAREN, "expected '(' after 'print'");
    std::vector<ASTPtr> exprs;
    if (!check(TokenType::RPAREN)) {
        exprs.push_back(parse_expression());
        while (match(TokenType::COMMA)) {
            exprs.push_back(parse_expression());
        }
    }
    expect(TokenType::RPAREN, "expected ')' after print arguments");
    return std::make_unique<PrintStmt>(std::move(exprs), tok.line, tok.column);
}

ASTPtr Parser::parse_let() {
    const Token& tok = advance();
    const Token& name = expect(TokenType::IDENT, "expected variable name");
    ASTPtr type = nullptr;
    if (match(TokenType::COLON)) {
        type = parse_type();
    }
    expect(TokenType::EQ, "expected '=' after variable name");
    auto init = parse_expression();
    return std::make_unique<LetStmt>(name.value, std::move(type), std::move(init), tok.line, tok.column);
}

ASTPtr Parser::parse_fn() {
    return parse_fn("");
}

ASTPtr Parser::parse_fn(const std::string& decorator) {
    const Token& tok = advance();
    const Token& name = expect(TokenType::IDENT, "expected function name");
    // Skip generic type parameters: fn max[T](a, b)
    if (match(TokenType::LBRACKET)) {
        while (!check(TokenType::RBRACKET) && !check(TokenType::EOF_TOKEN)) {
            advance();
        }
        expect(TokenType::RBRACKET, "expected ']' after type parameters");
    }
    expect(TokenType::LPAREN, "expected '(' after function name");

    std::vector<FnParam> typed_params;
    std::vector<std::string> params;
    std::map<std::string, ASTPtr> defaults;
    if (!check(TokenType::RPAREN)) {
        const Token& param = expect(TokenType::IDENT, "expected parameter name");
        ASTPtr param_type = nullptr;
        if (match(TokenType::COLON)) {
            param_type = parse_type();
        }
        typed_params.push_back(FnParam(param.value, std::move(param_type)));
        params.push_back(param.value);
        if (check(TokenType::EQ)) {
            advance();
            defaults[param.value] = parse_expression();
        }
        while (match(TokenType::COMMA)) {
            const Token& p = expect(TokenType::IDENT, "expected parameter name");
            ASTPtr ptype = nullptr;
            if (match(TokenType::COLON)) {
                ptype = parse_type();
            }
            typed_params.push_back(FnParam(p.value, std::move(ptype)));
            params.push_back(p.value);
            if (check(TokenType::EQ)) {
                advance();
                defaults[p.value] = parse_expression();
            }
        }
    }
    expect(TokenType::RPAREN, "expected ')' after parameters");
    ASTPtr ret_type = nullptr;
    if (match(TokenType::RARROW)) {
        ret_type = parse_type();
    }
    expect(TokenType::COLON, "expected ':' after function signature");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    expect(TokenType::INDENT, "expected indented block");

    std::vector<ASTPtr> body;
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        body.push_back(parse_statement());
    }
    if (check(TokenType::DEDENT)) advance();

    return std::make_unique<FnStmt>(name.value, std::move(typed_params), std::move(params), std::move(ret_type), std::move(body), std::move(defaults), decorator, tok.line, tok.column);
}

ASTPtr Parser::parse_return() {
    const Token& tok = advance();
    ASTPtr expr = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
        expr = parse_expression();
    }
    return std::make_unique<ReturnStmt>(std::move(expr), tok.line, tok.column);
}

ASTPtr Parser::parse_if() {
    const Token& tok = advance();
    auto cond = parse_expression();
    expect(TokenType::COLON, "expected ':' after condition");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    auto then_body = parse_block();

    std::vector<ASTPtr> else_body;
    bool has_else = false;

    if (check(TokenType::KW_ELIF)) {
        has_else = true;
        else_body.push_back(parse_if());
    } else if (check(TokenType::KW_ELSE)) {
        has_else = true;
        advance();
        if (check(TokenType::KW_IF) || check(TokenType::KW_ELIF)) {
            else_body.push_back(parse_if());
        } else {
            expect(TokenType::COLON, "expected ':' after 'else'");
            expect(TokenType::NEWLINE, "expected newline after ':'");
            else_body = parse_block();
        }
    }

    return std::make_unique<IfStmt>(std::move(cond), std::move(then_body), std::move(else_body), has_else, tok.line, tok.column);
}

ASTPtr Parser::parse_while() {
    const Token& tok = advance();
    auto cond = parse_expression();
    expect(TokenType::COLON, "expected ':' after condition");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    auto body = parse_block();
    std::vector<ASTPtr> else_body;
    if (check(TokenType::KW_ELSE)) {
        advance();
        expect(TokenType::COLON, "expected ':' after 'else'");
        expect(TokenType::NEWLINE, "expected newline after ':'");
        else_body = parse_block();
    }
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), std::move(else_body), tok.line, tok.column);
}

ASTPtr Parser::parse_import() {
    const Token& tok = advance();
    if (check(TokenType::C_HEADER)) {
        const Token& header = advance();
        return std::make_unique<ImportStmt>(header.value, tok.line, tok.column);
    }
    const Token& mod = expect(TokenType::IDENT, "expected module name or '<header>' after 'import'");
    std::string alias;
    if (match(TokenType::KW_AS)) {
        const Token& a = expect(TokenType::IDENT, "expected alias name after 'as'");
        alias = a.value;
    }
    return std::make_unique<ImportStmt>(mod.value, std::vector<std::string>(), alias, tok.line, tok.column);
}

ASTPtr Parser::parse_match() {
    const Token& tok = advance();
    auto value = parse_expression();
    expect(TokenType::COLON, "expected ':' after match value");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    expect(TokenType::INDENT, "expected indented cases");

    std::vector<MatchCase> cases;
    int default_index = -1;

    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;

        expect(TokenType::KW_CASE, "expected 'case' in match");
        ASTPtr pattern;
        bool is_default = false;
        if (check(TokenType::IDENT) && peek().value == "_") {
            pattern = std::make_unique<NoneExpr>(peek().line, peek().column);
            advance();
            is_default = true;
        } else if ((check(TokenType::IDENT) || check(TokenType::NONE_LIT) || check(TokenType::BOOL_LIT)) && peek_next().type == TokenType::LPAREN) {
            const Token& vn = advance();
            advance();
            std::vector<std::string> bindings;
            while (!check(TokenType::RPAREN)) {
                const Token& b = expect(TokenType::IDENT, "expected binding name");
                bindings.push_back(b.value);
                if (check(TokenType::COMMA)) advance();
            }
            expect(TokenType::RPAREN, "expected ')'");
            pattern = std::make_unique<ConstructorPattern>("", vn.value, std::move(bindings), vn.line, vn.column);
        } else {
            pattern = parse_expression();
        }
        expect(TokenType::COLON, "expected ':' after case pattern");
        expect(TokenType::NEWLINE, "expected newline after ':'");

        std::vector<ASTPtr> body_stmts;
        expect(TokenType::INDENT, "expected indented case body");
        while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
            while (check(TokenType::NEWLINE)) advance();
            if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
            body_stmts.push_back(parse_statement());
        }
        if (check(TokenType::DEDENT)) advance();

        if (is_default) {
            default_index = static_cast<int>(cases.size());
        }
        cases.push_back(MatchCase(std::move(pattern), std::move(body_stmts)));
    }
    if (check(TokenType::DEDENT)) advance();

    return std::make_unique<MatchStmt>(
        std::move(value), std::move(cases), default_index,
        tok.line, tok.column
    );
}

ASTPtr Parser::parse_for() {
    const Token& tok = advance();
    const Token& var = expect(TokenType::IDENT, "expected variable name after 'for'");
    expect(TokenType::KW_IN, "expected 'in' after variable name");

    if (check_string("range")) {
        advance();
        expect(TokenType::LPAREN, "expected '(' after 'range'");
        auto first_arg = parse_expression();
        ASTPtr start;
        ASTPtr end;
        if (match(TokenType::COMMA)) {
            auto second_arg = parse_expression();
            start = std::move(first_arg);
            end = std::move(second_arg);
        } else {
            start = std::make_unique<IntExpr>(0, tok.line, tok.column);
            end = std::move(first_arg);
        }
        expect(TokenType::RPAREN, "expected ')' after range argument");
        expect(TokenType::COLON, "expected ':' after for header");
        expect(TokenType::NEWLINE, "expected newline after ':'");
        auto body = parse_block();
        std::vector<ASTPtr> else_body;
        if (check(TokenType::KW_ELSE)) {
            advance();
            expect(TokenType::COLON, "expected ':' after 'else'");
            expect(TokenType::NEWLINE, "expected newline after ':'");
            else_body = parse_block();
        }
        return std::make_unique<ForStmt>(var.value, std::move(start), std::move(end), std::move(body), std::move(else_body), tok.line, tok.column);
    }

    auto iterable = parse_expression();
    expect(TokenType::COLON, "expected ':' after for header");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    auto body = parse_block();
    std::vector<ASTPtr> else_body;
    if (check(TokenType::KW_ELSE)) {
        advance();
        expect(TokenType::COLON, "expected ':' after 'else'");
        expect(TokenType::NEWLINE, "expected newline after ':'");
        else_body = parse_block();
    }
    return std::make_unique<ForStmt>(var.value, std::move(iterable), std::move(body), std::move(else_body), tok.line, tok.column);
}

ASTPtr Parser::parse_class() {
    const Token& tok = advance();
    const Token& name = expect(TokenType::IDENT, "expected class name");
    // Skip generic type params: class Box[T]:
    if (match(TokenType::LBRACKET)) {
        while (!check(TokenType::RBRACKET) && !check(TokenType::EOF_TOKEN)) {
            advance();
        }
        expect(TokenType::RBRACKET, "expected ']' after type parameters");
    }
    std::vector<std::string> parents;
    if (check(TokenType::LPAREN)) {
        advance();
        const Token& p = expect(TokenType::IDENT, "expected parent class name");
        parents.push_back(p.value);
        while (check(TokenType::COMMA)) {
            advance();
            const Token& pp = expect(TokenType::IDENT, "expected parent class name");
            parents.push_back(pp.value);
        }
        expect(TokenType::RPAREN, "expected ')' after parents");
    } else if (check(TokenType::KW_EXTENDS)) {
        advance();
        const Token& p = expect(TokenType::IDENT, "expected parent class name");
        parents.push_back(p.value);
        while (check(TokenType::COMMA)) {
            advance();
            const Token& pp = expect(TokenType::IDENT, "expected parent class name");
            parents.push_back(pp.value);
        }
    }
    expect(TokenType::COLON, "expected ':' after class name");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    expect(TokenType::INDENT, "expected indented class body");

    std::vector<ASTPtr> methods;
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        if (check(TokenType::KW_PASS)) {
            advance();
            continue;
        }
        methods.push_back(parse_fn());
    }
    if (check(TokenType::DEDENT)) advance();

    return std::make_unique<ClassStmt>(name.value, parents, std::move(methods), tok.line, tok.column);
}

ASTPtr Parser::parse_enum() {
    const Token& tok = advance();
    const Token& name = expect(TokenType::IDENT, "expected enum name");
    expect(TokenType::COLON, "expected ':' after enum name");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    expect(TokenType::INDENT, "expected indented enum values");

    auto accept_ident = [&](const std::string& msg) -> const Token& {
        if (check(TokenType::IDENT) || check(TokenType::NONE_LIT) || check(TokenType::BOOL_LIT))
            return advance();
        return expect(TokenType::IDENT, msg);
    };

    std::vector<EnumVariant> variants;
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        const Token& val = accept_ident("expected enum value name");
        std::vector<EnumField> fields;
        if (check(TokenType::LPAREN)) {
            advance();
            while (!check(TokenType::RPAREN)) {
                const Token& fn = accept_ident("expected field name");
                expect(TokenType::COLON, "expected ':' after field name");
                std::string ft;
                TokenType cur = peek().type;
                if (check(TokenType::IDENT) || (cur >= TokenType::TYPE_I32 && cur <= TokenType::TYPE_USIZE)) {
                    ft = advance().value;
                } else if (check(TokenType::LBRACKET)) {
                    advance();
                    ft = "[" + advance().value + "]";
                    expect(TokenType::RBRACKET, "expected ']'");
                } else {
                    expect(TokenType::IDENT, "expected field type");
                }
                fields.emplace_back(fn.value, ft);
                if (check(TokenType::COMMA)) advance();
            }
            expect(TokenType::RPAREN, "expected ')' after fields");
        }
        variants.emplace_back(val.value, fields);
    }
    if (check(TokenType::DEDENT)) advance();

    return std::make_unique<EnumStmt>(name.value, std::move(variants), tok.line, tok.column);
}

ASTPtr Parser::parse_try() {
    const Token& tok = advance();
    expect(TokenType::COLON, "expected ':' after 'try'");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    auto body = parse_block();

    std::vector<ASTPtr> handlers;
    while (check(TokenType::KW_EXCEPT)) {
        advance();
        std::string var_name;
        if (match_string("as")) {
            const Token& v = expect(TokenType::IDENT, "expected variable name after 'as'");
            var_name = v.value;
        }
        expect(TokenType::COLON, "expected ':' after 'except'");
        expect(TokenType::NEWLINE, "expected newline after ':'");
        auto handler_body = parse_block();
        handlers.push_back(std::make_unique<ExceptHandler>(var_name, std::move(handler_body), tok.line, tok.column));
    }

    return std::make_unique<TryStmt>(std::move(body), std::move(handlers), tok.line, tok.column);
}

std::vector<ASTPtr> Parser::parse_block() {
    expect(TokenType::INDENT, "expected indented block");
    std::vector<ASTPtr> stmts;
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        stmts.push_back(parse_statement());
    }
    if (check(TokenType::DEDENT)) advance();
    return stmts;
}

ASTPtr Parser::parse_expression() {
    auto left = parse_and_or();

    while (check(TokenType::PIPE_GT)) {
        advance();
        auto right = parse_and_or();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<PipeExpr>(std::move(left), std::move(right), line, col);
    }

    if (check_string("if")) {
        int line = left->line;
        int col = left->column;
        advance();
        auto condition = parse_expression();
        expect_string("else", "expected 'else' after condition");
        auto else_expr = parse_expression();
        return std::make_unique<TernaryExpr>(std::move(condition), std::move(left), std::move(else_expr), line, col);
    }

    return left;
}

ASTPtr Parser::parse_and_or() {
    auto left = parse_comparison();

    while (check(TokenType::KW_AND) || check(TokenType::KW_OR)) {
        std::string op = advance().value;
        auto right = parse_comparison();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<BinOpExpr>(op, std::move(left), std::move(right), line, col);
    }

    return left;
}

ASTPtr Parser::parse_bitwise() {
    auto left = parse_add_sub();

    while (check(TokenType::AMP) || check(TokenType::PIPE) ||
           check(TokenType::CARET) || check(TokenType::LT_LT) ||
           check(TokenType::GT_GT)) {
        std::string op = advance().value;
        auto right = parse_add_sub();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<BinOpExpr>(op, std::move(left), std::move(right), line, col);
    }

    return left;
}

ASTPtr Parser::parse_comparison() {
    auto left = parse_bitwise();

    while (check(TokenType::GT) || check(TokenType::LT) ||
           check(TokenType::GT_EQ) || check(TokenType::LT_EQ) ||
           check(TokenType::EQ_EQ) || check(TokenType::BANG_EQ) ||
           check(TokenType::KW_IS) || check(TokenType::KW_IN)) {
        std::string op = advance().value;
        auto right = parse_add_sub();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<BinOpExpr>(op, std::move(left), std::move(right), line, col);
    }

    return left;
}

ASTPtr Parser::parse_add_sub() {
    auto left = parse_mul_div();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = advance().value;
        auto right = parse_mul_div();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<BinOpExpr>(op, std::move(left), std::move(right), line, col);
    }

    return left;
}

ASTPtr Parser::parse_unary() {
    if (check(TokenType::KW_NOT)) {
        const Token& tok = advance();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>("not", std::move(operand), tok.line, tok.column);
    }
    if (check(TokenType::MINUS)) {
        const Token& tok = advance();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>("-", std::move(operand), tok.line, tok.column);
    }
    if (check(TokenType::TILDE)) {
        const Token& tok = advance();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>("~", std::move(operand), tok.line, tok.column);
    }
    if (check(TokenType::AMP)) {
        const Token& tok = advance();
        auto operand = parse_unary();
        return std::make_unique<AddressOfExpr>(std::move(operand), tok.line, tok.column);
    }
    if (check(TokenType::STAR)) {
        const Token& tok = advance();
        auto operand = parse_unary();
        return std::make_unique<DerefExpr>(std::move(operand), tok.line, tok.column);
    }
    return parse_primary();
}

ASTPtr Parser::parse_mul_div() {
    auto left = parse_unary();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        std::string op = advance().value;
        auto right = parse_unary();
        int line = left->line;
        int col = left->column;
        left = std::make_unique<BinOpExpr>(op, std::move(left), std::move(right), line, col);
    }

    return left;
}

ASTPtr Parser::parse_primary() {
    if (check(TokenType::INT_LIT)) {
        const Token& tok = advance();
        long long value = std::stoll(tok.value, nullptr, 0);
        return std::make_unique<IntExpr>(value, tok.line, tok.column);
    }

    if (check(TokenType::FLOAT_LIT)) {
        const Token& tok = advance();
        double value = std::stod(tok.value);
        return std::make_unique<FloatExpr>(value, tok.line, tok.column);
    }

    if (check(TokenType::STRING_LIT)) {
        const Token& tok = advance();
        return std::make_unique<StringExpr>(tok.value, tok.line, tok.column);
    }

    if (check(TokenType::BOOL_LIT)) {
        const Token& tok = advance();
        bool value = (tok.value == "True" || tok.value == "true");
        return std::make_unique<BoolExpr>(value, tok.line, tok.column);
    }

    if (check(TokenType::NONE_LIT)) {
        const Token& tok = advance();
        return std::make_unique<NoneExpr>(tok.line, tok.column);
    }

    if (check(TokenType::KW_SIZEOF)) {
        const Token& tok = advance();
        expect(TokenType::LPAREN, "expected '(' after 'sizeof'");
        auto type = parse_type();
        expect(TokenType::RPAREN, "expected ')' after type");
        return std::make_unique<SizeofExpr>(std::move(type), tok.line, tok.column);
    }

    if (check(TokenType::KW_ALLOC)) {
        const Token& tok = advance();
        expect(TokenType::LBRACKET, "expected '[' after 'alloc'");
        auto type = parse_type();
        expect(TokenType::RBRACKET, "expected ']'");
        expect(TokenType::LPAREN, "expected '(' after 'alloc[T]'");
        auto count = parse_expression();
        expect(TokenType::RPAREN, "expected ')'");
        std::vector<ASTPtr> args;
        args.push_back(std::move(count));
        args.push_back(std::move(type));
        return std::make_unique<CallExpr>("alloc", std::move(args), tok.line, tok.column);
    }

    if (check(TokenType::KW_FREE)) {
        const Token& tok = advance();
        expect(TokenType::LPAREN, "expected '(' after 'free'");
        auto ptr = parse_expression();
        expect(TokenType::RPAREN, "expected ')'");
        std::vector<ASTPtr> args;
        args.push_back(std::move(ptr));
        return std::make_unique<CallExpr>("free", std::move(args), tok.line, tok.column);
    }

    if (check(TokenType::KW_AWAIT)) {
        advance();
        return parse_expression();
    }

    if (check(TokenType::KW_REALLOC)) {
        const Token& tok = advance();
        expect(TokenType::LPAREN, "expected '(' after 'realloc'");
        auto ptr = parse_expression();
        expect(TokenType::COMMA, "expected ','");
        auto count = parse_expression();
        expect(TokenType::COMMA, "expected ','");
        auto type = parse_type();
        expect(TokenType::RPAREN, "expected ')'");
        std::vector<ASTPtr> args;
        args.push_back(std::move(ptr));
        args.push_back(std::move(count));
        args.push_back(std::move(type));
        return std::make_unique<CallExpr>("realloc", std::move(args), tok.line, tok.column);
    }

    if (check(TokenType::KW_SUPER)) {
        const Token& tok = advance();
        expect(TokenType::DOT, "expected '.' after 'super'");
        const Token& method = expect(TokenType::IDENT, "expected method name after 'super.'");
        expect(TokenType::LPAREN, "expected '(' after method name");
        std::vector<ASTPtr> args;
        if (!check(TokenType::RPAREN)) {
            args.push_back(parse_expression());
            while (match(TokenType::COMMA)) {
                args.push_back(parse_expression());
            }
        }
        expect(TokenType::RPAREN, "expected ')' after arguments");
        return std::make_unique<SuperMethodCallExpr>(method.value, std::move(args), tok.line, tok.column);
    }

    if (check(TokenType::IDENT)) {
        const Token& tok = advance();
        std::string func_name = tok.value;
        if (check(TokenType::COLON_COLON)) {
            advance();
            const Token& mem = expect(TokenType::IDENT, "expected member name after '::'");
            func_name = func_name + "_" + mem.value;
        }
        if (check(TokenType::LPAREN)) {
            advance();
            auto args = parse_argument_list();
            expect(TokenType::RPAREN, "expected ')' after arguments");
            return std::make_unique<CallExpr>(func_name, std::move(args), tok.line, tok.column);
        }
        if (check(TokenType::LBRACKET)) {
            advance();
            while (!check(TokenType::RBRACKET) && !check(TokenType::EOF_TOKEN)) advance();
            expect(TokenType::RBRACKET, "expected ']'");
        }
        if (check(TokenType::LBRACE)) {
            advance();
            std::vector<ASTPtr> args;
            if (!check(TokenType::RBRACE)) {
                args.push_back(parse_expression());
                while (match(TokenType::COMMA)) {
                    args.push_back(parse_expression());
                }
            }
            expect(TokenType::RBRACE, "expected '}' after struct literal");
            return std::make_unique<CallExpr>(tok.value, std::move(args), tok.line, tok.column);
        }
        ASTPtr expr = std::make_unique<IdentExpr>(tok.value, tok.line, tok.column);
        while (check(TokenType::DOT) || check(TokenType::COLON_COLON) || check(TokenType::LBRACKET)) {
            if (check(TokenType::COLON_COLON)) {
                advance();
                const Token& member = expect(TokenType::IDENT, "expected member name after '::'");
                expr = std::make_unique<IdentExpr>(tok.value + "_" + member.value, tok.line, tok.column);
            } else if (check(TokenType::DOT)) {
                advance();
                const Token& field = expect(TokenType::IDENT, "expected member name");
                if (check(TokenType::LPAREN)) {
                    advance();
                    std::vector<ASTPtr> args;
                    if (!check(TokenType::RPAREN)) {
                        args.push_back(parse_expression());
                        while (match(TokenType::COMMA)) {
                            args.push_back(parse_expression());
                        }
                    }
                    expect(TokenType::RPAREN, "expected ')' after arguments");
                    expr = std::make_unique<MethodCallExpr>(std::move(expr), field.value, std::move(args), tok.line, tok.column);
                } else {
                    expr = std::make_unique<MemberExpr>(std::move(expr), field.value, tok.line, tok.column);
                }
            } else {
                advance();
                auto idx = parse_expression();
                expect(TokenType::RBRACKET, "expected ']'");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idx), tok.line, tok.column);
            }
        }
        return expr;
    }

    if (check(TokenType::LPAREN)) {
        const Token& tok = advance();
        if (check(TokenType::IDENT)) {
            size_t saved = m_pos;
            advance();
            if (check(TokenType::COLON)) {
                advance();
                if (!check(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
                    m_pos = saved;
                    std::vector<std::string> names;
                    std::vector<ASTPtr> vals;
                    names.push_back(expect(TokenType::IDENT, "expected field name").value);
                    expect(TokenType::COLON, "expected ':'");
                    vals.push_back(parse_expression());
                    while (match(TokenType::COMMA)) {
                        names.push_back(expect(TokenType::IDENT, "expected field name").value);
                        expect(TokenType::COLON, "expected ':'");
                        vals.push_back(parse_expression());
                    }
                    expect(TokenType::RPAREN, "expected ')'");
                    return std::make_unique<NamedTupleExpr>(std::move(names), std::move(vals), tok.line, tok.column);
                }
                m_pos = saved;
            }
            m_pos = saved;
        }
        if (check(TokenType::RPAREN) || check(TokenType::IDENT)) {
            std::vector<std::string> params;
            if (!check(TokenType::RPAREN)) {
                params.push_back(expect(TokenType::IDENT, "expected parameter name").value);
                while (match(TokenType::COMMA)) {
                    params.push_back(expect(TokenType::IDENT, "expected parameter name").value);
                }
            }
            expect(TokenType::RPAREN, "expected ')'");
            if (match(TokenType::ARROW)) {
                auto body_expr = parse_expression();
                std::vector<ASTPtr> body;
                body.push_back(std::make_unique<ReturnStmt>(std::move(body_expr), body_expr->line, body_expr->column));
                return std::make_unique<LambdaExpr>(std::move(params), std::move(body), tok.line, tok.column);
            }
            if (check(TokenType::COLON)) {
                advance();
                std::vector<ASTPtr> body;
                while (!check(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
                    body.push_back(parse_statement());
                    if (check(TokenType::NEWLINE)) advance();
                }
                return std::make_unique<LambdaExpr>(std::move(params), std::move(body), tok.line, tok.column);
            }
        }
        auto expr = parse_expression();
        expect(TokenType::RPAREN, "expected ')'");
        return expr;
    }

    if (check(TokenType::LBRACKET)) {
        const Token& tok = advance();
        auto first = parse_expression();
        if (check_string("for")) {
            advance();
            const Token& var = expect(TokenType::IDENT, "expected variable name after 'for'");
            expect_string("in", "expected 'in' after variable name");
            auto iterable = parse_expression();
            expect(TokenType::RBRACKET, "expected ']'");
            return std::make_unique<ListCompExpr>(std::move(first), var.value, std::move(iterable), tok.line, tok.column);
        }
        std::vector<ASTPtr> elements;
        elements.push_back(std::move(first));
        while (match(TokenType::COMMA)) {
            elements.push_back(parse_expression());
        }
        expect(TokenType::RBRACKET, "expected ']'");
        return std::make_unique<ArrayExpr>(std::move(elements), tok.line, tok.column);
    }

    if (check(TokenType::LBRACE)) {
        const Token& tok = advance();
        std::vector<ASTPtr> keys;
        std::vector<ASTPtr> values;
        if (!check(TokenType::RBRACE)) {
            auto key = parse_expression();
            expect(TokenType::COLON, "expected ':' in dict literal");
            auto val = parse_expression();
            keys.push_back(std::move(key));
            values.push_back(std::move(val));
            while (match(TokenType::COMMA)) {
                auto k = parse_expression();
                expect(TokenType::COLON, "expected ':' in dict literal");
                auto v = parse_expression();
                keys.push_back(std::move(k));
                values.push_back(std::move(v));
            }
        }
        expect(TokenType::RBRACE, "expected '}'");
        return std::make_unique<DictExpr>(std::move(keys), std::move(values), tok.line, tok.column);
    }

    std::ostringstream oss;
    oss << "line " << peek().line << ", col " << peek().column
        << ": unexpected token '" << peek().value << "'";
    throw std::runtime_error(oss.str());
}

std::vector<ASTPtr> Parser::parse_argument_list() {
    std::vector<ASTPtr> args;

    if (check(TokenType::RPAREN)) {
        return args;
    }

    args.push_back(parse_expression());
    while (match(TokenType::COMMA)) {
        args.push_back(parse_expression());
    }

    return args;
}

ASTPtr Parser::parse_type() {
    const Token& tok = peek();
    if (match(TokenType::STAR)) {
        auto base = parse_type();
        return std::make_unique<PtrTypeExpr>(std::move(base), tok.line, tok.column);
    }
    if (match(TokenType::LBRACKET)) {
        auto elem_type = parse_type();
        ASTPtr size = nullptr;
        if (match(TokenType::SEMICOLON)) {
            size = parse_expression();
        }
        expect(TokenType::RBRACKET, "expected ']' after array type");
        return std::make_unique<ArrayTypeExpr>(std::move(elem_type), std::move(size), tok.line, tok.column);
    }
    if (check(TokenType::TYPE_I8) || check(TokenType::TYPE_I16) || check(TokenType::TYPE_I32) ||
        check(TokenType::TYPE_I64) || check(TokenType::TYPE_U8) || check(TokenType::TYPE_U16) ||
        check(TokenType::TYPE_U32) || check(TokenType::TYPE_U64) || check(TokenType::TYPE_F32) ||
        check(TokenType::TYPE_F64) || check(TokenType::TYPE_BOOL) || check(TokenType::TYPE_VOID) ||
        check(TokenType::TYPE_CHAR) || check(TokenType::TYPE_USIZE)) {
        const Token& t = advance();
        return std::make_unique<TypeExpr>(t.value, t.line, t.column);
    }
    if (check(TokenType::IDENT)) {
        const Token& t = advance();
        return std::make_unique<TypeExpr>(t.value, t.line, t.column);
    }
    std::ostringstream oss;
    oss << "line " << tok.line << ", col " << tok.column << ": expected type";
    throw std::runtime_error(oss.str());
}

ASTPtr Parser::parse_struct() {
    const Token& tok = advance();
    const Token& name = expect(TokenType::IDENT, "expected struct name");
    expect(TokenType::COLON, "expected ':' after struct name");
    expect(TokenType::NEWLINE, "expected newline after ':'");
    expect(TokenType::INDENT, "expected indented struct body");

    std::vector<StructField> fields;
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        while (check(TokenType::NEWLINE)) advance();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        const Token& field_name = expect(TokenType::IDENT, "expected field name");
        expect(TokenType::COLON, "expected ':' after field name");
        auto field_type = parse_type();
        fields.push_back(StructField(field_name.value, std::move(field_type)));
    }
    if (check(TokenType::DEDENT)) advance();

    return std::make_unique<StructStmt>(name.value, std::move(fields), tok.line, tok.column);
}

ASTPtr Parser::parse_extern_fn() {
    const Token& tok = advance();
    expect(TokenType::KW_FN, "expected 'fn' after 'extern'");
    const Token& name = expect(TokenType::IDENT, "expected function name after 'extern fn'");
    expect(TokenType::LPAREN, "expected '(' after function name");

    std::vector<FnParam> typed_params;
    if (!check(TokenType::RPAREN)) {
        const Token& param = expect(TokenType::IDENT, "expected parameter name");
        ASTPtr param_type = nullptr;
        if (match(TokenType::COLON)) {
            param_type = parse_type();
        }
        typed_params.push_back(FnParam(param.value, std::move(param_type)));
        while (match(TokenType::COMMA)) {
            const Token& p = expect(TokenType::IDENT, "expected parameter name");
            ASTPtr ptype = nullptr;
            if (match(TokenType::COLON)) {
                ptype = parse_type();
            }
            typed_params.push_back(FnParam(p.value, std::move(ptype)));
        }
    }
    expect(TokenType::RPAREN, "expected ')' after parameters");

    ASTPtr ret_type = nullptr;
    if (match(TokenType::RARROW)) {
        ret_type = parse_type();
    }

    std::string header;
    if (match(TokenType::STRING_LIT)) {
        header = m_tokens[m_pos - 1].value;
    }

    return std::make_unique<ExternFnStmt>(name.value, std::move(typed_params), std::move(ret_type), header, tok.line, tok.column);
}

} // namespace spy
