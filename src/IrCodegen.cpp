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

#include "spy/IrCodegen.h"
#include "spy/Lexer.h"
#include "spy/Parser.h"
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace spy {

std::string IrCodegen::generate(ASTNode* program) {
    m_ir.clear();
    m_globals.clear();
    m_body.clear();
    m_string_counter = 0;
    m_temp_counter = 0;
    m_label_counter = 0;
    m_vars.clear();
    m_string_vars.clear();
    m_fn_ret_types.clear();
    m_fn_param_types.clear();
    m_struct_sizes.clear();
    m_struct_fields.clear();
    m_struct_ptr_vars.clear();
    m_ptr_vars.clear();
    m_fn_string_params.clear();
    m_local_counter = 0;
    m_current_fn_ret_type = "double";
    m_fmt_g_emitted = false;
    m_fmt_s_emitted = false;
    m_fmt_nl_emitted = false;

    m_ir += "declare i32 @printf(ptr noundef, ...)\n";
    m_ir += "declare double @fmod(double, double)\n";
    m_ir += "declare i32 @strcmp(ptr, ptr)\n";
    m_ir += "declare ptr @spy_substr(ptr, i32, i32)\n";
    m_ir += "declare ptr @spy_chr(i32)\n";
    m_ir += "declare ptr @spy_read_file(ptr)\n";
    m_ir += "declare ptr @spy_strcat(ptr, ptr)\n";
    m_ir += "declare ptr @spy_format(ptr, ...)\n";
    m_ir += "declare ptr @spy_alloc(i32)\n";
    m_ir += "declare double @spy_strlen(ptr)\n";
    m_ir += "declare ptr @spy_str_from_double(double)\n\n";

    emit_program(static_cast<Program*>(program));

    m_ir += m_globals;
    m_ir += "\n";
    m_ir += m_body;
    return m_ir;
}

std::string IrCodegen::get_last_error() const { return m_error; }
void IrCodegen::add_search_path(const std::string& path) { (void)path; }

bool is_string_returning_builtin(const std::string& callee) {
    return callee == "str" || callee == "chr" || callee == "substr" ||
           callee == "read_file" || callee == "spy_strcat" || callee == "spy_format";
}

int type_size(ASTNode* type_node) {
    if (!type_node) return 8;
    if (type_node->type == NodeType::TYPE_EXPR) {
        auto* t = static_cast<TypeExpr*>(type_node);
        std::string n = t->name;
        if (n == "i32" || n == "i64") return 8;
        if (n == "double" || n == "f64") return 8;
        if (n == "char" || n == "string" || n == "*char") return 8;
        if (n == "bool") return 8;
        return 8;
    }
    if (type_node->type == NodeType::PTR_TYPE_EXPR) return 8;
    return 8;
}

std::string type_to_ir(ASTNode* type_node) {
    if (!type_node) return "double";
    if (type_node->type == NodeType::TYPE_EXPR) {
        auto* t = static_cast<TypeExpr*>(type_node);
        std::string n = t->name;
        if (n == "i32") return "i32";
        if (n == "i64") return "i64";
        if (n == "double" || n == "f64") return "double";
        if (n == "char" || n == "string" || n == "*char") return "ptr";
        return "double";
    }
    if (type_node->type == NodeType::PTR_TYPE_EXPR) {
        return "ptr";
    }
    return "double";
}

std::string type_node_name(ASTNode* type_node) {
    if (!type_node) return "";
    if (type_node->type == NodeType::TYPE_EXPR) {
        return static_cast<TypeExpr*>(type_node)->name;
    }
    if (type_node->type == NodeType::PTR_TYPE_EXPR) {
        auto* p = static_cast<PtrTypeExpr*>(type_node);
        std::string base = type_node_name(p->base_type.get());
        if (base.empty()) return "ptr";
        return "*" + base;
    }
    return "";
}

void IrCodegen::collect_string_params(ASTNode* node) {
    if (!node) return;
    if (node->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node);
        for (size_t i = 0; i < call->args.size(); ++i) {
            if (is_string_expr_check(call->args[i].get())) {
                m_fn_string_params[call->callee].insert((int)i);
            }
        }
    }
    // Recurse into children
    if (node->type == NodeType::PROGRAM) {
        auto* p = static_cast<Program*>(node);
        for (auto& s : p->statements) collect_string_params(s.get());
    } else if (node->type == NodeType::PRINT_STMT) {
        auto* p = static_cast<PrintStmt*>(node);
        for (auto& e : p->expressions) collect_string_params(e.get());
    } else if (node->type == NodeType::LET_STMT) {
        auto* l = static_cast<LetStmt*>(node);
        // Pre-populate m_string_vars from let statements with string init or *char type
        bool is_str = false;
        if (l->initializer && l->initializer->type == NodeType::STRING_EXPR) {
            is_str = true;
        } else if (l->initializer && l->initializer->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(l->initializer.get());
            if (is_string_returning_builtin(call->callee)) is_str = true;
        } else if (l->type_annotation && type_to_ir(l->type_annotation.get()) == "ptr") {
            is_str = true;
        }
        if (is_str) m_string_vars.insert(l->name);
        collect_string_params(l->initializer.get());
    } else if (node->type == NodeType::ASSIGN_STMT) {
        auto* a = static_cast<AssignStmt*>(node);
        collect_string_params(a->value.get());
    } else if (node->type == NodeType::RETURN_STMT) {
        auto* r = static_cast<ReturnStmt*>(node);
        collect_string_params(r->expression.get());
    } else if (node->type == NodeType::IF_STMT) {
        auto* i = static_cast<IfStmt*>(node);
        collect_string_params(i->condition.get());
        for (auto& s : i->then_body) collect_string_params(s.get());
        for (auto& s : i->else_body) collect_string_params(s.get());
    } else if (node->type == NodeType::WHILE_STMT) {
        auto* w = static_cast<WhileStmt*>(node);
        collect_string_params(w->condition.get());
        for (auto& s : w->body) collect_string_params(s.get());
    } else if (node->type == NodeType::FN_STMT) {
        auto* f = static_cast<FnStmt*>(node);
        // Pre-populate string vars from function params with type annotations
        for (size_t i = 0; i < f->params.size() && i < f->typed_params.size(); ++i) {
            if (f->typed_params[i].type) {
                std::string ir_t = type_to_ir(f->typed_params[i].type.get());
                if (ir_t == "ptr") {
                    std::string tn = type_node_name(f->typed_params[i].type.get());
                    if (tn == "*char" || tn == "ptr" || tn == "string" || tn == "char") {
                        m_string_vars.insert(f->params[i]);
                    }
                }
            }
        }
        for (auto& s : f->body) collect_string_params(s.get());
    } else if (node->type == NodeType::EXPR_STMT) {
        auto* es = static_cast<ExprStmt*>(node);
        collect_string_params(es->expression.get());
    } else if (node->type == NodeType::BINOP_EXPR) {
        auto* b = static_cast<BinOpExpr*>(node);
        collect_string_params(b->left.get());
        collect_string_params(b->right.get());
    }
}

