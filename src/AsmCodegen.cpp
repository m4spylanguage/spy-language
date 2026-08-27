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

#include "spy/AsmCodegen.h"
#include "spy/Lexer.h"
#include "spy/Parser.h"
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace spy {

std::string AsmCodegen::generate(ASTNode* program) {
    m_data.clear();
    m_code.clear();
    m_externs.clear();
    m_string_counter = 0;
    m_declared.clear();
    m_var_types.clear();

    m_externs += "option casemap:none\n\n";
    m_externs += "includelib kernel32.lib\n";
    m_externs += "extern GetStdHandle:PROC\n";
    m_externs += "extern WriteFile:PROC\n";
    m_externs += "extern ExitProcess:PROC\n\n";

    emit_program(static_cast<Program*>(program));

    std::string result;
    result += m_externs;
    result += ".data\n";
    result += m_data;
    result += "\n.code\n";
    result += m_code;
    result += "\n; Built-in helpers\n";
    result += "builtin_strlen PROC\n";
    result += "    xor eax, eax\n";
    result += "loop_start:\n";
    result += "    cmp byte ptr [rcx+rax], 0\n";
    result += "    je loop_end\n";
    result += "    inc rax\n";
    result += "    jmp loop_start\n";
    result += "loop_end:\n";
    result += "    ret\n";
    result += "builtin_strlen ENDP\n\n";
    result += "END\n";
    return result;
}

std::string AsmCodegen::get_last_error() const { return m_error; }
void AsmCodegen::add_search_path(const std::string& path) { (void)path; }

std::string AsmCodegen::mangle_name(const std::string& name) {
    return name;
}

void AsmCodegen::emit_program(Program* node) {
    for (auto& stmt : node->statements) {
        if (stmt->type == NodeType::FN_STMT) {
            emit_fn(static_cast<FnStmt*>(stmt.get()));
        }
    }
}

void AsmCodegen::emit_fn(FnStmt* node) {
    m_code += node->name + " PROC\n";
    m_code += "    push rbp\n";
    m_code += "    mov rbp, rsp\n";
    m_code += "    sub rsp, 64\n";  // shadow space + local vars

    emit_block(node->body);

    m_code += "    xor eax, eax\n";
    m_code += "    mov rsp, rbp\n";
    m_code += "    pop rbp\n";
    m_code += "    ret\n";
    m_code += node->name + " ENDP\n\n";
}

void AsmCodegen::emit_block(const std::vector<ASTPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        switch (stmt->type) {
            case NodeType::PRINT_STMT:
                emit_print(static_cast<PrintStmt*>(stmt.get()));
                break;
            case NodeType::LET_STMT:
                emit_let(static_cast<LetStmt*>(stmt.get()));
                break;
            case NodeType::ASSIGN_STMT:
                emit_assign(static_cast<AssignStmt*>(stmt.get()));
                break;
            default:
                break;
        }
    }
}

void AsmCodegen::emit_print(PrintStmt* node) {
    // Write each expression followed by newline using WriteFile
    for (auto& expr : node->expressions) {
        std::string val = emit_expr(expr.get());
        // Get length of string
        m_code += "    lea rcx, " + val + "\n";
        m_code += "    call builtin_strlen\n";
        m_code += "    mov r8, rax\n";  // length in r8
        m_code += "    lea rdx, " + val + "\n";  // buffer in rdx
        m_code += "    mov rcx, -11\n";  // STD_OUTPUT_HANDLE
        m_code += "    call GetStdHandle\n";
        m_code += "    mov rcx, rax\n";  // handle
        // rdx already has buffer
        // r8 already has length
        m_code += "    lea r9, [rsp+32]\n";  // bytes written pointer
        m_code += "    mov qword ptr [rsp+40], 0\n";  // lpOverlapped = NULL
        m_code += "    call WriteFile\n";
    }
    // Write newline
    std::string nl_name = "str_nl" + std::to_string(m_string_counter++);
    m_data += nl_name + " db 13, 10, 0\n";
    m_code += "    lea rcx, " + nl_name + "\n";
    m_code += "    call builtin_strlen\n";
    m_code += "    mov r8, rax\n";
    m_code += "    lea rdx, " + nl_name + "\n";
    m_code += "    mov rcx, -11\n";
    m_code += "    call GetStdHandle\n";
    m_code += "    mov rcx, rax\n";
    m_code += "    sub rsp, 48\n";
    m_code += "    lea r9, [rsp+32]\n";
    m_code += "    mov qword ptr [rsp+40], 0\n";
    m_code += "    call WriteFile\n";
    m_code += "    add rsp, 48\n";
}

void AsmCodegen::emit_let(LetStmt* node) {
    m_declared.insert(node->name);
    if (node->initializer) {
        std::string val = emit_expr(node->initializer.get());
        if (node->initializer->type == NodeType::STRING_EXPR) {
            m_var_types[node->name] = "qword";
            m_code += "    lea rax, " + val + "\n";
            m_code += "    mov " + node->name + ", rax\n";
            // Reserve stack space for the pointer
            m_code += "    push rax\n";
            m_code += "    sub rsp, 8\n";
            m_code += "    mov [rsp], rax\n";
        }
    }
}

void AsmCodegen::emit_assign(AssignStmt* node) {
    std::string val = emit_expr(node->value.get());
    std::string name;
    if (node->target->type == NodeType::IDENT_EXPR) {
        name = static_cast<IdentExpr*>(node->target.get())->name;
    }
    (void)name;
    (void)val;
}

std::string AsmCodegen::emit_expr(ASTNode* expr) {
    if (!expr) return "0";
    if (expr->type == NodeType::STRING_EXPR) {
        auto* s = static_cast<StringExpr*>(expr);
        std::string name = "str" + std::to_string(m_string_counter++);
        m_data += name + " db ";
        for (size_t i = 0; i < s->value.size(); i++) {
            if (i > 0) m_data += ", ";
            char c = s->value[i];
            if (c == '\n') m_data += "10";
            else if (c == '\t') m_data += "9";
            else if (c == '"') m_data += "34";
            else if (c == '\\') m_data += "92";
            else if (c >= 32 && c < 127) m_data += "'" + std::string(1, c) + "'";
            else m_data += std::to_string((int)(unsigned char)c);
        }
        m_data += ", 0\n";
        return name;
    }
    if (expr->type == NodeType::INT_EXPR) {
        auto* n = static_cast<IntExpr*>(expr);
        return std::to_string(n->value);
    }
    if (expr->type == NodeType::FLOAT_EXPR) {
        auto* n = static_cast<FloatExpr*>(expr);
        return std::to_string(n->value);
    }
    return "0";
}

} // namespace spy
