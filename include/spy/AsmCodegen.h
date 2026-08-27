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

#ifndef SPY_ASMCODEGEN_H
#define SPY_ASMCODEGEN_H

#include "AST.h"
#include <string>
#include <set>
#include <map>

namespace spy {

class AsmCodegen {
public:
    std::string generate(ASTNode* program);
    std::string get_last_error() const;
    void add_search_path(const std::string& path);

private:
    std::string m_error;
    std::set<std::string> m_declared;
    std::map<std::string, std::string> m_var_types;
    int m_string_counter = 0;
    std::string m_data;
    std::string m_code;
    std::string m_externs;

    void emit_program(Program* node);
    void emit_fn(FnStmt* node);
    void emit_block(const std::vector<ASTPtr>& stmts);
    void emit_print(PrintStmt* node);
    void emit_let(LetStmt* node);
    void emit_assign(AssignStmt* node);
    std::string emit_expr(ASTNode* expr);
    std::string mangle_name(const std::string& name);
};

} // namespace spy

#endif