bool IrCodegen::is_string_expr_check(ASTNode* expr) {
    if (!expr) return false;
    if (expr->type == NodeType::STRING_EXPR) return true;
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        return m_string_vars.count(id->name) > 0;
    }
    if (expr->type == NodeType::CALL_EXPR) {
        auto* ce = static_cast<CallExpr*>(expr);
        return is_string_returning_builtin(ce->callee);
    }
    if (expr->type == NodeType::MEMBER_EXPR) {
        auto* me = static_cast<MemberExpr*>(expr);
        std::string st = get_struct_type_from_expr(me->object.get());
        if (!st.empty()) {
            auto fit = m_struct_fields.find(st);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        return ft == "*char" || ft == "ptr" || ft == "string";
                    }
                }
            }
        }
        return false;
    }
    if (expr->type == NodeType::BINOP_EXPR) {
        auto* be = static_cast<BinOpExpr*>(expr);
        if (be->op == "+") {
            return is_string_expr_check(be->left.get()) || is_string_expr_check(be->right.get());
        }
        return false;
    }
    return false;
}

void IrCodegen::emit_program(Program* node) {
    // Pre-pass: collect string params
    collect_string_params(node);

    // First pass: record struct sizes, fields, and local function signatures
    std::set<std::string> local_fns;
    for (auto& stmt : node->statements) {
        if (stmt->type == NodeType::STRUCT_STMT) {
            auto* s = static_cast<StructStmt*>(stmt.get());
            int total = 0;
            std::vector<std::tuple<std::string, int, std::string>> fields;
            for (auto& f : s->fields) {
                std::string ft = type_node_name(f.type.get());
                fields.emplace_back(f.name, total, ft);
                total += type_size(f.type.get());
            }
            m_struct_sizes[s->name] = total;
            m_struct_fields[s->name] = fields;
        }
    }
    for (auto& stmt : node->statements) {
        if (stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            local_fns.insert(fn->name);
            m_fn_ret_types[fn->name] = "double";
            // Track return type from annotation
            if (fn->return_type) {
                std::string rt = type_to_ir(fn->return_type.get());
                if (rt == "ptr") {
                    m_fn_return_types[fn->name] = "ptr";
                }
            }
            // Track ptr-typed params from type annotations
            for (size_t pi = 0; pi < fn->typed_params.size(); ++pi) {
                if (fn->typed_params[pi].type) {
                    std::string ir_t = type_to_ir(fn->typed_params[pi].type.get());
                    if (ir_t == "ptr") {
                        m_fn_string_params[fn->name].insert((int)pi);
                    }
                }
            }
        }
    }
    // Second pass: emit function bodies
    bool has_main = false;
    for (auto& stmt : node->statements) {
        if (stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name == "main") has_main = true;
            emit_fn(fn);
        }
    }
    // If there's no main function but there are top-level statements,
    // emit an implicit main wrapping them
    bool has_top_stmts = false;
    for (auto& stmt : node->statements) {
        if (stmt->type != NodeType::FN_STMT && stmt->type != NodeType::STRUCT_STMT) {
            has_top_stmts = true;
            break;
        }
    }
    if (!has_main && has_top_stmts) {
        m_current_fn_ret_type = "i32";
        m_body += "define i32 @main() {\n";
        // Process top-level statements inline (they are not inside a function body)
        for (auto& stmt : node->statements) {
            if (!stmt || stmt->type == NodeType::FN_STMT || stmt->type == NodeType::STRUCT_STMT) continue;
            if (stmt->type == NodeType::PRINT_STMT) {
                auto* p = static_cast<PrintStmt*>(stmt.get());
                for (size_t i = 0; i < p->expressions.size(); ++i) {
                    if (i > 0) {
                        std::string sp = emit_string_literal(" ");
                        m_body += "    call i32 (ptr, ...) @printf(ptr " + sp + ")\n";
                    }
                    emit_print_expr(p->expressions[i].get());
                }
                m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_nl() + ")\n";
            } else if (stmt->type == NodeType::LET_STMT) {
                emit_let(static_cast<LetStmt*>(stmt.get()));
            } else if (stmt->type == NodeType::ASSIGN_STMT) {
                emit_assign(static_cast<AssignStmt*>(stmt.get()));
            } else if (stmt->type == NodeType::EXPR_STMT) {
                auto* es = static_cast<ExprStmt*>(stmt.get());
                emit_expr(es->expression.get());
            } else if (stmt->type == NodeType::IF_STMT) {
                emit_if(static_cast<IfStmt*>(stmt.get()));
            } else if (stmt->type == NodeType::WHILE_STMT) {
                emit_while(static_cast<WhileStmt*>(stmt.get()));
            } else if (stmt->type == NodeType::RETURN_STMT) {
                auto* r = static_cast<ReturnStmt*>(stmt.get());
                if (r->expression) {
                    std::string val = emit_expr_double(r->expression.get());
                    std::string val_i32 = emit_fptosi(val);
                    m_body += "    ret i32 " + val_i32 + "\n";
                } else {
                    m_body += "    ret i32 0\n";
                }
            }
        }
        m_body += "    ret i32 0\n";
        m_body += "}\n\n";
    }
}

std::string IrCodegen::get_fmt_g() {
    if (!m_fmt_g_emitted) {
        m_globals += "@.fmt_g = constant [3 x i8] c\"%g\\00\"\n";
        m_fmt_g_emitted = true;
    }
    return "@.fmt_g";
}

std::string IrCodegen::get_fmt_s() {
    if (!m_fmt_s_emitted) {
        m_globals += "@.fmt_s = constant [3 x i8] c\"%s\\00\"\n";
        m_fmt_s_emitted = true;
    }
    return "@.fmt_s";
}

std::string IrCodegen::get_fmt_nl() {
    if (!m_fmt_nl_emitted) {
        m_globals += "@.fmt_nl = constant [2 x i8] c\"\\0a\\00\"\n";
        m_fmt_nl_emitted = true;
    }
    return "@.fmt_nl";
}

