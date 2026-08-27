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

#ifndef SPY_CODEGEN_H
#define SPY_CODEGEN_H

#include "AST.h"
#include <string>
#include <set>
#include <map>

namespace spy {

struct CapturedVar {
    std::string name;
    std::string c_type;
};

struct LambdaInfo {
    std::string name;
    LambdaExpr* expr;
    std::vector<CapturedVar> captures;
};

class Codegen {
public:
    std::string generate(ASTNode* program);
    std::string get_last_error() const;
    void add_search_path(const std::string& path);
    std::vector<std::string> m_search_paths;
    std::string find_module_file(const std::string& mod) const;

private:
    std::string m_error;
    std::set<std::string> m_declared;
    std::set<std::string> m_string_vars;
    std::set<std::string> m_dict_vars;
    std::set<std::string> m_set_vars;
    std::set<std::string> m_list_vars;
    std::set<std::string> m_named_tuple_vars;
    std::set<std::string> m_classes;
    std::map<std::string, std::string> m_var_class;
    std::map<std::string, std::set<std::string>> m_module_fns;
    std::map<std::string, std::set<std::string>> m_module_from_fns;
    std::set<std::string> m_global_vars;
    std::set<std::string> m_generator_fns;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_class_fields;
    std::map<std::string, std::vector<std::string>> m_class_parent;
    std::map<std::string, std::set<std::string>> m_class_methods;
    std::map<std::string, std::set<std::string>> m_class_static_methods;
    std::map<std::string, std::vector<std::pair<std::string, std::vector<std::string>>>> m_class_method_params;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_class_static_param_types;
    std::set<std::string> m_class_string_params;
    std::string m_current_class;
    int m_lambda_counter = 0;
    std::map<std::string, FnStmt*> m_fn_defaults;
    std::map<std::string, std::set<std::string>> m_fn_string_params;
    std::vector<LambdaInfo> m_lambdas;
    std::set<std::string> m_closure_vars;
    std::map<std::string, FnStmt*> m_all_fns;
    std::map<std::string, std::vector<EnumVariant>> m_enum_variants;
    std::map<std::string, std::string> m_variant_to_enum;
    std::map<std::string, std::string> m_var_types;
    std::map<std::string, std::string> m_fn_return_types;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_struct_defs;
    std::set<std::string> m_structs;

    void collect_field_types(ASTNode* node);
    void collect_call_string_params(ASTNode* node);
    void emit_node(ASTNode* node, std::string& out, int indent);
    void emit_program(Program* node, std::string& out);
    void emit_print(PrintStmt* node, std::string& out, int indent);
    void emit_let(LetStmt* node, std::string& out, int indent);
    void emit_fn(FnStmt* node, std::string& out);
    void emit_return(ReturnStmt* node, std::string& out, int indent);
    void emit_if(IfStmt* node, std::string& out, int indent);
    void emit_while(WhileStmt* node, std::string& out, int indent);
    void emit_import(ImportStmt* node, std::string& out);
    void emit_match(MatchStmt* node, std::string& out, int indent);
    void emit_for(ForStmt* node, std::string& out, int indent);
    void emit_try(TryStmt* node, std::string& out, int indent);
    void emit_class(ClassStmt* node, std::string& out);
    void emit_block(const std::vector<ASTPtr>& stmts, std::string& out, int indent);
    void emit_expr_stmt(ExprStmt* node, std::string& out, int indent);
    std::string emit_expression(ASTNode* expr);
    std::string get_ident_name(ASTNode* expr);
    bool is_string_expr(ASTNode* expr);
    void collect_lambdas(ASTNode* node);
    void scan_lambdas(ASTNode* stmt);
    void find_captures_in_node(ASTNode* node, const std::set<std::string>& params, const std::set<std::string>& locals, std::vector<CapturedVar>& captures);
    std::vector<CapturedVar> find_captures(LambdaExpr* lambda, ASTNode* scope);
    std::string get_var_c_type(const std::string& name);
    std::string get_expr_class(ASTNode* expr);
    int m_loop_else_counter = 0;
    std::vector<bool> m_loop_has_else;
    std::map<std::string, std::set<std::string>> m_class_dunder_returns_struct;
    std::set<std::string> m_ptr_params;
    std::string get_binop_struct_class(ASTNode* expr);
    std::string m_module_prefix;
    std::string type_to_c(ASTNode* type_node);
    void emit_struct(StructStmt* node, std::string& out);
    void emit_extern_fn(ExternFnStmt* node, std::string& out);
    std::string get_var_type(const std::string& name);
    std::string get_struct_field_type(ASTNode* obj, const std::string& member);
};

} // namespace spy

#endif // SPY_CODEGEN_H
