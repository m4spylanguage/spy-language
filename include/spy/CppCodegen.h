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

#ifndef SPY_CPPCODEGEN_H
#define SPY_CPPCODEGEN_H

#include "AST.h"
#include <string>
#include <set>
#include <map>
#include <vector>

namespace spy {

class CppCodegen {
public:
    std::string generate(ASTNode* program);
    std::string get_last_error() const;
    void add_search_path(const std::string& path);
    std::vector<std::string> m_search_paths;
    std::string find_module_file(const std::string& mod) const;

private:
    std::string m_error;
    std::set<std::string> m_spysui_types;
    std::map<std::string, std::string> m_var_types;
    std::map<std::string, std::set<std::string>> m_module_fns;
    std::map<std::string, std::set<std::string>> m_module_from_fns;
    std::string m_module_prefix;
    std::map<std::string, FnStmt*> m_fn_defaults;
    std::set<std::string> m_global_vars;
    std::map<std::string, FnStmt*> m_all_fns;
    std::map<std::string, std::string> m_class_parent_name;
    std::map<std::string, std::set<std::string>> m_class_methods;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_class_fields;
    std::string m_current_class;
    int m_indent = 0;
    bool m_in_main = false;
    bool m_has_spyui_import = false;
    bool m_try_active = false;
    std::map<std::string, std::vector<EnumVariant>> m_enum_variants;
    std::map<std::string, std::string> m_variant_to_enum;

    bool is_spyui_type(const std::string& name) const;
    std::string spy_type_to_cpp(const std::string& spy_type) const;
    std::string type_to_cpp(ASTNode* type_node);
    std::string get_expr_type(ASTNode* node);
    std::string get_ident_name(ASTNode* node);
    bool is_string_expr(ASTNode* node);

    void emit_node(ASTNode* node, std::string& out, int indent);
    void emit_let(LetStmt* node, std::string& out, int indent);
    void emit_fn(FnStmt* node, std::string& out);
    void emit_class(ClassStmt* node, std::string& out);
    void emit_return(ReturnStmt* node, std::string& out, int indent);
    void emit_if(IfStmt* node, std::string& out, int indent);
    void emit_while(WhileStmt* node, std::string& out, int indent);
    void emit_for(ForStmt* node, std::string& out, int indent);
    void emit_print(PrintStmt* node, std::string& out, int indent);
    void emit_expr_stmt(ExprStmt* node, std::string& out, int indent);
    void emit_try(TryStmt* node, std::string& out, int indent);
    void emit_match(MatchStmt* node, std::string& out, int indent);
    void emit_import(ImportStmt* node, std::string& out);
    void emit_struct(StructStmt* node, std::string& out);
    void emit_extern_fn(ExternFnStmt* node, std::string& out);
    void emit_enum(EnumStmt* node, std::string& out);
    void emit_block(const std::vector<ASTPtr>& stmts, std::string& out, int indent);

    std::string emit_expression(ASTNode* expr);
    std::string emit_method_call(MethodCallExpr* node);
    std::string emit_call(CallExpr* node);
    std::string emit_binop(BinOpExpr* node);
    std::string emit_unary(UnaryExpr* node);

    std::string spy_method_to_cpp(const std::string& type, const std::string& method) const;
};

} // namespace spy

#endif