std::string IrCodegen::get_ir_type(const std::string& spy_type) {
    if (spy_type == "i32") return "i32";
    if (spy_type == "i64") return "i64";
    if (spy_type == "f64" || spy_type == "double") return "double";
    if (spy_type == "bool") return "i1";
    if (spy_type == "char" || spy_type == "string" || spy_type == "*char") return "ptr";
    return "double";
}

std::string IrCodegen::alloc_local(const std::string& name, const std::string& ir_type) {
    std::string ir_name = name + ".a" + std::to_string(m_local_counter++);
    m_body += "    %" + ir_name + " = alloca " + ir_type + "\n";
    return "%" + ir_name;
}

std::string IrCodegen::emit_string_literal(const std::string& value) {
    std::string name = ".str" + std::to_string(m_string_counter++);
    std::string escaped;
    for (char c : value) {
        if (c == '\n') escaped += "\\0a";
        else if (c == '\r') escaped += "\\0d";
        else if (c == '\t') escaped += "\\09";
        else if (c == '"') escaped += "\\22";
        else if (c == '\\') escaped += "\\5c";
        else if (c >= 32 && c < 127) escaped += c;
        else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%02x", (unsigned char)c);
            escaped += buf;
        }
    }
    std::string len_str = std::to_string(value.size() + 1);
    m_globals += "@" + name + " = constant [" + len_str + " x i8] c\"" + escaped + "\\00\"\n";
    return "@" + name;
}

// Load a variable's pointer value from its alloca
std::string IrCodegen::load_var_ptr(const std::string& var_name) {
    std::string ptr = "%" + var_name;
    auto vit = m_vars.find(var_name);
    if (vit != m_vars.end()) ptr = vit->second;
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = load ptr, ptr " + ptr + "\n";
    return tmp;
}

// Store a value to a variable's alloca
void IrCodegen::store_to_var(const std::string& var_name, const std::string& val, const std::string& ir_type) {
    std::string ptr = "%" + var_name;
    auto vit = m_vars.find(var_name);
    if (vit != m_vars.end()) ptr = vit->second;
    m_body += "    store " + ir_type + " " + val + ", ptr " + ptr + "\n";
}

// Emit getelementptr i8, ptr, i32
std::string IrCodegen::emit_gep_i8(const std::string& ptr, const std::string& offset) {
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = getelementptr i8, ptr " + ptr + ", i32 " + offset + "\n";
    return tmp;
}

// Emit getelementptr i8, ptr, i32 constant
std::string IrCodegen::emit_gep_i8_int(const std::string& ptr, int offset) {
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = getelementptr i8, ptr " + ptr + ", i32 " + std::to_string(offset) + "\n";
    return tmp;
}

// Load double from pointer
std::string IrCodegen::emit_load_double(const std::string& ptr) {
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = load double, ptr " + ptr + "\n";
    return tmp;
}

// Load ptr from pointer
std::string IrCodegen::emit_load_ptr(const std::string& ptr) {
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = load ptr, ptr " + ptr + "\n";
    return tmp;
}

// fptosi double to i32
std::string IrCodegen::emit_fptosi(const std::string& val) {
    std::string tmp = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + tmp + " = fptosi double " + val + " to i32\n";
    return tmp;
}

// Check if an expression has string type (returns the IR type string if so, empty otherwise)
std::string IrCodegen::is_string_type(const std::string& ir_type) {
    if (ir_type == "ptr" || ir_type == "char*" || ir_type == "*char") return "ptr";
    return "";
}

// Determine if an expr node is string-typed, return ir type or empty
std::string IrCodegen::is_string_expr(ASTNode* expr) {
    if (!expr) return "";
    if (expr->type == NodeType::STRING_EXPR) return "ptr";
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        if (m_string_vars.count(id->name)) return "ptr";
        return "";
    }
    if (expr->type == NodeType::CALL_EXPR) {
        auto* ce = static_cast<CallExpr*>(expr);
        if (is_string_returning_builtin(ce->callee)) return "ptr";
        return "";
    }
    if (expr->type == NodeType::MEMBER_EXPR) {
        // Check field type
        auto* me = static_cast<MemberExpr*>(expr);
        std::string struct_type = get_struct_type_from_expr(me->object.get());
        if (!struct_type.empty()) {
            auto fit = m_struct_fields.find(struct_type);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        if (ft == "*char" || ft == "ptr" || ft == "string") return "ptr";
                        return "";
                    }
                }
            }
        }
        return "";
    }
    if (expr->type == NodeType::INDEX_EXPR) {
        // If indexing a char* variable, it returns a single char (numeric), not a string
        auto* ix = static_cast<IndexExpr*>(expr);
        if (ix->object->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(ix->object.get());
            if (m_string_vars.count(id->name)) return ""; // char indexing gives a number
        }
        return "";
    }
    if (expr->type == NodeType::BINOP_EXPR) {
        auto* be = static_cast<BinOpExpr*>(expr);
        if (be->op == "+") {
            // A + expression is string if either operand is string
            std::string l = is_string_expr(be->left.get());
            std::string r = is_string_expr(be->right.get());
            if (!l.empty() || !r.empty()) return "ptr";
        }
        return "";
    }
    return "";
}

// Get the struct type name from an expression
std::string IrCodegen::get_struct_type_from_expr(ASTNode* expr) {
    if (!expr) return "";
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        auto it = m_struct_ptr_vars.find(id->name);
        if (it != m_struct_ptr_vars.end()) return it->second;
        // Check if an ident is a struct by seeing if its name matches a struct... 
        // That's not how it works - we need tracking.
        return "";
    }
    if (expr->type == NodeType::INDEX_EXPR) {
        // For array[i], the struct type is from the array base
        auto* ix = static_cast<IndexExpr*>(expr);
        return get_struct_type_from_expr(ix->object.get());
    }
    if (expr->type == NodeType::MEMBER_EXPR) {
        // For a.b, if b is a struct type? Unlikely in self_lexer
        return "";
    }
    return "";
}

