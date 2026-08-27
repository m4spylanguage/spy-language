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

#ifndef SPY_IRCODEGEN_H
#define SPY_IRCODEGEN_H

#include "AST.h"
#include <string>
#include <map>
#include <set>
#include <vector>
#include <tuple>

namespace spy {

class IrCodegen {
public:
    std::string generate(ASTNode* program);
    std::string get_last_error() const;
    void add_search_path(const std::string& path);

private:
    std::string m_error;
    int m_string_counter = 0;
    int m_temp_counter = 0;
    int m_label_counter = 0;
    int m_fn_decl_counter = 0;
    int m_local_counter = 0;
    bool m_fmt_g_emitted = false;
    bool m_fmt_s_emitted = false;
    bool m_fmt_nl_emitted = false;
    std::string m_ir;
    std::string m_globals;
    std::string m_body;
    std::map<std::string, std::string> m_vars;
    std::set<std::string> m_string_vars;
    std::map<std::string, std::string> m_fn_ret_types;
    std::map<std::string, std::vector<std::string>> m_fn_param_types;
    std::map<std::string, std::string> m_fn_return_types;
    std::map<std::string, int> m_struct_sizes;
    std::map<std::string, std::set<int>> m_fn_string_params;
    std::string m_current_fn_ret_type;

    // struct_name -> vector of (field_name, offset, ir_type)
    std::map<std::string, std::vector<std::tuple<std::string, int, std::string>>> m_struct_fields;
    // var_name -> struct_type_name
    std::map<std::string, std::string> m_struct_ptr_vars;
    // var_name -> ir_type for non-double variables
    std::map<std::string, std::string> m_ptr_vars;

    void emit_program(Program* node);
    void emit_fn(FnStmt* node);
    void emit_fn_decl(FnStmt* node);
    void emit_block(const std::vector<ASTPtr>& stmts);
    void emit_if(IfStmt* node);
    void emit_while(WhileStmt* node);
    void emit_print_expr(ASTNode* expr);
    void emit_let(LetStmt* node);
    void emit_assign(AssignStmt* node);
    std::string emit_expr(ASTNode* expr);
    std::string emit_expr_double(ASTNode* expr);
    std::string emit_string_literal(const std::string& value);
    std::string get_ir_type(const std::string& spy_type);
    std::string get_fmt_g();
    std::string get_fmt_s();
    std::string get_fmt_nl();
    std::string alloc_local(const std::string& name, const std::string& ir_type);

    void collect_string_params(ASTNode* node);
    bool is_string_expr_check(ASTNode* expr);

    // New helpers for INDEX_EXPR, MEMBER_EXPR
    std::string load_var_ptr(const std::string& var_name);
    void store_to_var(const std::string& var_name, const std::string& val, const std::string& ir_type);
    std::string emit_gep_i8(const std::string& ptr, const std::string& offset);
    std::string emit_gep_i8_int(const std::string& ptr, int offset);
    std::string emit_load_double(const std::string& ptr);
    std::string emit_load_ptr(const std::string& ptr);
    std::string emit_fptosi(const std::string& val);
    std::string is_string_expr(ASTNode* expr);
    std::string is_string_type(const std::string& ir_type);
    std::string get_ptr_expr(ASTNode* expr);
    std::string get_struct_type_from_expr(ASTNode* expr);
};

} // namespace spy

#endif