// Get the pointer value from an expression (for struct/array base)
// Handles: IDENT_EXPR (load ptr var), INDEX_EXPR (GEP for struct arrays)
std::string IrCodegen::get_ptr_expr(ASTNode* expr) {
    if (!expr) return "null";
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        return load_var_ptr(id->name);
    }
    if (expr->type == NodeType::INDEX_EXPR) {
        auto* ix = static_cast<IndexExpr*>(expr);
        // Get base pointer
        std::string base = get_ptr_expr(ix->object.get());
        // Get index as i32
        std::string idx;
        if (ix->index->type == NodeType::INT_EXPR) {
            auto* n = static_cast<IntExpr*>(ix->index.get());
            idx = std::to_string((int)n->value);
        } else {
            std::string idx_d = emit_expr_double(ix->index.get());
            idx = emit_fptosi(idx_d);
        }
        // Get struct type and element size
        std::string st = get_struct_type_from_expr(ix->object.get());
        int elem_size = 8;
        if (!st.empty()) {
            auto sit = m_struct_sizes.find(st);
            if (sit != m_struct_sizes.end()) elem_size = sit->second;
        }
        // Compute byte offset: idx * elem_size
        if (elem_size == 1) {
            return emit_gep_i8(base, idx);
        }
        std::string off_tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + off_tmp + " = mul i32 " + idx + ", " + std::to_string(elem_size) + "\n";
        return emit_gep_i8(base, off_tmp);
    }
    // Fallback: treat as expression that returns a ptr
    return emit_expr(expr);
}

std::string IrCodegen::emit_expr_double(ASTNode* expr) {
    if (!expr) return "0.0";
    if (expr->type == NodeType::INT_EXPR) {
        auto* n = static_cast<IntExpr*>(expr);
        std::string tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp + " = sitofp i32 " + std::to_string(n->value) + " to double\n";
        return tmp;
    }
    if (expr->type == NodeType::FLOAT_EXPR) {
        auto* n = static_cast<FloatExpr*>(expr);
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", n->value);
        return buf;
    }
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        std::string ptr = "%" + id->name;
        auto vit = m_vars.find(id->name);
        if (vit != m_vars.end()) ptr = vit->second;
        if (m_string_vars.count(id->name) || m_struct_ptr_vars.count(id->name)) {
            return "0.0"; // ptr var used as double
        }
        std::string tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp + " = load double, ptr " + ptr + "\n";
        return tmp;
    }
    if (expr->type == NodeType::INDEX_EXPR) {
        // source[i] - load byte, convert to double
        auto* ix = static_cast<IndexExpr*>(expr);
        // Get array base pointer from object
        std::string base;
        if (ix->object->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(ix->object.get());
            base = load_var_ptr(id->name);
        } else {
            base = get_ptr_expr(ix->object.get());
        }

        std::string idx;
        if (ix->index->type == NodeType::INT_EXPR) {
            auto* n = static_cast<IntExpr*>(ix->index.get());
            idx = std::to_string((int)n->value);
        } else {
            std::string idx_d = emit_expr_double(ix->index.get());
            idx = emit_fptosi(idx_d);
        }

        // GEP: source + idx (element size 1 for byte access)
        std::string elem_ptr = emit_gep_i8(base, idx);
        std::string byte_tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + byte_tmp + " = load i8, ptr " + elem_ptr + "\n";
        std::string result = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + result + " = uitofp i8 " + byte_tmp + " to double\n";
        return result;
    }
    if (expr->type == NodeType::MEMBER_EXPR) {
        // Read struct field value as double
        auto* me = static_cast<MemberExpr*>(expr);
        std::string struct_ptr = get_ptr_expr(me->object.get());
        std::string st = get_struct_type_from_expr(me->object.get());
        if (!st.empty()) {
            auto fit = m_struct_fields.find(st);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        std::string fptr = emit_gep_i8_int(struct_ptr, foff);
                        // If field is *char/ptr, return as double? That doesn't make sense for numeric ops.
                        // For line/column fields which are i32, we load double.
                        if (ft == "*char" || ft == "ptr" || ft == "string") {
                            return "0.0"; // string field used as double - shouldn't happen
                        }
                        return emit_load_double(fptr);
                    }
                }
            }
        }
        return "0.0";
    }
    // For BINOP_EXPR and UNARY_EXPR, delegate to emit_expr which returns a double
    return emit_expr(expr);
}

std::string IrCodegen::emit_expr(ASTNode* expr) {
    if (!expr) return "0.0";
    if (expr->type == NodeType::STRING_EXPR) {
        auto* s = static_cast<StringExpr*>(expr);
        return emit_string_literal(s->value);
    }
    if (expr->type == NodeType::INT_EXPR) {
        auto* n = static_cast<IntExpr*>(expr);
        return std::to_string(n->value);
    }
    if (expr->type == NodeType::FLOAT_EXPR) {
        auto* n = static_cast<FloatExpr*>(expr);
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", n->value);
        return buf;
    }
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        std::string ptr = "%" + id->name;
        auto vit = m_vars.find(id->name);
        if (vit != m_vars.end()) ptr = vit->second;
        if (m_string_vars.count(id->name) || m_struct_ptr_vars.count(id->name)) {
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = load ptr, ptr " + ptr + "\n";
            return tmp;
        }
        std::string tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp + " = load double, ptr " + ptr + "\n";
        return tmp;
    }
    if (expr->type == NodeType::CALL_EXPR) {
        auto* e = static_cast<CallExpr*>(expr);

        // Built-in: len(string)
        if (e->callee == "len" && e->args.size() == 1) {
            std::string arg_ptr;
            auto* arg = e->args[0].get();
            if (arg->type == NodeType::STRING_EXPR) {
                arg_ptr = "ptr " + emit_string_literal(static_cast<StringExpr*>(arg)->value);
            } else if (arg->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(arg);
                std::string p = load_var_ptr(id->name);
                arg_ptr = "ptr " + p;
            } else if (arg->type == NodeType::MEMBER_EXPR) {
                std::string p = emit_expr(arg);
                arg_ptr = "ptr " + p;
            } else {
                arg_ptr = "ptr " + emit_expr_double(arg);
            }
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call double @spy_strlen(" + arg_ptr + ")\n";
            return tmp;
        }

        // Built-in: str(number) -> string
        if (e->callee == "str" && e->args.size() == 1) {
            std::string val = "double " + emit_expr_double(e->args[0].get());
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call ptr @spy_str_from_double(" + val + ")\n";
            return tmp;
        }

        // Built-in: substr(string, start, len)
        if (e->callee == "substr" && e->args.size() == 3) {
            std::string s_arg;
            auto* st = e->args[0].get();
            if (st->type == NodeType::STRING_EXPR) {
                s_arg = "ptr " + emit_string_literal(static_cast<StringExpr*>(st)->value);
            } else if (st->type == NodeType::IDENT_EXPR) {
                std::string p = load_var_ptr(static_cast<IdentExpr*>(st)->name);
                s_arg = "ptr " + p;
            } else if (st->type == NodeType::MEMBER_EXPR) {
                std::string p = emit_expr(st);
                s_arg = "ptr " + p;
            } else {
                s_arg = "ptr " + emit_expr_double(st);
            }
            std::string start_val = emit_expr_double(e->args[1].get());
            std::string len_val = emit_expr_double(e->args[2].get());
            std::string start_tmp = emit_fptosi(start_val);
            std::string len_tmp = emit_fptosi(len_val);
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call ptr @spy_substr(" + s_arg + ", i32 " + start_tmp + ", i32 " + len_tmp + ")\n";
            return tmp;
        }

        // Built-in: chr(int) -> string
        if (e->callee == "chr" && e->args.size() == 1) {
            std::string val = emit_expr_double(e->args[0].get());
            std::string tmp_i = emit_fptosi(val);
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call ptr @spy_chr(i32 " + tmp_i + ")\n";
            return tmp;
        }

        // Built-in: alloc[Type](count) -> ptr
        if (e->callee == "alloc" && e->args.size() == 2) {
            int elem_size = 8;
            if (e->args[1]->type == NodeType::TYPE_EXPR) {
                auto* te = static_cast<TypeExpr*>(e->args[1].get());
                auto sit = m_struct_sizes.find(te->name);
                if (sit != m_struct_sizes.end()) elem_size = sit->second;
            }
            std::string count = emit_expr_double(e->args[0].get());
            std::string cnt_int = emit_fptosi(count);
            std::string size_int = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + size_int + " = mul i32 " + cnt_int + ", " + std::to_string(elem_size) + "\n";
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call ptr @spy_alloc(i32 " + size_int + ")\n";
            return tmp;
        }

        // Built-in: read_file(path) -> string
        if (e->callee == "read_file" && e->args.size() == 1) {
            auto* arg = e->args[0].get();
            std::string arg_ptr;
            if (arg->type == NodeType::STRING_EXPR) {
                arg_ptr = emit_string_literal(static_cast<StringExpr*>(arg)->value);
            } else {
                arg_ptr = emit_expr(arg);
            }
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = call ptr @spy_read_file(ptr " + arg_ptr + ")\n";
            return tmp;
        }

        // Regular function call
        std::string args;
        for (size_t i = 0; i < e->args.size(); ++i) {
            if (i > 0) args += ", ";
            auto* arg = e->args[i].get();
            // Check if this param is expected to be a string
            auto sp_it = m_fn_string_params.find(e->callee);
            bool is_str_param = (sp_it != m_fn_string_params.end() && sp_it->second.count((int)i));
            if (is_str_param) {
                std::string arg_val = emit_expr(arg);
                args += "ptr " + arg_val;
            } else {
                std::string arg_val = "double " + emit_expr_double(arg);
                args += arg_val;
            }
        }
        std::string ret_type = "double";
        auto rrit = m_fn_return_types.find(e->callee);
        if (rrit != m_fn_return_types.end()) ret_type = rrit->second;

        std::string tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp + " = call " + ret_type + " @" + e->callee + "(" + args + ")\n";
        return tmp;
    }

    if (expr->type == NodeType::INDEX_EXPR) {
        // source[i] - load byte, convert to double
        auto* ix = static_cast<IndexExpr*>(expr);
        std::string base;
        if (ix->object->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(ix->object.get());
            base = load_var_ptr(id->name);
        } else {
            base = get_ptr_expr(ix->object.get());
        }

        std::string idx;
        if (ix->index->type == NodeType::INT_EXPR) {
            auto* n = static_cast<IntExpr*>(ix->index.get());
            idx = std::to_string((int)n->value);
        } else {
            std::string idx_d = emit_expr_double(ix->index.get());
            idx = emit_fptosi(idx_d);
        }

        std::string elem_ptr = emit_gep_i8(base, idx);
        std::string byte_tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + byte_tmp + " = load i8, ptr " + elem_ptr + "\n";
        std::string result = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + result + " = uitofp i8 " + byte_tmp + " to double\n";
        return result;
    }

    if (expr->type == NodeType::MEMBER_EXPR) {
        // Read struct field
        auto* me = static_cast<MemberExpr*>(expr);
        std::string struct_ptr = get_ptr_expr(me->object.get());
        std::string st = get_struct_type_from_expr(me->object.get());
        if (!st.empty()) {
            auto fit = m_struct_fields.find(st);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        std::string fptr = emit_gep_i8_int(struct_ptr, foff);
                        if (!ft.empty() && ft[0] == '*') {
                            return emit_load_ptr(fptr);
                        } else {
                            return emit_load_double(fptr);
                        }
                    }
                }
            }
        }
        // Fallback: if we don't have struct info, try loading as ptr then as double
        std::string tmp_ptr = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp_ptr + " = load ptr, ptr " + struct_ptr + "\n";
        return tmp_ptr;
    }

    if (expr->type == NodeType::BINOP_EXPR) {
        auto* e = static_cast<BinOpExpr*>(expr);

        // String concatenation with +
        if (e->op == "+") {
            std::string left_is_str = is_string_expr(e->left.get());
            std::string right_is_str = is_string_expr(e->right.get());
            if (!left_is_str.empty() || !right_is_str.empty()) {
                // String concatenation
                std::string left_val, right_val;
                if (!left_is_str.empty()) {
                    left_val = emit_expr(e->left.get());
                } else {
                    // Number to string conversion then concat
                    std::string num = emit_expr_double(e->left.get());
                    std::string tmp = "%t" + std::to_string(m_temp_counter++);
                    m_body += "    " + tmp + " = call ptr @spy_str_from_double(double " + num + ")\n";
                    left_val = tmp;
                }
                if (!right_is_str.empty()) {
                    right_val = emit_expr(e->right.get());
                } else {
                    std::string num = emit_expr_double(e->right.get());
                    std::string tmp = "%t" + std::to_string(m_temp_counter++);
                    m_body += "    " + tmp + " = call ptr @spy_str_from_double(double " + num + ")\n";
                    right_val = tmp;
                }
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = call ptr @spy_strcat(ptr " + left_val + ", ptr " + right_val + ")\n";
                return tmp;
            }
        }

        if (e->op == "+" || e->op == "-" || e->op == "*" || e->op == "/" || e->op == "%" ||
            e->op == "==" || e->op == "!=" || e->op == "<" || e->op == ">" ||
            e->op == "<=" || e->op == ">=") {

            std::string left = emit_expr_double(e->left.get());
            std::string right = emit_expr_double(e->right.get());

            if (e->op == "+") {
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = fadd double " + left + ", " + right + "\n";
                return tmp;
            }
            if (e->op == "-") {
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = fsub double " + left + ", " + right + "\n";
                return tmp;
            }
            if (e->op == "*") {
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = fmul double " + left + ", " + right + "\n";
                return tmp;
            }
            if (e->op == "/") {
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = fdiv double " + left + ", " + right + "\n";
                return tmp;
            }
            if (e->op == "%") {
                std::string tmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp + " = call double @fmod(double " + left + ", double " + right + ")\n";
                return tmp;
            }
            // String comparison with strcmp for == and !=
            if ((e->op == "==" || e->op == "!=") &&
                (is_string_expr_check(e->left.get()) || is_string_expr_check(e->right.get()))) {
                std::string l_str = emit_expr(e->left.get());
                std::string r_str = emit_expr(e->right.get());
                std::string cmp = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + cmp + " = call i32 @strcmp(ptr " + l_str + ", ptr " + r_str + ")\n";
                std::string zero = "%t" + std::to_string(m_temp_counter++);
                if (e->op == "==") {
                    m_body += "    " + zero + " = icmp eq i32 " + cmp + ", 0\n";
                } else {
                    m_body += "    " + zero + " = icmp ne i32 " + cmp + ", 0\n";
                }
                std::string ext = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + ext + " = uitofp i1 " + zero + " to double\n";
                return ext;
            }

            // Comparisons: result is 1.0 or 0.0 as double
            std::string cond;
            if (e->op == "==") cond = "oeq";
            else if (e->op == "!=") cond = "one";
            else if (e->op == "<") cond = "olt";
            else if (e->op == ">") cond = "ogt";
            else if (e->op == "<=") cond = "ole";
            else if (e->op == ">=") cond = "oge";

            std::string cmp = "%t" + std::to_string(m_temp_counter++);
            std::string ext = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + cmp + " = fcmp " + cond + " double " + left + ", " + right + "\n";
            m_body += "    " + ext + " = uitofp i1 " + cmp + " to double\n";
            return ext;
        }

        if (e->op == "and" || e->op == "or") {
            std::string left = emit_expr_double(e->left.get());
            std::string right = emit_expr_double(e->right.get());
            std::string lc = "%t" + std::to_string(m_temp_counter++);
            std::string rc = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + lc + " = fcmp one double " + left + ", 0.0\n";
            m_body += "    " + rc + " = fcmp one double " + right + ", 0.0\n";
            std::string combined = "%t" + std::to_string(m_temp_counter++);
            if (e->op == "and") {
                m_body += "    " + combined + " = and i1 " + lc + ", " + rc + "\n";
            } else {
                m_body += "    " + combined + " = or i1 " + lc + ", " + rc + "\n";
            }
            std::string ext = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + ext + " = uitofp i1 " + combined + " to double\n";
            return ext;
        }

        return "0.0";
    }
    if (expr->type == NodeType::UNARY_EXPR) {
        auto* u = static_cast<UnaryExpr*>(expr);
        if (u->op == "-") {
            std::string operand = emit_expr(u->operand.get());
            if (u->operand->type == NodeType::INT_EXPR) {
                std::string tmp1 = "%t" + std::to_string(m_temp_counter++);
                std::string tmp2 = "%t" + std::to_string(m_temp_counter++);
                m_body += "    " + tmp1 + " = sitofp i32 " + operand + " to double\n";
                m_body += "    " + tmp2 + " = fsub double -0.0, " + tmp1 + "\n";
                return tmp2;
            }
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = fsub double -0.0, " + operand + "\n";
            return tmp;
        }
        return "0.0";
    }
    return "0.0";
}

void IrCodegen::emit_let(LetStmt* node) {
    std::string ir_type;
    bool is_string = false;
    bool is_struct_ptr = false;
    std::string struct_type_name;

    // Determine type from initializer or type annotation
    if (node->initializer) {
        if (node->initializer->type == NodeType::STRING_EXPR) {
            is_string = true;
            ir_type = "ptr";
        } else if (node->initializer->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(node->initializer.get());
            if (is_string_returning_builtin(call->callee)) {
                is_string = true;
                ir_type = "ptr";
            } else if (m_fn_return_types.count(call->callee) && m_fn_return_types[call->callee] == "ptr") {
                is_string = true;
                ir_type = "ptr";
            } else if (call->callee == "alloc" && call->args.size() == 2) {
                // alloc[Type](count) -> ptr
                ir_type = "ptr";
                is_struct_ptr = true;
                if (call->args[1]->type == NodeType::TYPE_EXPR) {
                    auto* te = static_cast<TypeExpr*>(call->args[1].get());
                    struct_type_name = te->name;
                }
            } else {
                ir_type = "double";
            }
        } else {
            ir_type = "double";
        }
    } else if (!node->type_annotation) {
        ir_type = "double";
    } else {
        // Check for pointer type annotation
        if (node->type_annotation->type == NodeType::PTR_TYPE_EXPR) {
            ir_type = "ptr";
            auto* pt = static_cast<PtrTypeExpr*>(node->type_annotation.get());
            std::string base_name = type_node_name(pt->base_type.get());
            if (base_name == "char" || base_name == "*char" || base_name == "ptr") {
                is_string = true;
            } else {
                is_struct_ptr = true;
                struct_type_name = base_name;
            }
        } else if (node->type_annotation->type == NodeType::TYPE_EXPR) {
            ir_type = get_ir_type(static_cast<TypeExpr*>(node->type_annotation.get())->name);
        } else {
            ir_type = "ptr";
        }
    }

    if (is_string) {
        m_string_vars.insert(node->name);
    }
    if (is_struct_ptr && !struct_type_name.empty()) {
        m_struct_ptr_vars[node->name] = struct_type_name;
    }
    if (ir_type == "ptr") {
        m_ptr_vars[node->name] = ir_type;
    }

    std::string ptr = alloc_local(node->name, ir_type);
    m_vars[node->name] = ptr;

    if (node->initializer) {
        if (ir_type == "double") {
            std::string val = emit_expr_double(node->initializer.get());
            m_body += "    store double " + val + ", ptr " + ptr + "\n";
        } else {
            std::string val = emit_expr(node->initializer.get());
            m_body += "    store ptr " + val + ", ptr " + ptr + "\n";
        }
    }
}

void IrCodegen::emit_assign(AssignStmt* node) {
    ASTNode* target = node->target.get();

    // Simple identifier assignment
    if (target->type == NodeType::IDENT_EXPR) {
        std::string target_name = static_cast<IdentExpr*>(target)->name;
        std::string ptr = "%" + target_name;
        auto vit = m_vars.find(target_name);
        if (vit != m_vars.end()) ptr = vit->second;

        if (m_string_vars.count(target_name)) {
            std::string val = emit_expr(node->value.get());
            m_body += "    store ptr " + val + ", ptr " + ptr + "\n";
        } else {
            std::string val = emit_expr_double(node->value.get());
            m_body += "    store double " + val + ", ptr " + ptr + "\n";
        }
        return;
    }

    // tokens[ti].kind = val  -> MEMBER_EXPR(INDEX_EXPR, member)
    if (target->type == NodeType::MEMBER_EXPR) {
        auto* me = static_cast<MemberExpr*>(target);
        std::string struct_ptr = get_ptr_expr(me->object.get());
        std::string st = get_struct_type_from_expr(me->object.get());
        if (!st.empty()) {
            auto fit = m_struct_fields.find(st);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        std::string fptr = emit_gep_i8_int(struct_ptr, foff);
                        if (!ft.empty() && ft[0] == '*') {
                            std::string val = emit_expr(node->value.get());
                            m_body += "    store ptr " + val + ", ptr " + fptr + "\n";
                        } else {
                            std::string val = emit_expr_double(node->value.get());
                            m_body += "    store double " + val + ", ptr " + fptr + "\n";
                        }
                        return;
                    }
                }
            }
        }
        // Fallback: store double at struct pointer offset 0
        std::string val = emit_expr_double(node->value.get());
        m_body += "    store double " + val + ", ptr " + struct_ptr + "\n";
        return;
    }

    // source[i] = val
    if (target->type == NodeType::INDEX_EXPR) {
        auto* ix = static_cast<IndexExpr*>(target);
        std::string base;
        if (ix->object->type == NodeType::IDENT_EXPR) {
            base = load_var_ptr(static_cast<IdentExpr*>(ix->object.get())->name);
        } else {
            base = get_ptr_expr(ix->object.get());
        }

        std::string idx;
        if (ix->index->type == NodeType::INT_EXPR) {
            auto* n = static_cast<IntExpr*>(ix->index.get());
            idx = std::to_string((int)n->value);
        } else {
            std::string idx_d = emit_expr_double(ix->index.get());
            idx = emit_fptosi(idx_d);
        }

        std::string elem_ptr = emit_gep_i8(base, idx);
        // Convert double value to i8 and store
        std::string val = emit_expr_double(node->value.get());
        std::string val_i32 = emit_fptosi(val);
        std::string val_i8 = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + val_i8 + " = trunc i32 " + val_i32 + " to i8\n";
        m_body += "    store i8 " + val_i8 + ", ptr " + elem_ptr + "\n";
        return;
    }
}

void IrCodegen::emit_if(IfStmt* node) {
    std::string cond_val = emit_expr_double(node->condition.get());
    std::string cond_bool = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + cond_bool + " = fcmp one double " + cond_val + ", 0.0\n";

    std::string then_label = "L" + std::to_string(m_label_counter++);
    std::string else_label = "L" + std::to_string(m_label_counter++);
    std::string merge_label = "L" + std::to_string(m_label_counter++);

    m_body += "    br i1 " + cond_bool + ", label %" + then_label + ", label %" + else_label + "\n";

    // Then block
    m_body += "\n" + then_label + ":\n";
    emit_block(node->then_body);
    m_body += "    br label %" + merge_label + "\n";

    // Else block
    m_body += "\n" + else_label + ":\n";
    if (node->has_else) {
        if (node->else_body.size() == 1 && node->else_body[0]->type == NodeType::IF_STMT) {
            emit_if(static_cast<IfStmt*>(node->else_body[0].get()));
        } else {
            emit_block(node->else_body);
        }
    }
    m_body += "    br label %" + merge_label + "\n";

    // Merge block
    m_body += "\n" + merge_label + ":\n";
}

void IrCodegen::emit_while(WhileStmt* node) {
    std::string cond_label = "L" + std::to_string(m_label_counter++);
    std::string body_label = "L" + std::to_string(m_label_counter++);
    std::string end_label = "L" + std::to_string(m_label_counter++);

    m_body += "    br label %" + cond_label + "\n";

    // Condition block
    m_body += "\n" + cond_label + ":\n";
    std::string cond_val = emit_expr_double(node->condition.get());
    std::string cond_bool = "%t" + std::to_string(m_temp_counter++);
    m_body += "    " + cond_bool + " = fcmp one double " + cond_val + ", 0.0\n";
    m_body += "    br i1 " + cond_bool + ", label %" + body_label + ", label %" + end_label + "\n";

    // Body block
    m_body += "\n" + body_label + ":\n";
    emit_block(node->body);
    m_body += "    br label %" + cond_label + "\n";

    // End block
    m_body += "\n" + end_label + ":\n";
}

void IrCodegen::emit_fn_decl(FnStmt* node) {
    std::string ret_type = "double";
    m_fn_ret_types[node->name] = ret_type;

    std::vector<std::string> param_types;
    m_ir += "declare " + ret_type + " @" + node->name + "(";
    for (size_t i = 0; i < node->params.size(); ++i) {
        if (i > 0) m_ir += ", ";
        param_types.push_back("double");
        m_ir += "double";
    }
    m_fn_param_types[node->name] = param_types;
    m_ir += ")\n";
}

void IrCodegen::emit_fn(FnStmt* node) {
    std::string ret_type = "double";
    auto rit = m_fn_return_types.find(node->name);
    if (rit != m_fn_return_types.end()) ret_type = rit->second;
    if (node->name == "main") ret_type = "i32";
    m_current_fn_ret_type = ret_type;

    // Build parameter types list - check typed_params for pointer types and inferred string params
    std::vector<std::string> param_ir_types;
    std::map<std::string, std::string> param_ptr_overrides;

    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string ir_t = "double";
        // Check typed_params annotation
        if (i < node->typed_params.size() && node->typed_params[i].type) {
            ASTNode* pt = node->typed_params[i].type.get();
            std::string tn = type_node_name(pt);
            std::string pt_ir = type_to_ir(pt);
            if (pt_ir == "ptr") {
                ir_t = "ptr";
                std::string pname = node->params[i];
                param_ptr_overrides[pname] = tn;
                if (tn == "*char" || tn == "ptr" || tn == "string" || tn == "char") {
                    m_string_vars.insert(pname);
                } else {
                    std::string base = tn;
                    if (base.size() > 1 && base[0] == '*') {
                        base = base.substr(1);
                    }
                    if (m_struct_sizes.count(base)) {
                        m_struct_ptr_vars[pname] = base;
                    }
                }
            }
        }
        // Check inferred string params from calls
        if (ir_t == "double") {
            auto sit = m_fn_string_params.find(node->name);
            if (sit != m_fn_string_params.end() && sit->second.count((int)i)) {
                ir_t = "ptr";
                m_string_vars.insert(node->params[i]);
            }
        }
        param_ir_types.push_back(ir_t);
    }

    // Emit function signature
    m_body += "define " + ret_type + " @" + node->name + "(";
    for (size_t i = 0; i < node->params.size(); ++i) {
        if (i > 0) m_body += ", ";
        std::string ir_t = (i < param_ir_types.size()) ? param_ir_types[i] : "double";
        m_body += ir_t + " %" + node->params[i];
    }
    m_body += ") {\n";

    // Allocate and store params
    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string pname = node->params[i];
        std::string ir_t = (i < param_ir_types.size()) ? param_ir_types[i] : "double";
        std::string ir_name = pname + ".a" + std::to_string(m_local_counter++);
        m_body += "    %" + ir_name + " = alloca " + ir_t + "\n";
        m_body += "    store " + ir_t + " %" + pname + ", ptr %" + ir_name + "\n";
        m_vars[pname] = "%" + ir_name;
    }

    emit_block(node->body);

    bool has_return = false;
    for (auto& stmt : node->body) {
        if (stmt && stmt->type == NodeType::RETURN_STMT) {
            has_return = true;
            break;
        }
    }
    if (!has_return) {
        if (node->name == "main") {
            m_body += "    ret i32 0\n";
        } else {
            m_body += "    ret double 0.0\n";
        }
    }
    m_body += "}\n\n";
}

void IrCodegen::emit_print_expr(ASTNode* expr) {
    if (expr->type == NodeType::STRING_EXPR) {
        std::string ptr = emit_expr(expr);
        m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_s() + ", ptr " + ptr + ")\n";
    } else if (expr->type == NodeType::INT_EXPR) {
        std::string raw = emit_expr(expr);
        std::string tmp = "%t" + std::to_string(m_temp_counter++);
        m_body += "    " + tmp + " = sitofp i32 " + raw + " to double\n";
        m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + tmp + ")\n";
    } else if (expr->type == NodeType::FLOAT_EXPR) {
        std::string val = emit_expr(expr);
        m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
    } else if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        std::string val = emit_expr(expr);
        if (m_string_vars.count(id->name)) {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_s() + ", ptr " + val + ")\n";
        } else if (m_struct_ptr_vars.count(id->name)) {
            std::string tmp = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp + " = ptrtoint ptr " + val + " to i64\n";
            std::string tmp2 = "%t" + std::to_string(m_temp_counter++);
            m_body += "    " + tmp2 + " = uitofp i64 " + tmp + " to double\n";
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + tmp2 + ")\n";
        } else {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
        }
    } else if (expr->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(expr);
        std::string val = emit_expr(expr);
        if (is_string_returning_builtin(call->callee)) {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_s() + ", ptr " + val + ")\n";
        } else {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
        }
    } else if (expr->type == NodeType::INDEX_EXPR) {
        // source[i] - prints the byte as number
        std::string val = emit_expr(expr);
        m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
    } else if (expr->type == NodeType::MEMBER_EXPR) {
        // Determine if field is string or numeric
        auto* me = static_cast<MemberExpr*>(expr);
        std::string st = get_struct_type_from_expr(me->object.get());
        bool is_str_field = false;
        if (!st.empty()) {
            auto fit = m_struct_fields.find(st);
            if (fit != m_struct_fields.end()) {
                for (auto& [fname, foff, ft] : fit->second) {
                    if (fname == me->member) {
                        if (ft == "*char" || ft == "ptr" || ft == "string") {
                            is_str_field = true;
                        }
                        break;
                    }
                }
            }
        }
        std::string val = emit_expr(expr);
        if (is_str_field) {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_s() + ", ptr " + val + ")\n";
        } else {
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
        }
    } else if (expr->type == NodeType::BINOP_EXPR) {
        // Check if this is a string concatenation expression
        auto* be = static_cast<BinOpExpr*>(expr);
        if (be->op == "+" && !is_string_expr(expr).empty()) {
            std::string val = emit_expr(expr);
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_s() + ", ptr " + val + ")\n";
        } else {
            std::string val = emit_expr(expr);
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
        }
    } else {
        std::string val = emit_expr(expr);
        m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_g() + ", double " + val + ")\n";
    }
}

void IrCodegen::emit_block(const std::vector<ASTPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        if (stmt->type == NodeType::PRINT_STMT) {
            auto* p = static_cast<PrintStmt*>(stmt.get());
            for (size_t i = 0; i < p->expressions.size(); ++i) {
                if (i > 0) {
                    std::string sp = emit_string_literal(" ");
                    m_body += "    call i32 (ptr, ...) @printf(ptr " + sp + ")\n";
                }
                emit_print_expr(p->expressions[i].get());
            }
            m_body += "    call i32 (ptr, ...) @printf(ptr " + get_fmt_nl() + ")\n";
        } else if (stmt->type == NodeType::LET_STMT) {
            emit_let(static_cast<LetStmt*>(stmt.get()));
        } else if (stmt->type == NodeType::ASSIGN_STMT) {
            emit_assign(static_cast<AssignStmt*>(stmt.get()));
        } else if (stmt->type == NodeType::IF_STMT) {
            emit_if(static_cast<IfStmt*>(stmt.get()));
        } else if (stmt->type == NodeType::WHILE_STMT) {
            emit_while(static_cast<WhileStmt*>(stmt.get()));
        } else if (stmt->type == NodeType::EXPR_STMT) {
            auto* es = static_cast<ExprStmt*>(stmt.get());
            emit_expr(es->expression.get());
        } else if (stmt->type == NodeType::RETURN_STMT) {
            auto* r = static_cast<ReturnStmt*>(stmt.get());
            if (r->expression) {
                if (m_current_fn_ret_type == "ptr") {
                    std::string val = emit_expr(r->expression.get());
                    m_body += "    ret ptr " + val + "\n";
                } else if (m_current_fn_ret_type == "i32") {
                    std::string val = emit_expr_double(r->expression.get());
                    std::string val_i32 = emit_fptosi(val);
                    m_body += "    ret i32 " + val_i32 + "\n";
                } else {
                    std::string val = emit_expr_double(r->expression.get());
                    m_body += "    ret " + m_current_fn_ret_type + " " + val + "\n";
                }
            } else {
                std::string zero_val = (m_current_fn_ret_type == "double") ? "0.0" : "0";
                m_body += "    ret " + m_current_fn_ret_type + " " + zero_val + "\n";
            }
        }
    }
}

} // namespace spy
