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

#include "spy/Codegen.h"
#include "spy/Lexer.h"
#include "spy/Parser.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>

namespace spy {

static std::string escape_string(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '\n') result += "\\n";
        else if (c == '\t') result += "\\t";
        else if (c == '\\') result += "\\\\";
        else if (c == '"') result += "\\\"";
        else result += c;
    }
    return result;
}

static std::string spy_type_to_c(const std::string& t);

static std::string format_for_type(const std::string& ctype) {
    if (ctype == "int" || ctype == "int32_t") return "%d";
    if (ctype == "int8_t") return "%hhd";
    if (ctype == "int16_t") return "%hd";
    if (ctype == "int64_t") return "%lld";
    if (ctype == "uint8_t") return "%hhu";
    if (ctype == "uint16_t") return "%hu";
    if (ctype == "uint32_t") return "%u";
    if (ctype == "uint64_t") return "%llu";
    if (ctype == "size_t") return "%zu";
    if (ctype == "float") return "%f";
    if (ctype == "char") return "%c";
    if (ctype == "char*" || ctype == "const char*") return "%s";
    return "%g";
}

void Codegen::collect_call_string_params(ASTNode* node) {
    if (!node) return;
    if (node->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node);
        auto it = m_all_fns.find(call->callee);
        if (it != m_all_fns.end()) {
            auto* fn = it->second;
            for (size_t i = 0; i < call->args.size() && i < fn->params.size(); ++i) {
                if (is_string_expr(call->args[i].get())) {
                    m_fn_string_params[call->callee].insert(fn->params[i]);
                }
            }
        }
    } else if (node->type == NodeType::METHOD_CALL_EXPR) {
        // For join().split() etc., check string args passed to user-defined functions
        // Resolve obj.method(args) → module or class methods
    } else if (node->type == NodeType::PROGRAM) {
        auto* p = static_cast<Program*>(node);
        for (auto& s : p->statements) collect_call_string_params(s.get());
    } else if (node->type == NodeType::EXPR_STMT) {
        collect_call_string_params(static_cast<ExprStmt*>(node)->expression.get());
    } else if (node->type == NodeType::ASSIGN_STMT) {
        auto* a = static_cast<AssignStmt*>(node);
        collect_call_string_params(a->target.get());
        collect_call_string_params(a->value.get());
    } else if (node->type == NodeType::LET_STMT) {
        auto* let = static_cast<LetStmt*>(node);
        collect_call_string_params(let->initializer.get());
        // Track string-returning initializers so subsequent calls can detect string vars
        if (let->initializer && is_string_expr(let->initializer.get())) {
            m_string_vars.insert(let->name);
        } else if (let->initializer && let->initializer->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(let->initializer.get());
            if (call->callee == "read_file" || call->callee == "input" || call->callee == "exec"
                || call->callee == "get_cwd" || call->callee == "chr" || call->callee == "substr"
                || call->callee == "str" || call->callee == "cg_emit_expr" || call->callee == "cg_type_to_c"
                || call->callee == "cg_find_ret_type" || call->callee == "cg_find_field_type"
                || call->callee == "cg_get_field_type") {
                m_string_vars.insert(let->name);
            }
        } else if (let->type_annotation) {
            std::string ctype = type_to_c(let->type_annotation.get());
            if (ctype == "const char*" || ctype == "char*" || ctype == "char *") {
                m_string_vars.insert(let->name);
            }
        }
    } else if (node->type == NodeType::IF_STMT) {
        auto* i = static_cast<IfStmt*>(node);
        collect_call_string_params(i->condition.get());
        for (auto& s : i->then_body) collect_call_string_params(s.get());
        for (auto& s : i->else_body) collect_call_string_params(s.get());
    } else if (node->type == NodeType::WHILE_STMT) {
        auto* w = static_cast<WhileStmt*>(node);
        collect_call_string_params(w->condition.get());
        for (auto& s : w->body) collect_call_string_params(s.get());
    } else if (node->type == NodeType::FOR_STMT) {
        auto* f = static_cast<ForStmt*>(node);
        collect_call_string_params(f->end.get());
        for (auto& s : f->body) collect_call_string_params(s.get());
    } else if (node->type == NodeType::RETURN_STMT) {
        collect_call_string_params(static_cast<ReturnStmt*>(node)->expression.get());
    } else if (node->type == NodeType::PRINT_STMT) {
        auto* p = static_cast<PrintStmt*>(node);
        for (auto& e : p->expressions) collect_call_string_params(e.get());
    } else if (node->type == NodeType::BINOP_EXPR) {
        auto* b = static_cast<BinOpExpr*>(node);
        collect_call_string_params(b->left.get());
        collect_call_string_params(b->right.get());
        // Track string concatenation results
        if (b->op == "+" && (is_string_expr(b->left.get()) || is_string_expr(b->right.get())
            || (b->left->type == NodeType::STRING_EXPR) || (b->right->type == NodeType::STRING_EXPR))) {
            // The result of this expression is a string, but we can't assign it to m_string_vars here
            // because we don't know the LET variable name. It will be detected when used as a call arg.
        }
    } else if (node->type == NodeType::TRY_STMT) {
        auto* t = static_cast<TryStmt*>(node);
        for (auto& s : t->body) collect_call_string_params(s.get());
        for (auto& h : t->handlers) collect_call_string_params(h.get());
    } else if (node->type == NodeType::EXCEPT_HANDLER) {
        auto* h = static_cast<ExceptHandler*>(node);
        for (auto& s : h->body) collect_call_string_params(s.get());
    } else if (node->type == NodeType::CLASS_STMT) {
        auto* c = static_cast<ClassStmt*>(node);
        std::string saved_class = m_current_class;
        m_current_class = c->name;
        for (auto& m : c->methods) collect_call_string_params(m.get());
        m_current_class = saved_class;
    } else if (node->type == NodeType::FN_STMT) {
        auto* f = static_cast<FnStmt*>(node);
        for (auto& s : f->body) collect_call_string_params(s.get());
    } else if (node->type == NodeType::SUPER_METHOD_CALL_EXPR) {
        auto* e = static_cast<SuperMethodCallExpr*>(node);
        auto pit = m_class_parent.find(m_current_class);
        if (pit != m_class_parent.end() && !pit->second.empty()) {
            std::string parent_name = pit->second.front();
            std::string method_key = parent_name + "." + e->method;
            auto mpit = m_class_method_params.find(parent_name);
            if (mpit != m_class_method_params.end()) {
                for (auto& [mn, mp] : mpit->second) {
                    if (mn == e->method) {
                        size_t arg_i = 0;
                        for (size_t i = 0; i < mp.size(); ++i) {
                            if (mp[i] == "self") continue;
                            if (arg_i < e->args.size() && is_string_expr(e->args[arg_i].get())) {
                                m_fn_string_params[method_key].insert(mp[i]);
                                m_class_string_params.insert(mp[i]);
                                auto& pfields = m_class_fields[parent_name];
                                for (auto& f : pfields) {
                                    if (f.first == mp[i] && f.second == "double") {
                                        f.second = "const char*";
                                    }
                                }
                            }
                            arg_i++;
                        }
                        break;
                    }
                }
            }
        }
    }
}

std::string Codegen::get_struct_field_type(ASTNode* obj, const std::string& member) {
    std::string struct_type;
    if (obj->type == NodeType::IDENT_EXPR) {
        struct_type = get_var_type(static_cast<IdentExpr*>(obj)->name);
    } else if (obj->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExpr*>(obj);
        if (idx->object->type == NodeType::IDENT_EXPR) {
            struct_type = get_var_type(static_cast<IdentExpr*>(idx->object.get())->name);
        } else if (idx->object->type == NodeType::MEMBER_EXPR) {
            auto* inner_me = static_cast<MemberExpr*>(idx->object.get());
            if (inner_me->object->type == NodeType::IDENT_EXPR) {
                std::string base_type = get_var_type(static_cast<IdentExpr*>(inner_me->object.get())->name);
                if (!base_type.empty() && base_type.back() == '*') {
                    std::string b = base_type;
                    if (b.back() == '*') b.pop_back();
                    while (!b.empty() && b.back() == ' ') b.pop_back();
                    size_t sp = b.rfind("struct ");
                    if (sp != std::string::npos) b = b.substr(sp + 7);
                    auto it = m_struct_defs.find(b);
                    if (it != m_struct_defs.end()) {
                        for (auto& [fn, ft] : it->second) {
                            if (fn == inner_me->member) {
                                struct_type = ft;
                                break;
                            }
                        }
                    }
                }
            }
        }
    } else if (obj->type == NodeType::MEMBER_EXPR) {
        auto* inner_me = static_cast<MemberExpr*>(obj);
        if (inner_me->object->type == NodeType::IDENT_EXPR) {
            struct_type = get_var_type(static_cast<IdentExpr*>(inner_me->object.get())->name);
        }
    }
    if (struct_type.empty()) return "";
    // Strip pointer suffix
    if (struct_type.back() == '*') struct_type.pop_back();
    // Strip trailing space after removing *
    while (!struct_type.empty() && struct_type.back() == ' ') struct_type.pop_back();
    // Strip "struct " prefix if present
    size_t spos = struct_type.rfind("struct ");
    if (spos != std::string::npos) struct_type = struct_type.substr(spos + 7);
    auto sit = m_struct_defs.find(struct_type);
    if (sit != m_struct_defs.end()) {
        for (auto& [fname, ftype] : sit->second) {
            if (fname == member) return ftype;
        }
    }
    return "";
}

void Codegen::add_search_path(const std::string& path) {
    m_search_paths.push_back(path);
}

std::string Codegen::find_module_file(const std::string& mod) const {
    // First try search paths
    for (auto& p : m_search_paths) {
        std::string full = p + "/" + mod + ".spy";
        std::ifstream test(full);
        if (test.is_open()) {
            test.close();
            return full;
        }
    }
    // Fallback: just "mod.spy" in CWD
    std::string fallback = mod + ".spy";
    std::ifstream test(fallback);
    if (test.is_open()) {
        test.close();
        return fallback;
    }
    return "";
}

void Codegen::collect_field_types(ASTNode* node) {
    if (!node) return;
    if (node->type == NodeType::PROGRAM) {
        auto* prog = static_cast<Program*>(node);
        for (auto& s : prog->statements) collect_field_types(s.get());
    } else if (node->type == NodeType::CLASS_STMT) {
        auto* cls = static_cast<ClassStmt*>(node);
        for (auto& m : cls->methods) collect_field_types(m.get());
    } else if (node->type == NodeType::FN_STMT) {
        auto* fn = static_cast<FnStmt*>(node);
        for (auto& s : fn->body) collect_field_types(s.get());
    } else if (node->type == NodeType::LET_STMT) {
        auto* let = static_cast<LetStmt*>(node);
        collect_field_types(let->initializer.get());
    } else if (node->type == NodeType::IF_STMT) {
        auto* ifn = static_cast<IfStmt*>(node);
        collect_field_types(ifn->condition.get());
        for (auto& s : ifn->then_body) collect_field_types(s.get());
        for (auto& s : ifn->else_body) collect_field_types(s.get());
    } else if (node->type == NodeType::WHILE_STMT) {
        auto* wh = static_cast<WhileStmt*>(node);
        collect_field_types(wh->condition.get());
        for (auto& s : wh->body) collect_field_types(s.get());
    } else if (node->type == NodeType::FOR_STMT) {
        auto* fr = static_cast<ForStmt*>(node);
        for (auto& s : fr->body) collect_field_types(s.get());
    } else if (node->type == NodeType::EXPR_STMT) {
        auto* es = static_cast<ExprStmt*>(node);
        collect_field_types(es->expression.get());
    } else if (node->type == NodeType::RETURN_STMT) {
        auto* ret = static_cast<ReturnStmt*>(node);
        collect_field_types(ret->expression.get());
    } else if (node->type == NodeType::MATCH_STMT) {
        auto* mt = static_cast<MatchStmt*>(node);
        collect_field_types(mt->value.get());
    } else if (node->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node);
        for (auto& a : call->args) collect_field_types(a.get());
    } else if (node->type == NodeType::BINOP_EXPR) {
        auto* bin = static_cast<BinOpExpr*>(node);
        collect_field_types(bin->left.get());
        collect_field_types(bin->right.get());
    } else if (node->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExpr*>(node);
        collect_field_types(idx->object.get());
        collect_field_types(idx->index.get());
    } else if (node->type == NodeType::PIPE_EXPR) {
        auto* pipe = static_cast<PipeExpr*>(node);
        collect_field_types(pipe->value.get());
        collect_field_types(pipe->call.get());
    } else if (node->type == NodeType::MEMBER_EXPR) {
        auto* mem = static_cast<MemberExpr*>(node);
        collect_field_types(mem->object.get());
    } else if (node->type == NodeType::METHOD_CALL_EXPR) {
        auto* mc = static_cast<MethodCallExpr*>(node);
        collect_field_types(mc->object.get());
        for (auto& a : mc->args) collect_field_types(a.get());
    } else if (node->type == NodeType::ASSIGN_STMT) {
        auto* as = static_cast<AssignStmt*>(node);
        collect_field_types(as->value.get());
    } else if (node->type == NodeType::ARRAY_EXPR) {
        auto* arr = static_cast<ArrayExpr*>(node);
        for (auto& e : arr->elements) collect_field_types(e.get());
    }
}

std::string Codegen::generate(ASTNode* program) {
    std::string out;

    auto* prog = static_cast<Program*>(program);

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            m_all_fns[fn->name] = fn;
            if (!fn->defaults.empty()) {
                m_fn_defaults[fn->name] = fn;
                for (auto& p : fn->params) {
                    if (fn->defaults.count(p) && fn->defaults.at(p)) {
                        if (fn->defaults.at(p)->type == NodeType::STRING_EXPR) {
                            m_fn_string_params[fn->name].insert(p);
                        }
                    }
                }
            }
        }
        if (stmt && stmt->type == NodeType::LET_STMT) {
            auto* let = static_cast<LetStmt*>(stmt.get());
            if (let->initializer && let->initializer->type == NodeType::LAMBDA_EXPR) {
                auto* e = static_cast<LambdaExpr*>(let->initializer.get());
                LambdaInfo li;
                li.name = let->name;
                li.expr = e;
                li.captures = find_captures(e, stmt.get());
                m_lambdas.push_back(li);
                m_lambda_counter++;
            } else {
                collect_lambdas(let->initializer.get());
            }
        }
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            auto* cls = static_cast<ClassStmt*>(stmt.get());
            for (auto& m : cls->methods) {
                auto* fn = static_cast<FnStmt*>(m.get());
                std::string key = cls->name + "." + fn->name;
                if (!fn->defaults.empty()) {
                    m_fn_defaults[key] = fn;
                    for (auto& p : fn->params) {
                        if (fn->defaults.count(p) && fn->defaults.at(p)) {
                            if (fn->defaults.at(p)->type == NodeType::STRING_EXPR) {
                                m_fn_string_params[key].insert(p);
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            for (auto& s : fn->body) {
                scan_lambdas(s.get());
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(stmt.get());
            for (size_t i = 0; i < call->args.size(); ++i) {
                if (is_string_expr(call->args[i].get())) {
                    if (m_fn_defaults.count(call->callee)) {
                        auto* fn = m_fn_defaults[call->callee];
                        if (i < fn->params.size()) {
                            m_fn_string_params[call->callee].insert(fn->params[i]);
                        }
                    }
                }
            }
        }
        if (stmt && stmt->type == NodeType::EXPR_STMT) {
            auto* es = static_cast<ExprStmt*>(stmt.get());
            if (es->expression->type == NodeType::CALL_EXPR) {
                auto* call = static_cast<CallExpr*>(es->expression.get());
                for (size_t i = 0; i < call->args.size(); ++i) {
                    if (is_string_expr(call->args[i].get())) {
                        if (m_fn_defaults.count(call->callee)) {
                            auto* fn = m_fn_defaults[call->callee];
                            if (i < fn->params.size()) {
                                m_fn_string_params[call->callee].insert(fn->params[i]);
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            auto* cls = static_cast<ClassStmt*>(stmt.get());
            if (!cls->parents.empty()) {
                m_class_parent[cls->name] = cls->parents;
            }
            std::set<std::string> method_names;
            for (auto& m : cls->methods) {
                auto* fn = static_cast<FnStmt*>(m.get());
                method_names.insert(fn->name);
            }
            m_class_methods[cls->name] = method_names;
            for (auto& m : cls->methods) {
                auto* fn = static_cast<FnStmt*>(m.get());
                m_class_method_params[cls->name].push_back(std::make_pair(fn->name, fn->params));
                if (fn->params.empty() || fn->params[0] != "self") {
                    m_class_static_methods[cls->name].insert(fn->name);
                    std::vector<std::pair<std::string, std::string>> param_types;
                    for (auto& p : fn->params) {
                        param_types.push_back({p, "double"});
                    }
                    m_class_static_param_types[cls->name + "." + fn->name] = param_types;
                }
            }
        }
    }

    collect_call_string_params(prog);

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            auto* cls = static_cast<ClassStmt*>(stmt.get());
            std::vector<std::pair<std::string, std::string>> fields;

            if (!cls->parents.empty()) {
                for (auto& pname : cls->parents) {
                    auto& parent_fields = m_class_fields[pname];
                    for (auto& pf : parent_fields) {
                        bool dup = false;
                        for (auto& f : fields) { if (f.first == pf.first) { dup = true; break; } }
                        if (!dup) fields.push_back(pf);
                    }
                }
            }

            for (auto& m : cls->methods) {
                auto* fn = static_cast<FnStmt*>(m.get());
                if (fn->name == "__init__") {
                    for (auto& p : fn->params) {
                        if (p == "self") continue;
                        bool already_inherited = false;
                        for (auto& f : fields) {
                            if (f.first == p) { already_inherited = true; break; }
                        }
                        if (!already_inherited) {
                            fields.push_back({p, "double"});
                        }
                    }
                    break;
                }
            }
            m_class_fields[cls->name] = fields;
        }
    }

    collect_field_types(program);

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            for (auto& s : fn->body) {
                if (s && s->type == NodeType::GLOBAL_STMT) {
                    m_global_vars.insert(static_cast<GlobalStmt*>(s.get())->name);
                }
            }
        }
    }

    for (auto& gname : m_global_vars) {
        out += "static double " + gname + " = 0;\n";
    }
    if (!m_global_vars.empty()) out += "\n";

    // Detect generator functions (contain yield)
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            for (auto& s : fn->body) {
                if (s && s->type == NodeType::YIELD_STMT) {
                    m_generator_fns.insert(fn->name);
                    break;
                }
            }
        }
    }

    if (!m_generator_fns.empty()) {
        out += "static double __spy_gen_buf[10000];\n";
        out += "static int __spy_gen_len = 0;\n";
        out += "\n";
    }

    for (auto& [cname, fields] : m_class_fields) {
        for (auto& [fname, ftype] : fields) {
            if (ftype == "double") {
                for (auto& stmt : prog->statements) {
                    if (stmt && stmt->type == NodeType::LET_STMT) {
                        auto* let = static_cast<LetStmt*>(stmt.get());
                        if (let->initializer->type == NodeType::CALL_EXPR) {
                            auto* call = static_cast<CallExpr*>(let->initializer.get());
                            if (call->callee == cname) {
                                auto it = std::find_if(fields.begin(), fields.end(),
                                    [&](auto& f){ return f.first == fname; });
                                if (it != fields.end() && it->second == "double") {
                                    size_t idx = std::distance(fields.begin(), it);
                                    if (idx < call->args.size() && is_string_expr(call->args[idx].get())) {
                                        it->second = "const char*";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto& [key, str_params] : m_fn_string_params) {
        auto dot = key.find('.');
        if (dot == std::string::npos) continue;
        std::string cname = key.substr(0, dot);
        auto& cfields = m_class_fields[cname];
        for (auto& sp : str_params) {
            for (auto& f : cfields) {
                if (f.first == sp && f.second == "double") {
                    f.second = "const char*";
                }
            }
        }
        auto pit = m_class_parent.find(cname);
        if (pit != m_class_parent.end()) {
            for (auto& pname : pit->second) {
                auto& pfields = m_class_fields[pname];
                for (auto& sp : str_params) {
                    for (auto& f : pfields) {
                        if (f.first == sp && f.second == "double") {
                            f.second = "const char*";
                        }
                    }
                }
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            auto* cls = static_cast<ClassStmt*>(stmt.get());
            for (auto& m : cls->methods) {
                auto* fn = static_cast<FnStmt*>(m.get());
                for (auto& s : fn->body) {
                    if (s && s->type == NodeType::EXPR_STMT) {
                        auto* es = static_cast<ExprStmt*>(s.get());
                        if (es->expression->type == NodeType::SUPER_METHOD_CALL_EXPR) {
                            auto* sc = static_cast<SuperMethodCallExpr*>(es->expression.get());
                            auto pit = m_class_parent.find(cls->name);
                            if (pit != m_class_parent.end() && !pit->second.empty()) {
                                std::string parent_name = pit->second.front();
                                auto mpit = m_class_method_params.find(parent_name);
                                if (mpit != m_class_method_params.end()) {
                                    for (auto& [mn, mp] : mpit->second) {
                                        if (mn == sc->method) {
                                            size_t arg_i = 0;
                                            for (size_t i = 0; i < mp.size(); ++i) {
                                                if (mp[i] == "self") continue;
                                                if (arg_i < sc->args.size()) {
                                                    auto& arg = sc->args[arg_i];
                                                    bool is_str = false;
                                                    if (arg->type == NodeType::STRING_EXPR) {
                                                        is_str = true;
                                                    } else if (arg->type == NodeType::IDENT_EXPR) {
                                                        std::string arg_name = static_cast<IdentExpr*>(arg.get())->name;
                                                        auto& child_fields = m_class_fields[cls->name];
                                                        for (auto& f : child_fields) {
                                                            if (f.first == arg_name && f.second == "const char*") {
                                                                is_str = true;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    if (is_str) {
                                                        auto& pf = m_class_fields[parent_name];
                                                        for (auto& f : pf) {
                                                            if (f.first == mp[i] && f.second == "double") {
                                                                f.second = "const char*";
                                                            }
                                                        }
                                                    }
                                                }
                                                arg_i++;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto& [key, param_types] : m_class_static_param_types) {
        auto dot = key.find('.');
        std::string cname = key.substr(0, dot);
        std::string mname = key.substr(dot + 1);
        for (auto& [pname, ptype] : param_types) {
            if (ptype != "double") continue;
            for (auto& stmt : prog->statements) {
                if (stmt && stmt->type == NodeType::EXPR_STMT) {
                    auto* es = static_cast<ExprStmt*>(stmt.get());
                    if (es->expression->type == NodeType::METHOD_CALL_EXPR) {
                        auto* mc = static_cast<MethodCallExpr*>(es->expression.get());
                        if (mc->object->type == NodeType::IDENT_EXPR) {
                            auto* id = static_cast<IdentExpr*>(mc->object.get());
                            if (id->name == cname && mc->method == mname) {
                                for (size_t i = 0; i < mc->args.size(); ++i) {
                                    if (i < param_types.size() && param_types[i].first == pname) {
                                        if (is_string_expr(mc->args[i].get())) {
                                            param_types[i].second = "const char*";
                                        }
                                    }
                                }
                            }
                        }
                    } else if (es->expression->type == NodeType::LET_STMT) {
                        auto* let = static_cast<LetStmt*>(es->expression.get());
                        if (let->initializer->type == NodeType::METHOD_CALL_EXPR) {
                            auto* mc = static_cast<MethodCallExpr*>(let->initializer.get());
                            if (mc->object->type == NodeType::IDENT_EXPR) {
                                auto* id = static_cast<IdentExpr*>(mc->object.get());
                                if (id->name == cname && mc->method == mname) {
                                    for (size_t i = 0; i < mc->args.size(); ++i) {
                                        if (i < param_types.size() && param_types[i].first == pname) {
                                            if (is_string_expr(mc->args[i].get())) {
                                                param_types[i].second = "const char*";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // C header imports first
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportStmt*>(stmt.get());
            if (imp->is_c_header) {
                out += "#include <" + imp->header + ">\n";
            }
        }
    }

    out += "#include <stdio.h>\n";
    out += "#include <stdlib.h>\n";
    out += "#include <string.h>\n";
    out += "#include <math.h>\n";
    out += "#include <stdint.h>\n";
    out += "typedef struct { double* elements; int size; int capacity; } SpySet;\n";
    out += "#include <time.h>\n";
    out += "#include <sys/stat.h>\n";
    out += "#ifdef _WIN32\n#include <io.h>\n#include <process.h>\n#else\n#include <unistd.h>\n#endif\n\n";

    out += "const char* spy_strcat(const char* a, const char* b);\n";
    out += "const char* spy_format(const char* fmt, ...);\n";
    out += "const char* spy_str_upper(const char* s);\n";
    out += "const char* spy_str_lower(const char* s);\n";
    out += "const char* spy_str_strip(const char* s);\n";
    out += "const char* spy_str_replace(const char* s, const char* old, const char* repl);\n";
    out += "double spy_str_find(const char* s, const char* sub);\n";
    out += "double spy_str_contains(const char* s, const char* sub);\n";
    out += "double spy_str_startswith(const char* s, const char* prefix);\n";
    out += "double spy_str_endswith(const char* s, const char* suffix);\n";
    out += "const char* spy_chr(int n);\n";
    out += "const char* spy_substr(const char* s, int start, int len);\n";
    out += "void spy_set_add(SpySet* s, double val);\n";
    out += "double spy_set_contains(SpySet* s, double val);\n";
    out += "void spy_set_remove(SpySet* s, double val);\n";
    out += "SpySet spy_set_new();\n";
    out += "typedef struct { double* elements; int size; int capacity; } SpyList;\n";
    out += "SpyList spy_list_new();\n";
    out += "void spy_list_push(SpyList* l, double val);\n";
    out += "double spy_list_pop(SpyList* l);\n";
    out += "double spy_list_get(SpyList* l, int idx);\n";
    out += "void spy_list_set(SpyList* l, int idx, double val);\n";
    out += "void spy_list_print(SpyList l);\n";
    out += "SpyList spy_str_split(const char* s, const char* delim);\n";
    out += "const char* spy_str_join(const char* delim, SpyList list);\n";
    out += "double spy_type(const char* s);\n\n";

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::ENUM_STMT) {
            auto* e = static_cast<EnumStmt*>(stmt.get());
            m_enum_variants[e->name] = e->variants;
            bool has_payload = false;
            for (auto& v : e->variants) {
                m_variant_to_enum[v.name] = e->name;
                if (v.has_payload) has_payload = true;
            }
            if (has_payload) {
                std::string lcname = e->name;
                for (auto& c : lcname) c = (char)tolower(c);
                out += "typedef enum { ";
                for (size_t i = 0; i < e->variants.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += e->name + "_Tag_" + e->variants[i].name + " = " + std::to_string(i);
                }
                out += " } " + e->name + "_Tag;\n";
                for (auto& v : e->variants) {
                    if (!v.has_payload) continue;
                    out += "typedef struct { ";
                    for (auto& f : v.fields) {
                        out += spy_type_to_c(f.type_name) + " " + f.name + "; ";
                    }
                    out += "} " + e->name + "_" + v.name + "_Data;\n";
                }
                out += "typedef struct {\n    " + e->name + "_Tag tag;\n    union {\n";
                for (auto& v : e->variants) {
                    if (!v.has_payload) continue;
                    std::string lname = v.name;
                    for (auto& c : lname) c = (char)tolower(c);
                    out += "        " + e->name + "_" + v.name + "_Data " + lname + ";\n";
                }
                out += "    } data;\n} " + e->name + ";\n\n";
                for (auto& v : e->variants) {
                    if (!v.has_payload) continue;
                    std::string lname = v.name;
                    for (auto& c : lname) c = (char)tolower(c);
                    std::string fn = lcname + "_" + lname;
                    out += e->name + " " + fn + "(";
                    for (size_t i = 0; i < v.fields.size(); ++i) {
                        if (i > 0) out += ", ";
                        out += spy_type_to_c(v.fields[i].type_name) + " " + v.fields[i].name;
                    }
                    out += ") {\n    " + e->name + " node;\n";
                    out += "    node.tag = " + e->name + "_Tag_" + v.name + ";\n";
                    for (auto& f : v.fields) {
                        out += "    node.data." + lname + "." + f.name + " = " + f.name + ";\n";
                    }
                    out += "    return node;\n}\n\n";
                }
                for (auto& v : e->variants) {
                    if (v.has_payload) continue;
                    std::string lname = v.name;
                    for (auto& c : lname) c = (char)tolower(c);
                    std::string fn = lcname + "_" + lname;
                    out += e->name + " " + fn + "() {\n    " + e->name + " node;\n";
                    out += "    node.tag = " + e->name + "_Tag_" + v.name + ";\n";
                    out += "    return node;\n}\n\n";
                }
            } else {
                out += "enum " + e->name + " { ";
                for (size_t i = 0; i < e->variants.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += e->name + "_" + e->variants[i].name + " = " + std::to_string(i);
                }
                out += " };\n";
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::EXTERN_FN_STMT) {
            emit_extern_fn(static_cast<ExternFnStmt*>(stmt.get()), out);
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::STRUCT_STMT) {
            emit_struct(static_cast<StructStmt*>(stmt.get()), out);
        }
    }
    out += "\n";

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            emit_class(static_cast<ClassStmt*>(stmt.get()), out);
        }
    }
    out += "\n";
    out += "const char* spy_strcat(const char* a, const char* b) {\n";
    out += "    size_t len = strlen(a) + strlen(b);\n";
    out += "    char* result = (char*)malloc(len + 1);\n";
    out += "    strcpy(result, a);\n";
    out += "    strcat(result, b);\n";
    out += "    return result;\n";
    out += "}\n\n";
    out += "const char* spy_str_upper(const char* s) {\n";
    out += "    size_t len = strlen(s);\n";
    out += "    char* r = (char*)malloc(len + 1);\n";
    out += "    for (size_t i = 0; i < len; i++) r[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];\n";
    out += "    r[len] = '\\0'; return r;\n";
    out += "}\n\n";
    out += "const char* spy_str_lower(const char* s) {\n";
    out += "    size_t len = strlen(s);\n";
    out += "    char* r = (char*)malloc(len + 1);\n";
    out += "    for (size_t i = 0; i < len; i++) r[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];\n";
    out += "    r[len] = '\\0'; return r;\n";
    out += "}\n\n";
    out += "const char* spy_str_strip(const char* s) {\n";
    out += "    while (*s == ' ' || *s == '\\t' || *s == '\\n' || *s == '\\r') s++;\n";
    out += "    size_t len = strlen(s);\n";
    out += "    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\\t' || s[len-1] == '\\n' || s[len-1] == '\\r')) len--;\n";
    out += "    char* r = (char*)malloc(len + 1);\n";
    out += "    memcpy(r, s, len); r[len] = '\\0'; return r;\n";
    out += "}\n\n";
    out += "const char* spy_str_replace(const char* s, const char* old, const char* repl) {\n";
    out += "    size_t slen = strlen(s), olen = strlen(old), rlen = strlen(repl);\n";
    out += "    size_t cap = slen * 2 + 1;\n";
    out += "    char* r = (char*)malloc(cap); r[0] = '\\0';\n";
    out += "    size_t pos = 0, ri = 0;\n";
    out += "    while (pos < slen) {\n";
    out += "        if (strncmp(s + pos, old, olen) == 0) {\n";
    out += "            if (ri + rlen >= cap) { cap *= 2; r = (char*)realloc(r, cap); }\n";
    out += "            memcpy(r + ri, repl, rlen); ri += rlen; pos += olen;\n";
    out += "        } else {\n";
    out += "            if (ri + 1 >= cap) { cap *= 2; r = (char*)realloc(r, cap); }\n";
    out += "            r[ri++] = s[pos++];\n";
    out += "        }\n";
    out += "    }\n";
    out += "    r[ri] = '\\0'; return r;\n";
    out += "}\n\n";
    out += "double spy_str_find(const char* s, const char* sub) {\n";
    out += "    const char* p = strstr(s, sub);\n";
    out += "    return p ? (double)(p - s) : -1.0;\n";
    out += "}\n\n";
    out += "double spy_str_contains(const char* s, const char* sub) {\n";
    out += "    return strstr(s, sub) != NULL ? 1.0 : 0.0;\n";
    out += "}\n\n";
    out += "double spy_str_startswith(const char* s, const char* prefix) {\n";
    out += "    return strncmp(s, prefix, strlen(prefix)) == 0 ? 1.0 : 0.0;\n";
    out += "}\n\n";
    out += "double spy_str_endswith(const char* s, const char* suffix) {\n";
    out += "    size_t slen = strlen(s), slen2 = strlen(suffix);\n";
    out += "    if (slen < slen2) return 0.0;\n";
    out += "    return strcmp(s + slen - slen2, suffix) == 0 ? 1.0 : 0.0;\n";
    out += "}\n\n";
    out += "const char* spy_chr(int n) {\n";
    out += "    char* r = (char*)malloc(2);\n";
    out += "    r[0] = (char)n;\n";
    out += "    r[1] = '\\0';\n";
    out += "    return r;\n";
    out += "}\n\n";
    out += "const char* spy_substr(const char* s, int start, int len) {\n";
    out += "    size_t slen = strlen(s);\n";
    out += "    if (start < 0) start = 0;\n";
    out += "    if ((size_t)start > slen) start = (int)slen;\n";
    out += "    if (len < 0) len = 0;\n";
    out += "    if ((size_t)(start + len) > slen) len = (int)(slen - start);\n";
    out += "    char* r = (char*)malloc((size_t)len + 1);\n";
    out += "    memcpy(r, s + start, (size_t)len);\n";
    out += "    r[len] = '\\0';\n";
    out += "    return r;\n";
    out += "}\n\n";
    out += "void spy_matrix_print(int rows, int cols, double* m) {\n";
    out += "    for (int i = 0; i < rows; i++) {\n";
    out += "        for (int j = 0; j < cols; j++) {\n";
    out += "            if (j > 0) printf(\" \");\n";
    out += "            printf(\"%g\", m[i * cols + j]);\n";
    out += "        }\n";
    out += "        printf(\"\\n\");\n";
    out += "    }\n";
    out += "}\n\n";
    out += "const char* spy_read_file(const char* path) {\n";
    out += "    FILE* f = fopen(path, \"rb\");\n";
    out += "    if (!f) return \"\";\n";
    out += "    fseek(f, 0, SEEK_END);\n";
    out += "    long len = ftell(f);\n";
    out += "    fseek(f, 0, SEEK_SET);\n";
    out += "    char* buf = (char*)malloc(len + 1);\n";
    out += "    fread(buf, 1, len, f);\n";
    out += "    buf[len] = '\\0';\n";
    out += "    fclose(f);\n";
    out += "    return buf;\n";
    out += "}\n\n";
    out += "double spy_write_file(const char* path, const char* data) {\n";
    out += "    FILE* f = fopen(path, \"wb\");\n";
    out += "    if (!f) return 0;\n";
    out += "    fwrite(data, 1, strlen(data), f);\n";
    out += "    fclose(f);\n";
    out += "    return 1;\n";
    out += "}\n\n";
    out += "const char* spy_input(const char* prompt) {\n";
    out += "    if (prompt && strlen(prompt) > 0) printf(\"%s\", prompt);\n";
    out += "    static char __spy_input_buf[4096];\n";
    out += "    if (fgets(__spy_input_buf, 4096, stdin)) {\n";
    out += "        size_t len = strlen(__spy_input_buf);\n";
    out += "        while (len > 0 && (__spy_input_buf[len-1] == '\\n' || __spy_input_buf[len-1] == '\\r')) __spy_input_buf[--len] = '\\0';\n";
    out += "    }\n";
    out += "    return __spy_input_buf;\n";
    out += "}\n\n";
    out += "const char* spy_exec(const char* cmd) {\n";
    out += "    static char __spy_exec_buf[65536];\n";
    out += "    FILE* pipe = popen(cmd, \"r\");\n";
    out += "    if (!pipe) return \"\";\n";
    out += "    __spy_exec_buf[0] = '\\0';\n";
    out += "    size_t total = 0;\n";
    out += "    char line[4096];\n";
    out += "    while (fgets(line, sizeof(line), pipe)) {\n";
    out += "        size_t linelen = strlen(line);\n";
    out += "        if (total + linelen < 65535) {\n";
    out += "            memcpy(__spy_exec_buf + total, line, linelen);\n";
    out += "            total += linelen;\n";
    out += "        }\n";
    out += "    }\n";
    out += "    __spy_exec_buf[total] = '\\0';\n";
    out += "    pclose(pipe);\n";
    out += "    return __spy_exec_buf;\n";
    out += "}\n\n";
    out += "double spy_file_exists(const char* path) {\n";
    out += "    FILE* f = fopen(path, \"rb\");\n";
    out += "    if (f) { fclose(f); return 1.0; }\n";
    out += "    return 0.0;\n";
    out += "}\n\n";
    out += "double spy_file_size(const char* path) {\n";
    out += "    FILE* f = fopen(path, \"rb\");\n";
    out += "    if (!f) return -1.0;\n";
    out += "    fseek(f, 0, SEEK_END);\n";
    out += "    double sz = (double)ftell(f);\n";
    out += "    fclose(f);\n";
    out += "    return sz;\n";
    out += "}\n\n";
    out += "double spy_mkdir(const char* path) {\n";
    out += "    return (double)mkdir(path, 0755);\n";
    out += "}\n\n";
    out += "double spy_remove_file(const char* path) {\n";
    out += "    return (double)remove(path);\n";
    out += "}\n\n";
    out += "double spy_rename_file(const char* old, const char* new_name) {\n";
    out += "    return (double)rename(old, new_name);\n";
    out += "}\n\n";
    out += "const char* spy_get_cwd() {\n";
    out += "    static char __spy_cwd_buf[4096];\n";
    out += "    getcwd(__spy_cwd_buf, 4096);\n";
    out += "    return __spy_cwd_buf;\n";
    out += "}\n\n";
    out += "double spy_chdir(const char* path) {\n";
    out += "    return (double)chdir(path);\n";
    out += "}\n\n";
    out += "double spy_sum(double* arr, int len) { double s = 0; for (int i = 0; i < len; i++) s += arr[i]; return s; }\n";
    out += "double spy_min(double a, double b) { return a < b ? a : b; }\n";
    out += "double spy_max(double a, double b) { return a > b ? a : b; }\n";
    out += "int spy_cmp_double(const void* a, const void* b) { double da = *(const double*)a; double db = *(const double*)b; return (da > db) - (da < db); }\n";
    out += "static int __spy_filter_len = 0;\n";
    out += "double* spy_map(double (*fn)(double), double* arr, int len) {\n";
    out += "    double* r = (double*)malloc(len * sizeof(double));\n";
    out += "    for (int i = 0; i < len; i++) r[i] = fn(arr[i]);\n";
    out += "    return r;\n";
    out += "}\n";
    out += "double* spy_filter(double (*fn)(double), double* arr, int len, int* out_len) {\n";
    out += "    double* r = (double*)malloc(len * sizeof(double));\n";
    out += "    int j = 0;\n";
    out += "    for (int i = 0; i < len; i++) { if (fn(arr[i])) r[j++] = arr[i]; }\n";
    out += "    *out_len = j;\n";
    out += "    return r;\n";
    out += "}\n";
    out += "double* spy_sorted_copy(double* arr, int len) {\n";
    out += "    double* r = (double*)malloc(len * sizeof(double));\n";
    out += "    memcpy(r, arr, len * sizeof(double));\n";
    out += "    qsort(r, len, sizeof(double), spy_cmp_double);\n";
    out += "    return r;\n";
    out += "}\n";
    out += "double spy_none_val() { double x; uint64_t* p = (uint64_t*)&x; *p = 0x7FF8000000000000ULL; return x; }\n";
    out += "double spy_is_none(double x) { return (x != x) ? 1.0 : 0.0; }\n";
    out += "static int __spy_recursion_depth = 0;\n";
    out += "void spy_check_recursion() { if (++__spy_recursion_depth > 1000) { fprintf(stderr, \"RuntimeError: maximum recursion depth exceeded\\n\"); exit(1); } }\n";
    out += "void spy_recursion_leave() { __spy_recursion_depth--; }\n";
    out += "SpySet spy_set_new() {\n";
    out += "    SpySet s = {NULL, 0, 0};\n";
    out += "    s.elements = (double*)malloc(16 * sizeof(double));\n";
    out += "    s.capacity = 16;\n";
    out += "    return s;\n";
    out += "}\n\n";
    out += "void spy_set_add(SpySet* s, double val) {\n";
    out += "    for (int i = 0; i < s->size; i++) { if (s->elements[i] == val) return; }\n";
    out += "    if (s->size >= s->capacity) {\n";
    out += "        s->capacity *= 2;\n";
    out += "        s->elements = (double*)realloc(s->elements, s->capacity * sizeof(double));\n";
    out += "    }\n";
    out += "    s->elements[s->size++] = val;\n";
    out += "}\n";
    out += "double spy_set_contains(SpySet* s, double val) {\n";
    out += "    for (int i = 0; i < s->size; i++) { if (s->elements[i] == val) return 1.0; }\n";
    out += "    return 0.0;\n";
    out += "}\n";
    out += "void spy_set_remove(SpySet* s, double val) {\n";
    out += "    int j = 0;\n";
    out += "    for (int i = 0; i < s->size; i++) { if (s->elements[i] != val) s->elements[j++] = s->elements[i]; }\n";
    out += "    s->size = j;\n";
    out += "}\n";
    out += "SpyList spy_list_new() {\n";
    out += "    SpyList l = {NULL, 0, 0};\n";
    out += "    return l;\n";
    out += "}\n";
    out += "void spy_list_push(SpyList* l, double val) {\n";
    out += "    if (l->size >= l->capacity) {\n";
    out += "        l->capacity = l->capacity ? l->capacity * 2 : 8;\n";
    out += "        l->elements = (double*)realloc(l->elements, l->capacity * sizeof(double));\n";
    out += "    }\n";
    out += "    l->elements[l->size++] = val;\n";
    out += "}\n";
    out += "double spy_list_pop(SpyList* l) {\n";
    out += "    if (l->size > 0) return l->elements[--l->size];\n";
    out += "    return 0;\n";
    out += "}\n";
    out += "double spy_list_get(SpyList* l, int idx) {\n";
    out += "    if (idx >= 0 && idx < l->size) return l->elements[idx];\n";
    out += "    return 0;\n";
    out += "}\n";
    out += "void spy_list_set(SpyList* l, int idx, double val) {\n";
    out += "    if (idx >= 0 && idx < l->size) l->elements[idx] = val;\n";
    out += "}\n";
    out += "void spy_list_print(SpyList l) {\n";
    out += "    printf(\"[\");\n";
    out += "    for (int i = 0; i < l.size; ++i) {\n";
    out += "        if (i > 0) printf(\", \");\n";
    out += "        printf(\"%g\", l.elements[i]);\n";
    out += "    }\n";
    out += "    printf(\"]\");\n";
    out += "}\n";
    out += "static const char** __spy_split_parts = NULL;\n";
    out += "static int __spy_split_count = 0;\n";
    out += "static void __spy_free_split() {\n";
    out += "    if (__spy_split_parts) {\n";
    out += "        for (int i = 0; i < __spy_split_count; i++) free((void*)__spy_split_parts[i]);\n";
    out += "        free(__spy_split_parts);\n";
    out += "        __spy_split_parts = NULL;\n";
    out += "        __spy_split_count = 0;\n";
    out += "    }\n";
    out += "}\n";
    out += "SpyList spy_str_split(const char* s, const char* delim) {\n";
    out += "    __spy_free_split();\n";
    out += "    int dlen = (int)strlen(delim);\n";
    out += "    const char* p = s;\n";
    out += "    int count = 1;\n";
    out += "    while ((p = strstr(p, delim)) != NULL) { count++; p += dlen; }\n";
    out += "    __spy_split_parts = (const char**)malloc(count * sizeof(const char*));\n";
    out += "    __spy_split_count = count;\n";
    out += "    p = s;\n";
    out += "    for (int i = 0; i < count; i++) {\n";
    out += "        const char* next = strstr(p, delim);\n";
    out += "        int len = next ? (int)(next - p) : (int)strlen(p);\n";
    out += "        char* part = (char*)malloc(len + 1);\n";
    out += "        strncpy(part, p, len);\n";
    out += "        part[len] = '\\0';\n";
    out += "        __spy_split_parts[i] = part;\n";
    out += "        p = next ? next + dlen : p + len;\n";
    out += "    }\n";
    out += "    SpyList l = spy_list_new();\n";
    out += "    for (int i = 0; i < count; i++) {\n";
    out += "        spy_list_push(&l, (double)(intptr_t)(__spy_split_parts[i]));\n";
    out += "    }\n";
    out += "    return l;\n";
    out += "}\n";
    out += "const char* spy_str_join(const char* delim, SpyList list) {\n";
    out += "    int dlen = (int)strlen(delim);\n";
    out += "    int total = 0;\n";
    out += "    for (int i = 0; i < list.size; i++) {\n";
    out += "        if (i > 0) total += dlen;\n";
    out += "        const char* s = (const char*)(intptr_t)list.elements[i];\n";
    out += "        total += (int)strlen(s);\n";
    out += "    }\n";
    out += "    char* result = (char*)malloc(total + 1);\n";
    out += "    result[0] = '\\0';\n";
    out += "    for (int i = 0; i < list.size; i++) {\n";
    out += "        if (i > 0) strcat(result, delim);\n";
    out += "        strcat(result, (const char*)(intptr_t)list.elements[i]);\n";
    out += "    }\n";
    out += "    return result;\n";
    out += "}\n";
    out += "typedef struct {\n";
    out += "    const char** keys;\n";
    out += "    double* values;\n";
    out += "    int size;\n";
    out += "    int capacity;\n";
    out += "} SpyDict;\n\n";
    out += "SpyDict spy_dict_new() {\n";
    out += "    SpyDict d = {NULL, NULL, 0, 0};\n";
    out += "    return d;\n";
    out += "}\n\n";
    out += "double spy_dict_get(SpyDict d, const char* key) {\n";
    out += "    for (int i = 0; i < d.size; i++) {\n";
    out += "        if (strcmp(d.keys[i], key) == 0) return d.values[i];\n";
    out += "    }\n";
    out += "    return 0;\n";
    out += "}\n\n";
    out += "void spy_dict_set(SpyDict* d, const char* key, double val) {\n";
    out += "    for (int i = 0; i < d->size; i++) {\n";
    out += "        if (strcmp(d->keys[i], key) == 0) { d->values[i] = val; return; }\n";
    out += "    }\n";
    out += "    if (d->size >= d->capacity) {\n";
    out += "        d->capacity = d->capacity ? d->capacity * 2 : 8;\n";
    out += "        d->keys = (const char**)realloc(d->keys, d->capacity * sizeof(const char*));\n";
    out += "        d->values = (double*)realloc(d->values, d->capacity * sizeof(double));\n";
    out += "    }\n";
    out += "    d->keys[d->size] = key;\n";
    out += "    d->values[d->size] = val;\n";
    out += "    d->size++;\n";
    out += "}\n\n";
    out += "double* spy_dict_values(SpyDict d) {\n";
    out += "    return d.values;\n";
    out += "}\n\n";
    out += "const char** spy_dict_keys(SpyDict d) {\n";
    out += "    return d.keys;\n";
    out += "}\n\n";
    out += "void spy_dict_print(SpyDict d) {\n";
    out += "    printf(\"{\");\n";
    out += "    for (int i = 0; i < d.size; i++) {\n";
    out += "        if (i > 0) printf(\", \");\n";
    out += "        printf(\"'%s': %g\", d.keys[i], d.values[i]);\n";
    out += "    }\n";
    out += "    printf(\"}\");\n";
    out += "}\n\n";
    out += "#include <stdarg.h>\n";
    out += "static char __spy_fmt_bufs[8][4096];\n";
    out += "static int __spy_fmt_idx = 0;\n";
    out += "const char* spy_format(const char* fmt, ...) {\n";
    out += "    char* buf = __spy_fmt_bufs[__spy_fmt_idx & 7];\n";
    out += "    __spy_fmt_idx++;\n";
    out += "    va_list args;\n";
    out += "    va_start(args, fmt);\n";
    out += "    vsnprintf(buf, 4096, fmt, args);\n";
    out += "    va_end(args);\n";
    out += "    return buf;\n";
    out += "}\n\n";

    out += "#include <setjmp.h>\n";
    out += "static jmp_buf __spy_jmp_buf;\n";
    out += "static const char* __spy_error_msg = NULL;\n";
    out += "void spy_error(const char* msg) {\n";
    out += "    __spy_error_msg = msg;\n";
    out += "    longjmp(__spy_jmp_buf, 1);\n";
    out += "}\n\n";

    out += "static char* __global_argv[256];\n";
    out += "static int __global_argc = 0;\n\n";

    for (auto& li : m_lambdas) {
        auto* lambda = li.expr;
        bool has_captures = !li.captures.empty();
        if (has_captures) {
            std::string struct_name = "__closure_" + li.name;
            out += "struct " + struct_name + " {\n";
            out += "    double (*fn)(struct " + struct_name + "*, ";
            for (size_t i = 0; i < lambda->params.size(); ++i) {
                if (i > 0) out += ", ";
                out += "double";
            }
            out += ");\n";
            for (auto& cv : li.captures) {
                out += "    " + cv.c_type + " " + cv.name + ";\n";
            }
            out += "};\n\n";
            std::string fn_name = li.name + "_fn";
            out += "double " + fn_name + "(struct " + struct_name + "* __env";
            for (size_t i = 0; i < lambda->params.size(); ++i) {
                out += ", double " + lambda->params[i];
            }
            out += ") {\n";
            for (auto& cv : li.captures) {
                out += "    " + cv.c_type + " " + cv.name + " = __env->" + cv.name + ";\n";
            }
            for (auto& stmt : lambda->body) {
                if (stmt) emit_node(stmt.get(), out, 1);
            }
            out += "}\n\n";
        } else {
            out += "double " + li.name + "(";
            for (size_t i = 0; i < lambda->params.size(); ++i) {
                if (i > 0) out += ", ";
                out += "double " + lambda->params[i];
            }
            out += ") {\n";
            for (auto& stmt : lambda->body) {
                if (stmt) emit_node(stmt.get(), out, 1);
            }
            out += "}\n\n";
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportStmt*>(stmt.get());
            if (!imp->is_c_header) {
                emit_import(imp, out);
            }
        }
    }

    // Forward declarations for all non-main functions
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            FnStmt* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name == "main") continue;
            std::string ret_type = "double";
            if (fn->return_type) ret_type = type_to_c(fn->return_type.get());
            out += ret_type + " " + fn->name + "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i > 0) out += ", ";
                std::string pt = "double";
                if (i < fn->typed_params.size() && fn->typed_params[i].type) {
                    pt = type_to_c(fn->typed_params[i].type.get());
                } else if (m_fn_string_params[fn->name].count(fn->params[i])) {
                    pt = "const char*";
                }
                out += pt;
            }
            out += ");\n";
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            FnStmt* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name != "main") {
                emit_fn(fn, out);
            }
        }
    }

    out += "int main(int argc, char** argv) {\n";
    out += "    __global_argc = argc;\n";
    out += "    for (int __i = 0; __i < argc && __i < 256; __i++) __global_argv[__i] = argv[__i];\n";

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            FnStmt* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name == "main") {
                for (auto& body_stmt : fn->body) {
                    if (body_stmt) emit_node(body_stmt.get(), out, 1);
                }
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type != NodeType::IMPORT_STMT && stmt->type != NodeType::FN_STMT && stmt->type != NodeType::EXTERN_FN_STMT && stmt->type != NodeType::STRUCT_STMT) {
            emit_node(stmt.get(), out, 1);
        }
    }
    out += "    return 0;\n";
    out += "}\n";
    return out;
}

std::string Codegen::get_last_error() const {
    return m_error;
}

static bool needs_struct_prefix(const std::string& t) {
    static const std::set<std::string> c_structs = {
        "wl_list", "wl_signal", "wl_display", "wl_event_loop",
        "wl_event_source", "wl_global", "wl_resource", "wl_listener",
        "wl_callback", "wl_proxy", "wl_shm_buffer",
        "wl_fixed_t",
        "drmModeModeInfo", "drmModeCrtc", "drmModeConnector",
        "drmModeEncoder", "drmModeRes", "drmModeFB",
        "gbm_device", "gbm_surface", "gbm_bo",
        "xkb_context", "xkb_keymap", "xkb_state",
        "libinput", "libinput_device", "libinput_context",
        "libseat",
        "timeval", "timespec", "stat",
    };
    return c_structs.count(t) > 0;
}

static std::string spy_type_to_c(const std::string& t) {
    if (t == "i32") return "int";
    if (t == "i64") return "long long";
    if (t == "u32") return "unsigned int";
    if (t == "u64") return "unsigned long long";
    if (t == "f32") return "float";
    if (t == "f64") return "double";
    if (t == "bool") return "int";
    if (t == "char") return "char";
    if (t == "string") return "char*";
    if (t == "void") return "void";
    if (t.size() > 2 && t[0] == '[' && t.back() == ']') {
        return spy_type_to_c(t.substr(1, t.size() - 2)) + "*";
    }
    if (needs_struct_prefix(t)) return "struct " + t;
    return t + "*";
}

void Codegen::emit_program(Program* node, std::string& out) {
    // First pass: emit forward declarations for all functions
    for (auto& stmt : node->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            std::string ret_type = "double";
            if (fn->return_type) ret_type = type_to_c(fn->return_type.get());
            out += ret_type + " " + fn->name + "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i > 0) out += ", ";
                std::string pt = "double";
                if (i < fn->typed_params.size() && fn->typed_params[i].type) pt = type_to_c(fn->typed_params[i].type.get());
                out += pt;
            }
            out += ");\n";
        }
    }
    // Second pass: emit actual definitions
    for (auto& stmt : node->statements) {
        if (stmt) {
            emit_node(stmt.get(), out, 1);
        }
    }
}

void Codegen::emit_node(ASTNode* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');

    switch (node->type) {
        case NodeType::PRINT_STMT:
            emit_print(static_cast<PrintStmt*>(node), out, indent);
            break;
        case NodeType::LET_STMT:
            emit_let(static_cast<LetStmt*>(node), out, indent);
            break;
        case NodeType::FN_STMT:
            emit_fn(static_cast<FnStmt*>(node), out);
            break;
        case NodeType::RETURN_STMT:
            emit_return(static_cast<ReturnStmt*>(node), out, indent);
            break;
        case NodeType::IF_STMT:
            emit_if(static_cast<IfStmt*>(node), out, indent);
            break;
        case NodeType::WHILE_STMT:
            emit_while(static_cast<WhileStmt*>(node), out, indent);
            break;
        case NodeType::IMPORT_STMT:
            break;
        case NodeType::EXPR_STMT:
            emit_expr_stmt(static_cast<ExprStmt*>(node), out, indent);
            break;
        case NodeType::MATCH_STMT:
            emit_match(static_cast<MatchStmt*>(node), out, indent);
            break;
        case NodeType::FOR_STMT:
            emit_for(static_cast<ForStmt*>(node), out, indent);
            break;
        case NodeType::ASSIGN_STMT: {
            auto* a = static_cast<AssignStmt*>(node);
            if (a->target->type == NodeType::INDEX_EXPR) {
                auto* idx = static_cast<IndexExpr*>(a->target.get());
                if (idx->object->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(idx->object.get());
                    if (m_dict_vars.count(id->name) || m_named_tuple_vars.count(id->name)) {
                        out += pad + "spy_dict_set(&" + id->name + ", " + emit_expression(idx->index.get()) + ", " + emit_expression(a->value.get()) + ");\n";
                        break;
                    }
                    if (m_list_vars.count(id->name)) {
                        out += pad + "spy_list_set(&" + id->name + ", (int)" + emit_expression(idx->index.get()) + ", " + emit_expression(a->value.get()) + ");\n";
                        break;
                    }
                }
            }
            if (a->target->type == NodeType::MEMBER_EXPR) {
                auto* mem = static_cast<MemberExpr*>(a->target.get());
                if (mem->object->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(mem->object.get());
                    if (m_named_tuple_vars.count(id->name)) {
                        out += pad + "spy_dict_set(&" + id->name + ", \"" + mem->member + "\", " + emit_expression(a->value.get()) + ");\n";
                        break;
                    }
                }
            }
            std::string target = emit_expression(a->target.get());
            std::string val = emit_expression(a->value.get());
            out += pad + target + " = " + val + ";\n";
            break;
        }
        case NodeType::CLASS_STMT:
            break;
        case NodeType::ENUM_STMT:
            break;
        case NodeType::TRY_STMT:
            emit_try(static_cast<TryStmt*>(node), out, indent);
            break;
        case NodeType::BREAK_STMT:
            if (!m_loop_has_else.empty() && m_loop_has_else.back()) {
                out += pad + "__no_break_" + std::to_string(m_loop_else_counter - 1) + " = 0;\n";
            }
            out += pad + "break;\n";
            break;
        case NodeType::CONTINUE_STMT:
            out += pad + "continue;\n";
            break;
        case NodeType::YIELD_STMT: {
            auto* ys = static_cast<YieldStmt*>(node);
            std::string val = ys->value ? emit_expression(ys->value.get()) : "0.0";
            out += pad + "__spy_gen_buf[__spy_gen_len++] = " + val + ";\n";
            break;
        }
        case NodeType::GLOBAL_STMT:
            break;
        case NodeType::STRUCT_STMT:
            emit_struct(static_cast<StructStmt*>(node), out);
            break;
        case NodeType::EXTERN_FN_STMT:
            emit_extern_fn(static_cast<ExternFnStmt*>(node), out);
            break;
        default:
            break;
    }
}

void Codegen::emit_print(PrintStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');

    for (size_t i = 0; i < node->expressions.size(); ++i) {
        auto& expr_node = node->expressions[i];
        std::string expr = emit_expression(expr_node.get());
        std::string fmt;
        std::string arg;

        if (expr_node->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(expr_node.get());
            if (m_list_vars.count(id->name)) {
                if (i > 0) {
                    out += pad + "printf(\" \");\n";
                }
                out += pad + "spy_list_print(" + id->name + ");\n";
                continue;
            }
            if (m_dict_vars.count(id->name)) {
                if (i > 0) {
                    out += pad + "printf(\" \");\n";
                }
                out += pad + "spy_dict_print(" + id->name + ");\n";
                continue;
            }
            auto vit = m_var_class.find(id->name);
            if (vit != m_var_class.end()) {
                auto& methods = m_class_methods[vit->second];
                if (methods.count("__str__")) {
                    if (i > 0) {
                        out += pad + "printf(\" \");\n";
                    }
                    std::string call = vit->second + "___str__((struct " + vit->second + "*)&" + id->name + ")";
                    out += pad + "printf(\"%s\", " + call + ");\n";
                    continue;
                }
            }
        }

        if (is_string_expr(expr_node.get())) {
            fmt = "%s";
            arg = expr;
        } else if (expr_node->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(expr_node.get());
            std::string ctype = get_var_type(id->name);
            if (ctype == "const char*" || m_string_vars.count(id->name)) {
                fmt = "%s";
                arg = expr;
            } else if (ctype != "double") {
                fmt = format_for_type(ctype);
                if (ctype == "char" || ctype == "float" || ctype == "double") {
                    arg = expr;
                } else {
                    arg = "(" + ctype + ")" + expr;
                }
            } else {
                fmt = "%g";
                arg = expr;
            }
        } else if (expr_node->type == NodeType::BINOP_EXPR) {
            auto* bop = static_cast<BinOpExpr*>(expr_node.get());
            if (bop->op == "%" && is_string_expr(bop->left.get())) {
                fmt = emit_expression(bop->left.get());
                std::string fmt_str;
                if (bop->left->type == NodeType::STRING_EXPR) {
                    fmt_str = static_cast<StringExpr*>(bop->left.get())->value;
                }
                std::vector<bool> arg_needs_int;
                for (size_t fi = 0; fi < fmt_str.size(); ++fi) {
                    if (fmt_str[fi] == '%') {
                        if (fi + 1 < fmt_str.size() && fmt_str[fi + 1] == '%') { fi++; continue; }
                        bool needs_int = false;
                        size_t si = fi + 1;
                        while (si < fmt_str.size() && (fmt_str[si] == '-' || fmt_str[si] == '0' || fmt_str[si] == '+' || fmt_str[si] == ' ' || fmt_str[si] == '#' || (fmt_str[si] >= '0' && fmt_str[si] <= '9') || fmt_str[si] == '.')) si++;
                        if (si < fmt_str.size()) {
                            if (fmt_str[si] == 'd' || fmt_str[si] == 'i') needs_int = true;
                        }
                        arg_needs_int.push_back(needs_int);
                        fi = si;
                    }
                }
                if (bop->right->type == NodeType::ARRAY_EXPR) {
                    auto* arr = static_cast<ArrayExpr*>(bop->right.get());
                    std::string fargs;
                    for (size_t j = 0; j < arr->elements.size(); ++j) {
                        if (j > 0) fargs += ", ";
                        std::string a = emit_expression(arr->elements[j].get());
                        if (j < arg_needs_int.size() && arg_needs_int[j]) a = "(int)" + a;
                        fargs += a;
                    }
                    fmt = "spy_format(" + fmt + ", " + fargs + ")";
                } else {
                    std::string r = emit_expression(bop->right.get());
                    if (!arg_needs_int.empty() && arg_needs_int[0]) r = "(int)" + r;
                    fmt = "spy_format(" + fmt + ", " + r + ")";
                }
                arg = "";
            } else {
                fmt = "%g";
                arg = expr;
            }
        } else if (expr_node->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(expr_node.get());
            if (call->callee == "len") {
                fmt = "%lld";
                arg = "(long long)" + expr;
            } else if (call->callee == "get_cwd" || call->callee == "exec" || call->callee == "read_file" || call->callee == "input") {
                fmt = "%s";
                arg = "(const char*)" + expr;
            } else {
                auto rit = m_fn_return_types.find(call->callee);
                if (rit != m_fn_return_types.end() && rit->second != "double" && rit->second != "void") {
                    fmt = format_for_type(rit->second);
                    if (rit->second != "char" && rit->second != "float" && rit->second != "double") {
                        arg = "(" + rit->second + ")" + expr;
                    } else {
                        arg = expr;
                    }
                } else {
                    fmt = "%g";
                    arg = expr;
                }
            }
            } else if (expr_node->type == NodeType::INDEX_EXPR) {
                auto* idx = static_cast<IndexExpr*>(expr_node.get());
                if (idx->object->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(idx->object.get());
                    std::string ptype = get_var_type(id->name);
                    if (ptype.size() > 1 && ptype.back() == '*') {
                        std::string base = ptype.substr(0, ptype.size() - 1);
                        if (base != "double" && base != "const char*" && base != "char" && base != "float") {
                            fmt = format_for_type(base);
                            arg = "(" + base + ")" + expr;
                        }
                    }
                }
                if (fmt.empty()) {
                    fmt = "%g";
                    arg = expr;
                }
            } else if (expr_node->type == NodeType::DEREF_EXPR) {
                auto* de = static_cast<DerefExpr*>(expr_node.get());
                if (de->operand->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(de->operand.get());
                    std::string ptype = get_var_type(id->name);
                    if (ptype.size() > 1 && ptype.back() == '*') {
                        std::string base = ptype.substr(0, ptype.size() - 1);
                        fmt = format_for_type(base);
                        if (base != "char" && base != "float" && base != "double") {
                            arg = "(" + base + ")" + expr;
                        } else {
                            arg = expr;
                        }
                    }
                }
                if (fmt.empty()) {
                    fmt = "%g";
                    arg = expr;
                }
            } else if (expr_node->type == NodeType::MEMBER_EXPR) {
                auto* mem = static_cast<MemberExpr*>(expr_node.get());
                std::string ftype = get_struct_field_type(mem->object.get(), mem->member);
                if (!ftype.empty()) {
                    if (ftype == "const char*") {
                        fmt = "%s";
                    } else {
                        fmt = format_for_type(ftype);
                        if (ftype != "char" && ftype != "float" && ftype != "double") {
                            arg = "(" + ftype + ")" + expr;
                        } else {
                            arg = expr;
                        }
                    }
                } else if (mem->object->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(mem->object.get());
                    std::string vtype = get_var_type(id->name);
                    auto sit = m_struct_defs.find(vtype);
                    if (sit != m_struct_defs.end()) {
                        for (auto& [fname, f2] : sit->second) {
                            if (fname == mem->member) {
                                if (f2 == "const char*") {
                                    fmt = "%s";
                                } else {
                                    fmt = format_for_type(f2);
                                    if (f2 != "char" && f2 != "float" && f2 != "double") {
                                        arg = "(" + f2 + ")" + expr;
                                    } else {
                                        arg = expr;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    if (fmt.empty()) {
                        fmt = "%g";
                        arg = expr;
                    }
                } else {
                    fmt = "%g";
                    arg = expr;
                }
            } else if (expr_node->type == NodeType::SIZEOF_EXPR) {
                fmt = "%zu";
                arg = "(size_t)" + expr;
            } else {
                fmt = "%g";
                arg = expr;
            }

        if (i > 0) {
            out += pad + "printf(\" \");\n";
        }
        if (arg.empty()) {
            out += pad + "printf(\"%s\", " + fmt + ");\n";
        } else {
            out += pad + "printf(\"" + fmt + "\", " + arg + ");\n";
        }
    }
    out += pad + "printf(\"\\n\");\n";
}

void Codegen::emit_let(LetStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    std::string init = emit_expression(node->initializer.get());

    if (m_global_vars.count(node->name)) {
        out += pad + node->name + " = " + init + ";\n";
        return;
    }

    if (m_declared.count(node->name)) {
        out += pad + node->name + " = " + init + ";\n";
        return;
    }

    m_declared.insert(node->name);

    if (node->type_annotation) {
        std::string ctype = type_to_c(node->type_annotation.get());
        m_var_types[node->name] = ctype;
        if (node->initializer->type == NodeType::STRING_EXPR) {
            m_string_vars.insert(node->name);
        }
        if (node->initializer->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(node->initializer.get());
            if (m_structs.count(call->callee)) {
                m_var_types[node->name] = call->callee;
            }
        }
        out += pad + ctype + " " + node->name + " = " + init + ";\n";
        return;
    }

    if (node->initializer->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node->initializer.get());
        if (call->callee == "map" || call->callee == "filter" || call->callee == "sorted") {
            out += pad + "double* " + node->name + " = " + init + ";\n";
            return;
        }
        if (call->callee == "set") {
            out += pad + "SpySet " + node->name + " = " + init + ";\n";
            m_set_vars.insert(node->name);
            return;
        }
        if (call->callee == "list") {
            out += pad + "SpyList " + node->name + " = " + init + ";\n";
            m_list_vars.insert(node->name);
            return;
        }
        if (m_generator_fns.count(call->callee)) {
            out += pad + "double* " + node->name + " = " + init + ";\n";
            return;
        }
        auto ev = m_variant_to_enum.find(call->callee);
        if (ev != m_variant_to_enum.end()) {
            out += pad + ev->second + " " + node->name + " = " + init + ";\n";
            m_var_types[node->name] = ev->second;
            return;
        }
        auto rit = m_fn_return_types.find(call->callee);
        if (rit != m_fn_return_types.end() && rit->second != "double" && rit->second != "void" && rit->second != "double*") {
            out += pad + rit->second + " " + node->name + " = " + init + ";\n";
            m_var_types[node->name] = rit->second;
            return;
        }
        if (call->callee == "alloc" && call->args.size() == 2) {
            std::string ctype = type_to_c(call->args[1].get());
            out += pad + ctype + "* " + node->name + " = " + init + ";\n";
            m_var_types[node->name] = ctype + "*";
            return;
        }
        if (call->callee == "from_void_ptr" && call->args.size() == 2) {
            std::string ctype = type_to_c(call->args[0].get());
            out += pad + ctype + "* " + node->name + " = " + init + ";\n";
            m_var_types[node->name] = ctype + "*";
            return;
        }
    }

    if (node->initializer->type == NodeType::STRING_EXPR) {
        out += pad + "const char* " + node->name + " = " + init + ";\n";
        m_string_vars.insert(node->name);
    } else if (node->initializer->type == NodeType::SET_EXPR) {
        out += pad + "SpySet " + node->name + " = " + init + ";\n";
        m_set_vars.insert(node->name);
    } else if (node->initializer->type == NodeType::NAMED_TUPLE_EXPR) {
        out += pad + "SpyDict " + node->name + " = " + init + ";\n";
        m_named_tuple_vars.insert(node->name);
    } else if (node->initializer->type == NodeType::BINOP_EXPR) {
        auto* bop = static_cast<BinOpExpr*>(node->initializer.get());
        std::string struct_cls = get_binop_struct_class(node->initializer.get());
        if (!struct_cls.empty()) {
            out += pad + "struct " + struct_cls + " " + node->name + " = " + init + ";\n";
            m_var_class[node->name] = struct_cls;
        } else if (bop->op == "%" && is_string_expr(bop->left.get())) {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else if (bop->op == "+" && (bop->left->type == NodeType::STRING_EXPR || bop->right->type == NodeType::STRING_EXPR
            || m_string_vars.count(get_ident_name(bop->left.get())) || m_string_vars.count(get_ident_name(bop->right.get())))) {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            out += pad + "double " + node->name + " = " + init + ";\n";
        }
    } else if (node->initializer->type == NodeType::ARRAY_EXPR) {
        auto* arr = static_cast<ArrayExpr*>(node->initializer.get());
        bool is_2d = !arr->elements.empty() && arr->elements[0]->type == NodeType::ARRAY_EXPR;
        if (is_2d) {
            auto* row = static_cast<ArrayExpr*>(arr->elements[0].get());
            out += pad + "double " + node->name + "[]["
                + std::to_string(row->elements.size()) + "] = " + init + ";\n";
        } else {
            out += pad + "double " + node->name + "[] = " + init + ";\n";
        }
    } else if (node->initializer->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node->initializer.get());
        if (m_classes.count(call->callee)) {
            out += pad + "struct " + call->callee + " " + node->name + " = " + init + ";\n";
            m_var_class[node->name] = call->callee;
            return;
        }
        if (call->callee == "read_file" || call->callee == "input" || call->callee == "exec" || call->callee == "get_cwd"
            || call->callee == "chr" || call->callee == "substr" || call->callee == "str"
            || call->callee == "cg_emit_expr" || call->callee == "cg_type_to_c"
            || call->callee == "cg_find_ret_type" || call->callee == "cg_find_field_type") {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else if (m_fn_return_types.count(call->callee) && m_fn_return_types[call->callee] == "const char*") {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            out += pad + "double " + node->name + " = " + init + ";\n";
        }
    } else if (node->initializer->type == NodeType::DICT_EXPR) {
        out += pad + "SpyDict " + node->name + " = " + init + ";\n";
        m_dict_vars.insert(node->name);
    } else if (node->initializer->type == NodeType::LAMBDA_EXPR) {
        // Check if this lambda has captures
        bool has_captures = false;
        std::string struct_name;
        for (auto& li : m_lambdas) {
            if (li.expr == node->initializer.get()) {
                has_captures = !li.captures.empty();
                if (has_captures) {
                    struct_name = "__closure_" + li.name;
                    out += pad + "struct " + struct_name + " " + node->name + " = { " + li.name + "_fn";
                    for (auto& cv : li.captures) {
                        out += ", " + cv.name;
                    }
                    out += " };\n";
                    m_closure_vars.insert(node->name);
                } else {
                    m_declared.insert(node->name);
                }
                break;
            }
        }
        if (!has_captures) {
            m_declared.insert(node->name);
        }
    } else if (node->initializer->type == NodeType::BINOP_EXPR) {
        auto* bop = static_cast<BinOpExpr*>(node->initializer.get());
        std::string struct_cls = get_binop_struct_class(node->initializer.get());
        if (!struct_cls.empty()) {
            out += pad + "struct " + struct_cls + " " + node->name + " = " + init + ";\n";
            m_var_class[node->name] = struct_cls;
        } else if (bop->op == "%" && is_string_expr(bop->left.get())) {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            out += pad + "double " + node->name + " = " + init + ";\n";
        }
    } else if (node->initializer->type == NodeType::MEMBER_EXPR) {
        auto* mem = static_cast<MemberExpr*>(node->initializer.get());
        std::string ftype = get_struct_field_type(mem->object.get(), mem->member);
        if (ftype == "const char*" || ftype == "char*" || ftype == "char *") {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            out += pad + "double " + node->name + " = " + init + ";\n";
        }
    } else if (node->initializer->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(node->initializer.get());
        if (m_string_vars.count(id->name)) {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            auto ev = m_variant_to_enum.find(id->name);
            if (ev != m_variant_to_enum.end()) {
                out += pad + ev->second + " " + node->name + " = " + init + ";\n";
                m_var_types[node->name] = ev->second;
            } else {
                out += pad + "double " + node->name + " = " + init + ";\n";
            }
        }
    } else if (node->initializer->type == NodeType::METHOD_CALL_EXPR) {
        auto* mc = static_cast<MethodCallExpr*>(node->initializer.get());
        if (mc->method == "split") {
            out += pad + "SpyList " + node->name + " = " + init + ";\n";
            m_list_vars.insert(node->name);
        } else if (is_string_expr(node->initializer.get())) {
            out += pad + "const char* " + node->name + " = " + init + ";\n";
            m_string_vars.insert(node->name);
        } else {
            out += pad + "double " + node->name + " = " + init + ";\n";
        }
    } else if (is_string_expr(node->initializer.get())) {
        out += pad + "const char* " + node->name + " = " + init + ";\n";
        m_string_vars.insert(node->name);
    } else if (node->initializer->type == NodeType::LIST_COMP_EXPR) {
        auto* lc = static_cast<ListCompExpr*>(node->initializer.get());
        if (lc->iterable->type == NodeType::CALL_EXPR) {
            auto* call = static_cast<CallExpr*>(lc->iterable.get());
            if (call->callee == "range" && call->args.size() == 1) {
                std::string n = emit_expression(call->args[0].get());
                std::string elem = emit_expression(lc->element.get());
                out += pad + "double " + node->name + "[(int)(" + n + ")];\n";
                out += pad + "for (double " + lc->var + " = 0; " + lc->var + " < (double)(" + n + "); " + lc->var + "++) {\n";
                out += pad + "    " + node->name + "[(int)" + lc->var + "] = " + elem + ";\n";
                out += pad + "}\n";
            }
        }
    } else if (node->initializer->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExpr*>(node->initializer.get());
        if (idx->object->type == NodeType::MEMBER_EXPR) {
            auto* mem = static_cast<MemberExpr*>(idx->object.get());
            std::string ftype = get_struct_field_type(mem->object.get(), mem->member);
            if (!ftype.empty() && ftype.back() == '*') {
                std::string elem_type = ftype;
                elem_type.pop_back();
                while (!elem_type.empty() && elem_type.back() == ' ') elem_type.pop_back();
                out += pad + elem_type + " " + node->name + " = " + init + ";\n";
                m_var_types[node->name] = elem_type;
                return;
            }
        }
        out += pad + "double " + node->name + " = " + init + ";\n";
    } else {
        out += pad + "double " + node->name + " = " + init + ";\n";
    }
}

void Codegen::emit_fn(FnStmt* node, std::string& out) {
    bool is_gen = m_generator_fns.count(node->name) > 0;
    std::string ret_type = "double";
    if (node->return_type) {
        ret_type = type_to_c(node->return_type.get());
    } else if (is_gen) {
        ret_type = "double*";
    }
    m_fn_return_types[node->name] = ret_type;
    out += ret_type + " " + node->name + "(";
    for (size_t i = 0; i < node->params.size(); ++i) {
        if (i > 0) out += ", ";
        std::string param_type = "double";
        if (i < node->typed_params.size() && node->typed_params[i].type) {
            param_type = type_to_c(node->typed_params[i].type.get());
        } else {
            bool is_str = m_fn_string_params[node->name].count(node->params[i]) > 0;
            if (node->defaults.count(node->params[i]) && node->defaults.at(node->params[i]) && is_string_expr(node->defaults.at(node->params[i]).get())) {
                is_str = true;
                m_fn_string_params[node->name].insert(node->params[i]);
            }
            if (is_str) {
                param_type = "const char*";
            }
        }
        out += param_type + " " + node->params[i];
        m_var_types[node->params[i]] = param_type;
        if (param_type.find('*') != std::string::npos) {
            m_ptr_params.insert(node->params[i]);
        }
    }
    out += ") {\n";

    std::set<std::string> saved_declared;
    for (auto& d : m_declared) saved_declared.insert(d);
    m_declared.clear();
    for (auto& p : node->params) {
        m_declared.insert(p);
    }

    out += "    spy_check_recursion();\n";

    if (is_gen) {
        out += "    __spy_gen_len = 0;\n";
    }

    if (node->decorator == "log") {
        out += "    printf(\"calling " + node->name + "\\n\");\n";
    } else if (node->decorator == "timer") {
        out += "    clock_t __start = clock();\n";
    }

    std::set<std::string> saved;
    for (auto& p : m_fn_string_params[node->name]) {
        if (m_string_vars.count(p) == 0) {
            m_string_vars.insert(p);
            saved.insert(p);
        }
    }

    for (auto& stmt : node->body) {
        if (stmt) {
            emit_node(stmt.get(), out, 1);
        }
    }

    for (auto& p : saved) {
        m_string_vars.erase(p);
    }

    m_declared = saved_declared;

    if (node->decorator == "log") {
        out += "    printf(\"" + node->name + " returned\\n\");\n";
    } else if (node->decorator == "timer") {
        out += "    printf(\"" + node->name + ": %g sec\\n\", (double)(clock() - __start) / CLOCKS_PER_SEC);\n";
    }

    out += "    spy_recursion_leave();\n";
    if (is_gen) {
        out += "    return __spy_gen_buf;\n";
    }
    if (ret_type != "void" && ret_type != "double*") {
        bool has_return = false;
        for (auto& stmt : node->body) {
            if (stmt && stmt->type == NodeType::RETURN_STMT) {
                has_return = true;
                break;
            }
        }
        if (!has_return) {
            out += "    return 0;\n";
        }
    }
    out += "}\n\n";
    m_ptr_params.clear();
}

void Codegen::emit_return(ReturnStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    out += pad + "spy_recursion_leave();\n";
    if (node->expression) {
        out += pad + "return " + emit_expression(node->expression.get()) + ";\n";
    } else {
        out += pad + "return 0.0;\n";
    }
}

void Codegen::emit_if(IfStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    std::set<std::string> saved;
    for (auto& d : m_declared) saved.insert(d);
    out += pad + "if (" + emit_expression(node->condition.get()) + ") {\n";
    emit_block(node->then_body, out, indent + 1);
    m_declared = saved;
    if (node->has_else) {
        out += pad + "} else {\n";
        emit_block(node->else_body, out, indent + 1);
        m_declared = saved;
    }
    out += pad + "}\n";
}

void Codegen::emit_while(WhileStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    bool has_else = !node->else_body.empty();
    std::string flag;
    if (has_else) {
        flag = "__no_break_" + std::to_string(m_loop_else_counter++);
        out += pad + "int " + flag + " = 1;\n";
        m_loop_has_else.push_back(true);
    }
    out += pad + "while (" + emit_expression(node->condition.get()) + ") {\n";
    emit_block(node->body, out, indent + 1);
    out += pad + "}\n";
    if (has_else) {
        m_loop_has_else.pop_back();
        out += pad + "if (" + flag + ") {\n";
        emit_block(node->else_body, out, indent + 1);
        out += pad + "}\n";
    }
}

void Codegen::emit_import(ImportStmt* node, std::string& out) {
    if (node->is_c_header) {
        out += "#include <" + node->header + ">\n";
        return;
    }
    std::string mod = node->module_name;
    std::string path = find_module_file(mod);
    if (path.empty()) {
        m_error = "cannot find module '" + mod + "'";
        return;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        m_error = "cannot open module file '" + path + "'";
        return;
    }
    std::string src((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    Lexer lexer(src, path);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto* prog = static_cast<Program*>(ast.get());
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportStmt*>(stmt.get());
            if (!imp->is_c_header) {
                std::string nested_mod = imp->module_name;
                std::string nested_path = find_module_file(nested_mod);
                if (!nested_path.empty()) {
                    std::ifstream nfile(nested_path);
                    std::string nsrc((std::istreambuf_iterator<char>(nfile)), std::istreambuf_iterator<char>());
                    nfile.close();
                    Lexer nlexer(nsrc, nested_path);
                    auto ntokens = nlexer.tokenize();
                    Parser nparser(ntokens);
                    auto nast = nparser.parse();
                    auto* nprog = static_cast<Program*>(nast.get());
                    for (auto& ns : nprog->statements) {
                        if (ns && ns->type == NodeType::FN_STMT) {
                            auto* nfn = static_cast<FnStmt*>(ns.get());
                            m_module_fns[nested_mod].insert(nfn->name);
                            std::string nfull = m_module_prefix + mod + "_" + nested_mod + "_" + nfn->name;
                            m_fn_defaults[nfull] = nfn;
                            std::string nbody;
                            nbody += "double " + nfull + "(";
                            for (size_t i = 0; i < nfn->params.size(); ++i) {
                                if (i > 0) nbody += ", ";
                                nbody += "double " + nfn->params[i];
                            }
                            nbody += ") {\n";
                            Codegen ninner;
                            ninner.m_module_prefix = m_module_prefix + mod + "_" + nested_mod + "_";
                            ninner.m_declared = m_declared;
                            ninner.m_module_fns = m_module_fns;
                            ninner.m_module_from_fns = m_module_from_fns;
                            ninner.m_fn_defaults = m_fn_defaults;
                            for (auto& ss : nfn->body) {
                                if (ss) ninner.emit_node(ss.get(), nbody, 1);
                            }
                            nbody += "}\n\n";
                            out += nbody;
                        } else if (ns && ns->type == NodeType::CLASS_STMT) {
                            auto* ncls = static_cast<ClassStmt*>(ns.get());
                            m_module_fns[nested_mod].insert(ncls->name);
                        }
                    }
                    if (!imp->names.empty()) {
                        for (auto& n : imp->names) {
                            m_module_from_fns[nested_mod].insert(n);
                        }
                    }
                }
            }
        }
    }
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            std::string prefixed = mod + "_" + fn->name;
            m_module_fns[mod].insert(fn->name);
            m_fn_defaults[prefixed] = fn;
            std::string body;
            body += "double " + prefixed + "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i > 0) body += ", ";
                body += "double " + fn->params[i];
            }
            body += ") {\n";
            Codegen inner;
            inner.m_module_prefix = m_module_prefix + mod + "_";
            inner.m_declared = m_declared;
            inner.m_module_fns = m_module_fns;
            inner.m_module_from_fns = m_module_from_fns;
            inner.m_fn_defaults = m_fn_defaults;
            for (auto& s : fn->body) {
                if (s) inner.emit_node(s.get(), body, 1);
            }
            body += "}\n\n";
            out += body;
        } else if (stmt && stmt->type == NodeType::CLASS_STMT) {
            auto* cls = static_cast<ClassStmt*>(stmt.get());
            m_module_fns[mod].insert(cls->name);
        }
    }
    if (!node->names.empty()) {
        for (auto& n : node->names) {
            m_module_from_fns[mod].insert(n);
        }
    }
}

void Codegen::emit_match(MatchStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    std::string val = emit_expression(node->value.get());
    bool has_ctor = false;
    for (auto& c : node->cases) {
        if (c.pattern && c.pattern->type == NodeType::CONSTRUCTOR_PATTERN) {
            has_ctor = true; break;
        }
    }
    if (has_ctor) {
        out += pad + "switch (" + val + ".tag) {\n";
        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& c = node->cases[i];
            if (c.pattern->type == NodeType::NONE_EXPR) continue;
            if (c.pattern->type == NodeType::CONSTRUCTOR_PATTERN) {
                auto* cp = static_cast<ConstructorPattern*>(c.pattern.get());
                auto it = m_variant_to_enum.find(cp->variant_name);
                if (it != m_variant_to_enum.end()) {
                    std::string tag = it->second + "_Tag_" + cp->variant_name;
                    out += pad + "    case " + tag + ": {\n";
                    auto& variants = m_enum_variants[it->second];
                    for (auto& v : variants) {
                        if (v.name == cp->variant_name && v.has_payload) {
                            std::string lname = cp->variant_name;
                            for (auto& c_ : lname) c_ = (char)tolower(c_);
                            for (size_t fi = 0; fi < v.fields.size() && fi < cp->bindings.size(); ++fi) {
                                std::string bt = spy_type_to_c(v.fields[fi].type_name);
                                out += pad + "        " + bt + " " + cp->bindings[fi] + " = " + val + ".data." + lname + "." + v.fields[fi].name + ";\n";
                            }
                            break;
                        }
                    }
                    for (auto& s : c.body) if (s) emit_node(s.get(), out, indent + 2);
                    out += pad + "        break;\n";
                    out += pad + "    }\n";
                }
            } else {
                std::string pat;
                if (c.pattern->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(c.pattern.get());
                    auto pit = m_variant_to_enum.find(id->name);
                    if (pit != m_variant_to_enum.end()) {
                        pat = pit->second + "_Tag_" + id->name;
                    } else {
                        pat = emit_expression(c.pattern.get());
                    }
                } else {
                    pat = emit_expression(c.pattern.get());
                }
                out += pad + "    case " + pat + ": {\n";
                for (auto& s : c.body) if (s) emit_node(s.get(), out, indent + 2);
                out += pad + "        break;\n";
                out += pad + "    }\n";
            }
        }
        if (node->default_index >= 0) {
            out += pad + "    default: {\n";
            auto& dc = node->cases[node->default_index];
            for (auto& s : dc.body) if (s) emit_node(s.get(), out, indent + 2);
            out += pad + "        break;\n";
            out += pad + "    }\n";
        }
        out += pad + "}\n";
    } else {
        std::string tmp = "__match_val";
        out += pad + "double " + tmp + " = " + val + ";\n";
        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& c = node->cases[i];
            if (c.pattern->type == NodeType::NONE_EXPR) {
                out += pad + "} else {\n";
            } else {
                std::string pat = emit_expression(c.pattern.get());
                if (i == 0) {
                    out += pad + "if (" + tmp + " == " + pat + ") {\n";
                } else {
                    out += pad + "} else if (" + tmp + " == " + pat + ") {\n";
                }
            }
            for (auto& s : c.body) if (s) emit_node(s.get(), out, indent + 1);
        }
        out += pad + "}\n";
    }
}

void Codegen::emit_for(ForStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    bool has_else = !node->else_body.empty();
    std::string flag;
    if (has_else) {
        flag = "__no_break_" + std::to_string(m_loop_else_counter++);
        out += pad + "int " + flag + " = 1;\n";
        m_loop_has_else.push_back(true);
    }
    if (node->iterable) {
        std::string iter = emit_expression(node->iterable.get());
        std::string iter_name = "__iter_" + std::to_string(node->line) + "_" + std::to_string(node->column);
        std::string iter_id = get_ident_name(node->iterable.get());
        if (m_list_vars.count(iter_id)) {
            out += pad + "for (int __i_" + iter_name + " = 0; __i_" + iter_name + " < " + iter + ".size; __i_" + iter_name + "++) {\n";
            out += pad + "    double " + node->var + " = " + iter + ".elements[__i_" + iter_name + "];\n";
        } else {
            out += pad + "double* " + iter_name + " = " + iter + ";\n";
            out += pad + "int __len_" + iter_name + " = (int)(sizeof(" + iter + ") / sizeof(" + iter + "[0]));\n";
            out += pad + "for (int __i_" + iter_name + " = 0; __i_" + iter_name + " < __len_" + iter_name + "; __i_" + iter_name + "++) {\n";
            out += pad + "    double " + node->var + " = " + iter_name + "[__i_" + iter_name + "];\n";
        }
        emit_block(node->body, out, indent + 1);
        out += pad + "}\n";
    } else {
        std::string start = emit_expression(node->start.get());
        std::string end = emit_expression(node->end.get());
        out += pad + "for (double " + node->var + " = " + start + "; " + node->var + " < (double)(" + end + "); " + node->var + "++) {\n";
        emit_block(node->body, out, indent + 1);
        out += pad + "}\n";
    }
    if (has_else) {
        m_loop_has_else.pop_back();
        out += pad + "if (" + flag + ") {\n";
        emit_block(node->else_body, out, indent + 1);
        out += pad + "}\n";
    }
}

void Codegen::emit_try(TryStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    out += pad + "if (setjmp(__spy_jmp_buf) == 0) {\n";
    emit_block(node->body, out, indent + 1);
    out += pad + "} else {\n";
    if (!node->handlers.empty()) {
        auto* handler = static_cast<ExceptHandler*>(node->handlers[0].get());
        if (!handler->var_name.empty()) {
            out += pad + "    const char* " + handler->var_name + " = __spy_error_msg;\n";
            m_string_vars.insert(handler->var_name);
        }
        emit_block(handler->body, out, indent + 1);
    }
    out += pad + "}\n";
}

void Codegen::emit_class(ClassStmt* node, std::string& out) {
    m_classes.insert(node->name);
    m_current_class = node->name;

    auto& fields = m_class_fields[node->name];

    if (!node->parents.empty()) {
        out += "struct " + node->name + " {\n";
        for (auto& pname : node->parents) {
            out += "    struct " + pname + " __parent_" + pname + ";\n";
        }
        for (auto& f : fields) {
            bool inherited = false;
            for (auto& pname : node->parents) {
                auto& pf = m_class_fields[pname];
                for (auto& pfi : pf) {
                    if (pfi.first == f.first) { inherited = true; break; }
                }
                if (inherited) break;
            }
            if (!inherited) {
                out += "    " + f.second + " " + f.first + ";\n";
            }
        }
        out += "};\n\n";
    } else {
        out += "struct " + node->name + " {\n";
        for (auto& f : fields) {
            out += "    " + f.second + " " + f.first + ";\n";
        }
        out += "};\n\n";
    }

    std::set<std::string> defined_methods;
    for (auto& m : node->methods) {
        auto* fn = static_cast<FnStmt*>(m.get());
        std::string prefix = node->name + "_" + fn->name;
        bool is_static = fn->params.empty() || fn->params[0] != "self";
        if (is_static) {
            out += "double " + prefix + "(";
            auto& param_types = m_class_static_param_types[node->name + "." + fn->name];
            bool first = true;
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (!first) out += ", ";
                first = false;
                std::string ptype = "double";
                if (i < param_types.size()) {
                    ptype = param_types[i].second;
                    if (ptype == "const char*") {
                        m_class_string_params.insert(fn->params[i]);
                    }
                }
                out += ptype + " " + fn->params[i];
            }
        } else {
            std::string ret_type = "double";
            if (fn->name == "__str__") ret_type = "const char*";
            else if (fn->name == "__add__" || fn->name == "__sub__" || fn->name == "__mul__" || fn->name == "__div__") {
                ret_type = "struct " + node->name;
                m_class_dunder_returns_struct[node->name].insert(fn->name);
            }
            out += ret_type + " " + prefix + "(struct " + node->name + "* self";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (fn->params[i] == "self") continue;
                std::string ptype = "double";
                if (fn->name == "__add__" || fn->name == "__sub__" ||
                    fn->name == "__eq__" || fn->name == "__ne__" || fn->name == "__lt__" || fn->name == "__gt__" ||
                    fn->name == "__le__" || fn->name == "__ge__") {
                    ptype = "struct " + node->name + "*";
                } else {
                    for (auto& f : fields) {
                        if (f.first == fn->params[i]) {
                            ptype = f.second;
                            break;
                        }
                    }
                }
                if (fn->name == "__init__" && ptype == "const char*") {
                    m_class_string_params.insert(fn->params[i]);
                }
                out += ", " + ptype + " " + fn->params[i];
            }
        }
        out += ") {\n";
        std::set<std::string> saved_string_vars;
        if (is_static) {
            for (auto& sp : m_class_string_params) {
                if (m_string_vars.count(sp) == 0) {
                    m_string_vars.insert(sp);
                    saved_string_vars.insert(sp);
                }
            }
        }
        if (!is_static) {
            for (auto& pname : fn->params) {
                if (pname != "self") {
                    bool is_ptr = false;
                    for (auto& pn : node->parents) {
                        if (pn == node->name) { is_ptr = true; break; }
                    }
                    if (fn->name == "__add__" || fn->name == "__sub__" ||
                        fn->name == "__eq__" || fn->name == "__ne__" || fn->name == "__lt__" || fn->name == "__gt__" ||
                        fn->name == "__le__" || fn->name == "__ge__") {
                        is_ptr = true;
                    }
                    if (is_ptr) {
                        m_ptr_params.insert(pname);
                    }
                }
            }
        }
        for (auto& s : fn->body) {
            if (s) emit_node(s.get(), out, 1);
        }
        for (auto& sp : saved_string_vars) {
            m_string_vars.erase(sp);
        }
        m_ptr_params.clear();
        bool has_struct_return = false;
        if (!is_static) {
            auto rs_it = m_class_dunder_returns_struct.find(node->name);
            if (rs_it != m_class_dunder_returns_struct.end() && rs_it->second.count(fn->name)) {
                has_struct_return = true;
            }
        }
        if (fn->name == "__str__") has_struct_return = true;
        if (!has_struct_return) {
            out += "    return 0.0;\n";
        }
        out += "}\n\n";
        defined_methods.insert(fn->name);
    }

    if (!node->parents.empty()) {
        for (auto& pname : node->parents) {
            auto& parent_method_params = m_class_method_params[pname];
            for (auto& [pm_name, pm_params] : parent_method_params) {
                if (defined_methods.count(pm_name)) continue;
                bool parent_is_static = m_class_static_methods[pname].count(pm_name) > 0;
                std::string prefix = node->name + "_" + pm_name;
                if (parent_is_static) {
                    std::string pkey = pname + "." + pm_name;
                    auto ptit = m_class_static_param_types.find(pkey);
                    out += "double " + prefix + "(";
                    bool first = true;
                    for (size_t pi = 0; pi < pm_params.size(); ++pi) {
                        if (pm_params[pi] == "self") continue;
                        if (!first) out += ", ";
                        first = false;
                        std::string ptype = "double";
                        if (ptit != m_class_static_param_types.end() && pi < ptit->second.size()) {
                            ptype = ptit->second[pi].second;
                        }
                        out += ptype + " " + pm_params[pi];
                    }
                    out += ") {\n";
                    out += "    return " + pname + "_" + pm_name + "(";
                    first = true;
                    for (auto& p : pm_params) {
                        if (p == "self") continue;
                        if (!first) out += ", ";
                        first = false;
                        out += p;
                    }
                    out += ");\n";
                    out += "}\n\n";
                } else {
                    out += "double " + prefix + "(struct " + node->name + "* self";
                    for (auto& p : pm_params) {
                        if (p == "self") continue;
                        std::string ptype = "double";
                        for (auto& f : m_class_fields[pname]) {
                            if (f.first == p) { ptype = f.second; break; }
                        }
                        out += ", " + ptype + " " + p;
                    }
                    out += ") {\n";
                    out += "    return " + pname + "_" + pm_name + "((struct " + pname + "*)&self->__parent_" + pname;
                    for (auto& p : pm_params) {
                        if (p == "self") continue;
                        out += ", " + p;
                    }
                    out += ");\n";
                    out += "}\n\n";
                }
            }
        }
    }
}

void Codegen::emit_block(const std::vector<ASTPtr>& stmts, std::string& out, int indent) {
    for (auto& stmt : stmts) {
        if (stmt) {
            emit_node(stmt.get(), out, indent);
        }
    }
}

void Codegen::emit_expr_stmt(ExprStmt* node, std::string& out, int indent) {
    std::string pad(indent * 4, ' ');
    std::string expr = emit_expression(node->expression.get());
    out += pad + expr + ";\n";
}

std::string Codegen::get_ident_name(ASTNode* expr) {
    if (expr->type == NodeType::IDENT_EXPR) {
        return static_cast<IdentExpr*>(expr)->name;
    }
    return "";
}

bool Codegen::is_string_expr(ASTNode* expr) {
    if (expr->type == NodeType::STRING_EXPR) return true;
    if (expr->type == NodeType::IDENT_EXPR) {
        return m_string_vars.count(static_cast<IdentExpr*>(expr)->name) > 0;
    }
    if (expr->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExpr*>(expr);
        if (idx->object->type == NodeType::MEMBER_EXPR) {
            auto* mem = static_cast<MemberExpr*>(idx->object.get());
            if (mem->object->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(mem->object.get());
                if (id->name == "sys" && mem->member == "argv") return true;
            }
        }
    }
    if (expr->type == NodeType::BINOP_EXPR) {
        auto* bop = static_cast<BinOpExpr*>(expr);
        if (bop->op == "+") return is_string_expr(bop->left.get()) || is_string_expr(bop->right.get());
    }
    if (expr->type == NodeType::MEMBER_EXPR) {
        auto* mem = static_cast<MemberExpr*>(expr);
        std::string struct_ftype = get_struct_field_type(mem->object.get(), mem->member);
        if (!struct_ftype.empty() && (struct_ftype == "const char*" || struct_ftype == "char*")) return true;
        auto check_member_string = [&](const std::string& cls_name, const std::string& member_name) -> bool {
            std::vector<std::string> worklist = {cls_name};
            while (!worklist.empty()) {
                std::string cur = worklist.back();
                worklist.pop_back();
                auto it2 = m_class_fields.find(cur);
                if (it2 != m_class_fields.end()) {
                    for (auto& f : it2->second) {
                        if (f.first == member_name && f.second == "const char*") return true;
                    }
                }
                auto it3 = m_class_parent.find(cur);
                if (it3 != m_class_parent.end()) {
                    for (auto& p : it3->second) worklist.push_back(p);
                }
            }
            return false;
        };
        if (mem->object->type == NodeType::IDENT_EXPR) {
            auto* ident = static_cast<IdentExpr*>(mem->object.get());
            auto it = m_var_class.find(ident->name);
            if (it != m_var_class.end()) {
                if (check_member_string(it->second, mem->member)) return true;
            }
            if (ident->name == "self" && !m_current_class.empty()) {
                if (check_member_string(m_current_class, mem->member)) return true;
            }
        } else if (mem->object->type == NodeType::MEMBER_EXPR) {
            auto* inner = static_cast<MemberExpr*>(mem->object.get());
            if (inner->object->type == NodeType::IDENT_EXPR && inner->member.substr(0, 9) == "__parent_") {
                std::string parent_cls = inner->member.substr(9);
                if (check_member_string(parent_cls, mem->member)) return true;
            }
        }
    }
    if (expr->type == NodeType::CALL_EXPR) {
        auto* c = static_cast<CallExpr*>(expr);
        if (c->callee == "read_file") return true;
        if (c->callee == "input") return true;
        if (c->callee == "chr" && c->args.size() == 1) return true;
        if (c->callee == "substr" && c->args.size() == 3) return true;
        if (c->callee == "str" && c->args.size() == 1) return true;
        if (c->callee == "type" && c->args.size() == 1) return true;
    }
    if (expr->type == NodeType::METHOD_CALL_EXPR) {
        auto* m = static_cast<MethodCallExpr*>(expr);
        if (m->object->type == NodeType::IDENT_EXPR) {
            auto* id = static_cast<IdentExpr*>(m->object.get());
            if (m_string_vars.count(id->name)) {
                if (m->method == "upper" || m->method == "lower" || m->method == "strip" ||
                    m->method == "replace" || m->method == "join") return true;
            }
        }
    }
    if (expr->type == NodeType::TERNARY_EXPR) {
        auto* t = static_cast<TernaryExpr*>(expr);
        return is_string_expr(t->then_expr.get()) || is_string_expr(t->else_expr.get());
    }
    return false;
}

std::string Codegen::get_binop_struct_class(ASTNode* expr) {
    if (!expr || expr->type != NodeType::BINOP_EXPR) return "";
    auto* e = static_cast<BinOpExpr*>(expr);
    std::string op_method;
    if (e->op == "+") op_method = "__add__";
    else if (e->op == "-") op_method = "__sub__";
    else if (e->op == "*") op_method = "__mul__";
    else if (e->op == "/") op_method = "__div__";
    else return "";

    std::string left_cls = get_expr_class(e->left.get());
    if (!left_cls.empty()) {
        auto& methods = m_class_methods[left_cls];
        if (methods.count(op_method)) {
            auto& rs = m_class_dunder_returns_struct[left_cls];
            if (rs.count(op_method)) return left_cls;
        }
    }
    std::string right_cls = get_expr_class(e->right.get());
    if (!right_cls.empty()) {
        auto& methods = m_class_methods[right_cls];
        if (methods.count(op_method)) {
            auto& rs = m_class_dunder_returns_struct[right_cls];
            if (rs.count(op_method)) return right_cls;
        }
    }
    return "";
}

std::string Codegen::get_var_c_type(const std::string& name) {
    if (m_string_vars.count(name)) return "const char*";
    if (m_dict_vars.count(name) || m_named_tuple_vars.count(name)) return "SpyDict";
    if (m_set_vars.count(name)) return "SpySet";
    if (m_list_vars.count(name)) return "SpyList";
    if (m_var_class.count(name)) return m_var_class[name];
    if (m_var_types.count(name)) {
        auto t = m_var_types[name];
        if (t == "const char*" || t == "char*") return "const char*";
        if (t == "SpyDict") return "SpyDict";
        if (t == "SpySet") return "SpySet";
    }
    return "double";
}

void Codegen::find_captures_in_node(ASTNode* node, const std::set<std::string>& params, const std::set<std::string>& locals, std::vector<CapturedVar>& captures) {
    if (!node) return;
    if (node->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(node);
        if (!params.count(id->name) && !locals.count(id->name) && id->name != "true" && id->name != "false" && id->name != "none" && id->name != "None"
            && id->name != "print" && id->name != "str" && id->name != "int" && id->name != "float" && id->name != "len"
            && id->name != "input" && id->name != "abs" && id->name != "round" && id->name != "min" && id->name != "max"
            && id->name != "sum" && id->name != "range" && id->name != "type" && id->name != "is_none" && id->name != "or_default"
            && !m_all_fns.count(id->name) && id->name != "set" && id->name != "list" && id->name != "map" && id->name != "filter" && id->name != "sorted"
            && id->name != "exit" && id->name != "exec" && id->name != "bool" && id->name != "chr" && id->name != "substr" && id->name != "read_file" && id->name != "write_file"
            && id->name != "file_exists" && id->name != "file_size" && id->name != "mkdir" && id->name != "rm" && id->name != "rename_file") {
            for (auto& c : captures) if (c.name == id->name) return;
            std::string ct = get_var_c_type(id->name);
            captures.push_back({id->name, ct});
        }
        return;
    }
    if (node->type == NodeType::LET_STMT) {
        auto* let = static_cast<LetStmt*>(node);
        std::set<std::string> inner_locals = locals;
        inner_locals.insert(let->name);
        if (let->initializer) find_captures_in_node(let->initializer.get(), params, inner_locals, captures);
        if (let->type_annotation) find_captures_in_node(let->type_annotation.get(), params, inner_locals, captures);
        return;
    }
    if (node->type == NodeType::BINOP_EXPR) {
        auto* b = static_cast<BinOpExpr*>(node);
        find_captures_in_node(b->left.get(), params, locals, captures);
        find_captures_in_node(b->right.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::CALL_EXPR) {
        auto* c = static_cast<CallExpr*>(node);
        for (auto& a : c->args) find_captures_in_node(a.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::UNARY_EXPR) {
        auto* u = static_cast<UnaryExpr*>(node);
        find_captures_in_node(u->operand.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::INDEX_EXPR) {
        auto* ix = static_cast<IndexExpr*>(node);
        find_captures_in_node(ix->object.get(), params, locals, captures);
        find_captures_in_node(ix->index.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::MEMBER_EXPR) {
        auto* m = static_cast<MemberExpr*>(node);
        find_captures_in_node(m->object.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::ARRAY_EXPR) {
        auto* a = static_cast<ArrayExpr*>(node);
        for (auto& e : a->elements) find_captures_in_node(e.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::DICT_EXPR) {
        auto* d = static_cast<DictExpr*>(node);
        for (auto& k : d->keys) find_captures_in_node(k.get(), params, locals, captures);
        for (auto& v : d->values) find_captures_in_node(v.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::TERNARY_EXPR) {
        auto* t = static_cast<TernaryExpr*>(node);
        find_captures_in_node(t->condition.get(), params, locals, captures);
        find_captures_in_node(t->then_expr.get(), params, locals, captures);
        find_captures_in_node(t->else_expr.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::LAMBDA_EXPR) {
        // Don't recurse into nested lambdas — their captures are their own responsibility
        return;
    }
    if (node->type == NodeType::RETURN_STMT) {
        find_captures_in_node(static_cast<ReturnStmt*>(node)->expression.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::EXPR_STMT) {
        find_captures_in_node(static_cast<ExprStmt*>(node)->expression.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::IF_STMT) {
        auto* i = static_cast<IfStmt*>(node);
        find_captures_in_node(i->condition.get(), params, locals, captures);
        for (auto& s : i->then_body) find_captures_in_node(s.get(), params, locals, captures);
        for (auto& s : i->else_body) find_captures_in_node(s.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::WHILE_STMT) {
        auto* w = static_cast<WhileStmt*>(node);
        find_captures_in_node(w->condition.get(), params, locals, captures);
        for (auto& s : w->body) find_captures_in_node(s.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::FOR_STMT) {
        auto* f = static_cast<ForStmt*>(node);
        std::set<std::string> inner = locals;
        inner.insert(f->var);
        find_captures_in_node(f->iterable.get(), params, inner, captures);
        for (auto& s : f->body) find_captures_in_node(s.get(), params, inner, captures);
        return;
    }
    if (node->type == NodeType::ASSIGN_STMT) {
        auto* a = static_cast<AssignStmt*>(node);
        find_captures_in_node(a->target.get(), params, locals, captures);
        find_captures_in_node(a->value.get(), params, locals, captures);
        return;
    }
    if (node->type == NodeType::MATCH_STMT) {
        auto* m = static_cast<MatchStmt*>(node);
        find_captures_in_node(m->value.get(), params, locals, captures);
        for (auto& c : m->cases) {
            for (auto& s : c.body) find_captures_in_node(s.get(), params, locals, captures);
        }
        return;
    }
}

std::vector<CapturedVar> Codegen::find_captures(LambdaExpr* lambda, ASTNode* scope) {
    (void)scope;
    std::set<std::string> params;
    for (auto& p : lambda->params) params.insert(p);
    std::set<std::string> locals = params;
    std::vector<CapturedVar> captures;
    for (auto& stmt : lambda->body) {
        find_captures_in_node(stmt.get(), params, locals, captures);
    }
    return captures;
}

void Codegen::scan_lambdas(ASTNode* stmt) {
    if (!stmt) return;
    if (stmt->type == NodeType::LET_STMT) {
        auto* let = static_cast<LetStmt*>(stmt);
        if (let->initializer && let->initializer->type == NodeType::LAMBDA_EXPR) {
            auto* e = static_cast<LambdaExpr*>(let->initializer.get());
            LambdaInfo li;
            li.expr = e;
            li.name = let->name;
            li.captures = find_captures(e, let);
            m_lambdas.push_back(li);
            m_lambda_counter++;
        } else {
            collect_lambdas(let->initializer.get());
        }
    } else if (stmt->type == NodeType::IF_STMT) {
        auto* i = static_cast<IfStmt*>(stmt);
        for (auto& s : i->then_body) scan_lambdas(s.get());
        for (auto& s : i->else_body) scan_lambdas(s.get());
    } else if (stmt->type == NodeType::WHILE_STMT) {
        auto* w = static_cast<WhileStmt*>(stmt);
        for (auto& s : w->body) scan_lambdas(s.get());
    } else if (stmt->type == NodeType::FOR_STMT) {
        auto* f = static_cast<ForStmt*>(stmt);
        for (auto& s : f->body) scan_lambdas(s.get());
    } else if (stmt->type == NodeType::EXPR_STMT) {
        auto* es = static_cast<ExprStmt*>(stmt);
        collect_lambdas(es->expression.get());
    } else if (stmt->type == NodeType::ASSIGN_STMT) {
        auto* a = static_cast<AssignStmt*>(stmt);
        collect_lambdas(a->target.get());
        collect_lambdas(a->value.get());
    } else if (stmt->type == NodeType::RETURN_STMT) {
        auto* r = static_cast<ReturnStmt*>(stmt);
        collect_lambdas(r->expression.get());
    } else if (stmt->type == NodeType::MATCH_STMT) {
        auto* m = static_cast<MatchStmt*>(stmt);
        collect_lambdas(m->value.get());
        for (auto& c : m->cases) {
            for (auto& s : c.body) scan_lambdas(s.get());
        }
    }
}

void Codegen::collect_lambdas(ASTNode* node) {
    if (!node) return;
    if (node->type == NodeType::LAMBDA_EXPR) {
        auto* e = static_cast<LambdaExpr*>(node);
        std::string name = "__lambda_" + std::to_string(m_lambda_counter++);
        LambdaInfo li;
        li.name = name;
        li.expr = e;
        li.captures = find_captures(e, nullptr);
        m_lambdas.push_back(li);
    } else if (node->type == NodeType::LET_STMT) {
        auto* let = static_cast<LetStmt*>(node);
        if (let->initializer && let->initializer->type == NodeType::LAMBDA_EXPR) {
            auto* e = static_cast<LambdaExpr*>(let->initializer.get());
            LambdaInfo li;
            li.name = let->name;
            li.expr = e;
            m_lambdas.push_back(li);
        } else {
            collect_lambdas(let->initializer.get());
        }
    } else if (node->type == NodeType::BINOP_EXPR) {
        auto* bin = static_cast<BinOpExpr*>(node);
        collect_lambdas(bin->left.get());
        collect_lambdas(bin->right.get());
    } else if (node->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node);
        for (auto& arg : call->args) collect_lambdas(arg.get());
    } else if (node->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExpr*>(node);
        collect_lambdas(idx->object.get());
        collect_lambdas(idx->index.get());
    } else if (node->type == NodeType::DICT_EXPR) {
        auto* d = static_cast<DictExpr*>(node);
        for (auto& k : d->keys) collect_lambdas(k.get());
        for (auto& v : d->values) collect_lambdas(v.get());
    }
}

std::string Codegen::get_expr_class(ASTNode* expr) {
    if (!expr) return "";
    if (expr->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(expr);
        auto it = m_var_class.find(id->name);
        if (it != m_var_class.end()) return it->second;
    }
    return "";
}

std::string Codegen::emit_expression(ASTNode* expr) {
    switch (expr->type) {
        case NodeType::INT_EXPR: {
            auto* e = static_cast<IntExpr*>(expr);
            return std::to_string(e->value);
        }
        case NodeType::FLOAT_EXPR: {
            auto* e = static_cast<FloatExpr*>(expr);
            return std::to_string(e->value);
        }
        case NodeType::STRING_EXPR: {
            auto* e = static_cast<StringExpr*>(expr);
            // Check for string interpolation: "Hello ${name}"
            if (e->value.find("${") != std::string::npos) {
                std::string result;
                size_t pos = 0;
                bool first = true;
                while (pos < e->value.size()) {
                    size_t dollar = e->value.find("${", pos);
                    if (dollar == std::string::npos) {
                        // Rest is literal
                        std::string literal = e->value.substr(pos);
                        if (!literal.empty()) {
                            if (first) { result = "\"" + escape_string(literal) + "\""; first = false; }
                            else result = "spy_strcat(" + result + ", \"" + escape_string(literal) + "\")";
                        }
                        break;
                    }
                    // Literal part before ${
                    std::string literal = e->value.substr(pos, dollar - pos);
                    if (!literal.empty()) {
                        if (first) { result = "\"" + escape_string(literal) + "\""; first = false; }
                        else result = "spy_strcat(" + result + ", \"" + escape_string(literal) + "\")";
                    }
                    // Find closing }
                    size_t close = e->value.find("}", dollar + 2);
                    if (close == std::string::npos) {
                        // No closing }, treat as literal
                        std::string rest = e->value.substr(dollar);
                        if (first) { result = "\"" + escape_string(rest) + "\""; first = false; }
                        else result = "spy_strcat(" + result + ", \"" + escape_string(rest) + "\")";
                        break;
                    }
                    std::string expr_str = e->value.substr(dollar + 2, close - dollar - 2);
                    // Parse the inner expression using a sub-lexer and sub-parser
                    Lexer subLexer(expr_str, "<interpolation>");
                    auto subTokens = subLexer.tokenize();
                    Parser subParser(subTokens);
                    auto subAst = subParser.parse_expression();
                    std::string exprCode = emit_expression(subAst.get());
                    // Check if it's a string variable
                    bool is_str = is_string_expr(subAst.get());
                    std::string part;
                    if (is_str) part = exprCode;
                    else part = "spy_format(\"%g\", " + exprCode + ")";
                    if (first) { result = part; first = false; }
                    else result = "spy_strcat(" + result + ", " + part + ")";
                    pos = close + 1;
                }
                return result;
            }
            std::string escaped;
            for (char c : e->value) {
                if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '"') escaped += "\\\"";
                else escaped += c;
            }
            return "\"" + escaped + "\"";
        }
        case NodeType::BOOL_EXPR: {
            auto* e = static_cast<BoolExpr*>(expr);
            return e->value ? "1" : "0";
        }
        case NodeType::NONE_EXPR: {
            return "spy_none_val()";
        }
        case NodeType::IDENT_EXPR: {
            auto* e = static_cast<IdentExpr*>(expr);
            auto it = m_variant_to_enum.find(e->name);
            if (it != m_variant_to_enum.end()) {
                auto& variants = m_enum_variants[it->second];
                for (auto& v : variants) {
                    if (v.name == e->name && !v.has_payload) {
                        std::string lcname = it->second;
                        for (auto& c_ : lcname) c_ = (char)tolower(c_);
                        std::string lname = e->name;
                        for (auto& c_ : lname) c_ = (char)tolower(c_);
                        return lcname + "_" + lname + "()";
                    }
                }
                return e->name;
            }
            return e->name;
        }
        case NodeType::BINOP_EXPR: {
            auto* e = static_cast<BinOpExpr*>(expr);
            std::string left = emit_expression(e->left.get());
            std::string right = emit_expression(e->right.get());

            std::string op_method;
            if (e->op == "+") op_method = "__add__";
            else if (e->op == "-") op_method = "__sub__";
            else if (e->op == "*") op_method = "__mul__";
            else if (e->op == "/") op_method = "__div__";
            else if (e->op == "==") op_method = "__eq__";
            else if (e->op == "!=") op_method = "__ne__";
            else if (e->op == "<") op_method = "__lt__";
            else if (e->op == ">") op_method = "__gt__";
            else if (e->op == "<=") op_method = "__le__";
            else if (e->op == ">=") op_method = "__ge__";

            if (!op_method.empty()) {
                std::string left_cls = get_expr_class(e->left.get());
                bool needs_struct_cast = (op_method == "__add__" || op_method == "__sub__" ||
                    op_method == "__eq__" || op_method == "__ne__" || op_method == "__lt__" || op_method == "__gt__" ||
                    op_method == "__le__" || op_method == "__ge__");
                if (!left_cls.empty()) {
                    auto& methods = m_class_methods[left_cls];
                    if (methods.count(op_method)) {
                        std::string prefix = left_cls + "_" + op_method;
                        std::string right_arg = right;
                        if (needs_struct_cast && right != "NULL") {
                            right_arg = "(struct " + left_cls + "*)&(" + right + ")";
                        }
                        return prefix + "((struct " + left_cls + "*)&(" + left + "), " + right_arg + ")";
                    }
                }
                std::string right_cls = get_expr_class(e->right.get());
                if (!right_cls.empty()) {
                    auto& methods = m_class_methods[right_cls];
                    if (methods.count(op_method)) {
                        std::string prefix = right_cls + "_" + op_method;
                        std::string left_arg = left;
                        if (needs_struct_cast && left != "NULL") {
                            left_arg = "(struct " + right_cls + "*)&(" + left + ")";
                        }
                        return prefix + "(" + left_arg + ", (struct " + right_cls + "*)&(" + right + "))";
                    }
                }
            }

            if (e->op == "+") {
                if (is_string_expr(e->left.get()) || is_string_expr(e->right.get())) {
                    if (e->left->type == NodeType::STRING_EXPR && e->right->type == NodeType::STRING_EXPR) {
                        auto* ls = static_cast<StringExpr*>(e->left.get());
                        auto* rs = static_cast<StringExpr*>(e->right.get());
                        return "\"" + ls->value + rs->value + "\"";
                    }
                    return "spy_strcat(" + left + ", " + right + ")";
                }
                return "(" + left + " + " + right + ")";
            } else if (e->op == "-") {
                return "(" + left + " - " + right + ")";
            } else if (e->op == "*") {
                return "(" + left + " * " + right + ")";
            } else if (e->op == "/") {
                return "(" + left + " / " + right + ")";
            } else if (e->op == "%") {
                if (is_string_expr(e->left.get())) {
                    std::string fmt_str;
                    if (e->left->type == NodeType::STRING_EXPR) {
                        fmt_str = static_cast<StringExpr*>(e->left.get())->value;
                    }
                    std::string fmt = emit_expression(e->left.get());
                    std::vector<bool> arg_needs_int;
                    for (size_t fi = 0; fi < fmt_str.size(); ++fi) {
                        if (fmt_str[fi] == '%') {
                            if (fi + 1 < fmt_str.size() && fmt_str[fi + 1] == '%') { fi++; continue; }
                            bool needs_int = false;
                            size_t si = fi + 1;
                            while (si < fmt_str.size() && (fmt_str[si] == '-' || fmt_str[si] == '0' || fmt_str[si] == '+' || fmt_str[si] == ' ' || fmt_str[si] == '#' || (fmt_str[si] >= '0' && fmt_str[si] <= '9') || fmt_str[si] == '.')) si++;
                            if (si < fmt_str.size()) {
                                if (fmt_str[si] == 'd' || fmt_str[si] == 'i') needs_int = true;
                            }
                            arg_needs_int.push_back(needs_int);
                            fi = si;
                        }
                    }
                    if (e->right->type == NodeType::ARRAY_EXPR) {
                        auto* arr = static_cast<ArrayExpr*>(e->right.get());
            std::string args;
                        for (size_t i = 0; i < arr->elements.size(); ++i) {
                            if (i > 0) args += ", ";
                            std::string a = emit_expression(arr->elements[i].get());
                            if (i < arg_needs_int.size() && arg_needs_int[i]) a = "(int)" + a;
                            args += a;
                        }
                        return "spy_format(" + fmt + ", " + args + ")";
                    }
                    std::string r = emit_expression(e->right.get());
                    if (!arg_needs_int.empty() && arg_needs_int[0]) r = "(int)" + r;
                    return "spy_format(" + fmt + ", " + r + ")";
                }
                return "fmod(" + left + ", " + right + ")";
            } else if (e->op == "is") {
                return "(double)((" + left + ") == (" + right + "))";
            } else if (e->op == "in") {
                if (is_string_expr(e->right.get())) {
                    return "(double)spy_str_contains(" + right + ", " + left + ")";
                }
                return "0";
            } else {
                std::string cop = e->op;
                if (cop == "and") cop = "&&";
                else if (cop == "or") cop = "||";
                else if (cop == "not") cop = "!";
                else if (cop == "&") { return "(double)((int)(" + left + ") & (int)(" + right + "))"; }
                else if (cop == "|") { return "(double)((int)(" + left + ") | (int)(" + right + "))"; }
                else if (cop == "^") { return "(double)((int)(" + left + ") ^ (int)(" + right + "))"; }
                else if (cop == "<<") { return "(double)((int)(" + left + ") << (int)(" + right + "))"; }
                else if (cop == ">>") { return "(double)((int)(" + left + ") >> (int)(" + right + "))"; }
                if ((cop == "==" || cop == "!=") && (is_string_expr(e->left.get()) || is_string_expr(e->right.get()))) {
                    std::string cmp = "strcmp(" + left + ", " + right + ") == 0";
                    if (cop == "!=") cmp = "strcmp(" + left + ", " + right + ") != 0";
                    return "(double)(" + cmp + ")";
                }
                return left + " " + cop + " " + right;
            }
        }
        case NodeType::UNARY_EXPR: {
            auto* e = static_cast<UnaryExpr*>(expr);
            std::string operand = emit_expression(e->operand.get());
            if (e->op == "not") return "(double)(!(" + operand + "))";
            if (e->op == "-") return "(-(" + operand + "))";
            if (e->op == "~") return "(double)(~(int)(" + operand + "))";
            return "(" + e->op + "(" + operand + "))";
        }
        case NodeType::ADDRESS_OF_EXPR: {
            auto* e = static_cast<AddressOfExpr*>(expr);
            return "(&(" + emit_expression(e->operand.get()) + "))";
        }
        case NodeType::DEREF_EXPR: {
            auto* e = static_cast<DerefExpr*>(expr);
            return "(*(" + emit_expression(e->operand.get()) + "))";
        }
        case NodeType::SIZEOF_EXPR: {
            auto* e = static_cast<SizeofExpr*>(expr);
            return "(sizeof(" + type_to_c(e->type.get()) + "))";
        }
        case NodeType::CALL_EXPR: {
            auto* e = static_cast<CallExpr*>(expr);
            if (e->callee == "len" && e->args.size() == 1) {
                std::string arg = emit_expression(e->args[0].get());
                if (e->args[0]->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(e->args[0].get());
                    if (m_dict_vars.count(id->name)) {
                        return arg + ".size";
                    }
                    if (m_set_vars.count(id->name)) {
                        return "(double)" + arg + ".size";
                    }
                    if (m_list_vars.count(id->name)) {
                        return "(double)" + arg + ".size";
                    }
                    if (m_generator_fns.count(id->name)) {
                        return "(double)__spy_gen_len";
                    }
                }
                if (e->args[0]->type == NodeType::CALL_EXPR) {
                    auto* call = static_cast<CallExpr*>(e->args[0].get());
                    if (m_generator_fns.count(call->callee)) {
                        return "(double)__spy_gen_len";
                    }
                    if (call->callee == "set") {
                        std::string s = emit_expression(e->args[0].get());
                        return "(double)" + s + ".size";
                    }
                    if (call->callee == "list") {
                        std::string s = emit_expression(e->args[0].get());
                        return "(double)" + s + ".size";
                    }
                }
                if (e->args[0]->type == NodeType::MEMBER_EXPR) {
                    auto* mem = static_cast<MemberExpr*>(e->args[0].get());
                    if (mem->object->type == NodeType::IDENT_EXPR) {
                        auto* id = static_cast<IdentExpr*>(mem->object.get());
                        if (id->name == "sys" && mem->member == "argv") {
                            return "(double)__global_argc";
                        }
                    }
                }
                if (is_string_expr(e->args[0].get())) {
                    return "(long long)strlen(" + arg + ")";
                }
                return "(sizeof(" + arg + ") / sizeof(" + arg + "[0]))";
            }
            if (e->callee == "matrix_print" && e->args.size() == 1) {
                std::string arg = emit_expression(e->args[0].get());
                std::string rows = "(sizeof(" + arg + ") / sizeof(" + arg + "[0]))";
                std::string cols = "(sizeof(" + arg + "[0]) / sizeof(" + arg + "[0][0]))";
                return "spy_matrix_print(" + rows + ", " + cols + ", " + arg + ")";
            }
            if (e->callee == "read_file" && e->args.size() == 1) {
                return "spy_read_file(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "write_file" && e->args.size() == 2) {
                return "spy_write_file(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "exec" && e->args.size() == 1) {
                return "spy_exec(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "file_exists" && e->args.size() == 1) {
                return "spy_file_exists(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "file_size" && e->args.size() == 1) {
                return "spy_file_size(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "mkdir" && e->args.size() == 1) {
                return "spy_mkdir(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "rm" && e->args.size() == 1) {
                return "spy_remove_file(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "rename_file" && e->args.size() == 2) {
                return "spy_rename_file(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "get_cwd" && e->args.size() == 0) {
                return "spy_get_cwd()";
            }
            if (e->callee == "chdir" && e->args.size() == 1) {
                return "spy_chdir(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "cp" && e->args.size() == 2) {
                std::string src = emit_expression(e->args[0].get());
                std::string dst = emit_expression(e->args[1].get());
                return "spy_write_file(" + dst + ", spy_read_file(" + src + "))";
            }
            if (e->callee == "error" && e->args.size() == 1) {
                return "spy_error(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "str" && e->args.size() == 1) {
                return "spy_format(\"%g\", (double)(" + emit_expression(e->args[0].get()) + "))";
            }
            if (e->callee == "type" && e->args.size() == 1) {
                auto* arg0 = e->args[0].get();
                if (is_string_expr(arg0)) return "\"string\"";
                if (arg0->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(arg0);
                    if (m_string_vars.count(id->name)) return "\"string\"";
                    if (m_dict_vars.count(id->name)) return "\"dict\"";
                    if (m_var_class.count(id->name)) return "\"" + m_var_class[id->name] + "\"";
                }
                return "\"number\"";
            }
            if (e->callee == "int" && e->args.size() == 1) {
                if (is_string_expr(e->args[0].get())) {
                    return "(double)(int)(atof(" + emit_expression(e->args[0].get()) + "))";
                }
                return "(double)(int)(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "bool" && e->args.size() == 1) {
                auto* arg0 = e->args[0].get();
                if (is_string_expr(arg0)) {
                    return "(double)(strlen(" + emit_expression(arg0) + ") > 0 ? 1.0 : 0.0)";
                }
                if (arg0->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(arg0);
                    if (m_list_vars.count(id->name) || m_set_vars.count(id->name))
                        return "(double)((" + id->name + ".size) > 0 ? 1.0 : 0.0)";
                    if (m_dict_vars.count(id->name))
                        return "(double)((" + id->name + ".size) > 0 ? 1.0 : 0.0)";
                }
                return "(double)((" + emit_expression(e->args[0].get()) + ") != 0.0 ? 1.0 : 0.0)";
            }
            if (e->callee == "chr" && e->args.size() == 1) {
                return "spy_chr((int)(" + emit_expression(e->args[0].get()) + "))";
            }
            if (e->callee == "substr" && e->args.size() == 3) {
                return "spy_substr(" + emit_expression(e->args[0].get()) + ", (int)(" + emit_expression(e->args[1].get()) + "), (int)(" + emit_expression(e->args[2].get()) + "))";
            }
            if (e->callee == "fprintf" && e->args.size() >= 2) {
                std::string args;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    if (i > 0) args += ", ";
                    if (i == 0 && e->args[0]->type == NodeType::STRING_EXPR) {
                        auto* s = static_cast<StringExpr*>(e->args[0].get());
                        if (s->value == "stderr") {
                            args += "stderr";
                            continue;
                        }
                    }
                    args += emit_expression(e->args[i].get());
                }
                return "fprintf(" + args + ")";
            }
            if (e->callee == "exit" && e->args.size() == 1) {
                return "exit((int)(" + emit_expression(e->args[0].get()) + "))";
            }
            if (e->callee == "input" && e->args.size() <= 1) {
                if (e->args.size() == 1) {
                    return "spy_input(" + emit_expression(e->args[0].get()) + ")";
                }
                return "spy_input(\"\")";
            }
            if (e->callee == "abs" && e->args.size() == 1) {
                return "fabs(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "round" && e->args.size() == 1) {
                return "round(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "min" && e->args.size() == 2) {
                return "spy_min(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "max" && e->args.size() == 2) {
                return "spy_max(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "sum" && e->args.size() == 1) {
                std::string arr = emit_expression(e->args[0].get());
                return "spy_sum(" + arr + ", (int)(sizeof(" + arr + ") / sizeof(" + arr + "[0])))";
            }
            if (e->callee == "map" && e->args.size() == 2) {
                std::string fn = emit_expression(e->args[0].get());
                std::string arr = emit_expression(e->args[1].get());
                return "spy_map(" + fn + ", " + arr + ", (int)(sizeof(" + arr + ") / sizeof(" + arr + "[0])))";
            }
            if (e->callee == "filter" && e->args.size() == 2) {
                std::string fn = emit_expression(e->args[0].get());
                std::string arr = emit_expression(e->args[1].get());
                return "spy_filter(" + fn + ", " + arr + ", (int)(sizeof(" + arr + ") / sizeof(" + arr + "[0])), &__spy_filter_len)";
            }
            if (e->callee == "sorted" && e->args.size() == 1) {
                std::string arr = emit_expression(e->args[0].get());
                return "spy_sorted_copy(" + arr + ", (int)(sizeof(" + arr + ") / sizeof(" + arr + "[0])))";
            }
            if (e->callee == "set") {
                std::string result = "spy_set_new()";
                for (size_t i = 0; i < e->args.size(); ++i) {
                    result = "({ SpySet __s = " + result + "; spy_set_add(&__s, " + emit_expression(e->args[i].get()) + "); __s; })";
                }
                return result;
            }
            if (e->callee == "list") {
                std::string result = "spy_list_new()";
                for (size_t i = 0; i < e->args.size(); ++i) {
                    result = "({ SpyList __l = " + result + "; spy_list_push(&__l, " + emit_expression(e->args[i].get()) + "); __l; })";
                }
                return result;
            }
            if (e->callee == "is_none" && e->args.size() == 1) {
                return "spy_is_none(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "or_default" && e->args.size() == 2) {
                return "(spy_is_none(" + emit_expression(e->args[0].get()) + ") ? " + emit_expression(e->args[1].get()) + " : " + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "set_add" && e->args.size() == 2) {
                return "spy_set_add(&" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "set_contains" && e->args.size() == 2) {
                return "spy_set_contains(&" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "set_remove" && e->args.size() == 2) {
                return "spy_set_remove(&" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "alloc" && e->args.size() == 2) {
                std::string count = emit_expression(e->args[0].get());
                std::string ctype = type_to_c(e->args[1].get());
                return "((" + ctype + "*)malloc((" + count + ") * sizeof(" + ctype + ")))";
            }
            if (e->callee == "realloc" && e->args.size() == 3) {
                std::string ptr = emit_expression(e->args[0].get());
                std::string count = emit_expression(e->args[1].get());
                std::string ctype = type_to_c(e->args[2].get());
                return "((" + ctype + "*)realloc(" + ptr + ", (" + count + ") * sizeof(" + ctype + ")))";
            }
            if (e->callee == "free" && e->args.size() == 1) {
                return "free(" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "as_void_ptr" && e->args.size() == 1) {
                return "((void*)(" + emit_expression(e->args[0].get()) + "))";
            }
            if (e->callee == "from_void_ptr" && e->args.size() == 1) {
                std::string ctype = type_to_c(e->args[0].get());
                return "((" + ctype + "*)" + emit_expression(e->args[0].get()) + ")";
            }
            if (e->callee == "from_void_ptr" && e->args.size() == 2) {
                std::string ctype = type_to_c(e->args[0].get());
                return "((" + ctype + "*)" + emit_expression(e->args[1].get()) + ")";
            }
            if (e->callee == "memcpy" && e->args.size() == 3) {
                return "memcpy(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ", " + emit_expression(e->args[2].get()) + ")";
            }
            if (e->callee == "memset" && e->args.size() == 3) {
                return "memset(" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ", " + emit_expression(e->args[2].get()) + ")";
            }
            if (e->callee == "sizeof" && e->args.size() == 1) {
                std::string ctype = type_to_c(e->args[0].get());
                return "(sizeof(" + ctype + "))";
            }
            for (auto& [mod, fns] : m_module_from_fns) {
                if (fns.count(e->callee)) {
                    std::string prefix = m_module_prefix + mod + "_" + e->callee;
                    std::string args;
                    for (size_t i = 0; i < e->args.size(); ++i) {
                        if (i > 0) args += ", ";
                        args += emit_expression(e->args[i].get());
                    }
                    auto dit = m_fn_defaults.find(prefix);
                    if (dit != m_fn_defaults.end()) {
                        auto* fn = dit->second;
                        for (size_t i = e->args.size(); i < fn->params.size(); ++i) {
                            if (!args.empty()) args += ", ";
                            auto dfit = fn->defaults.find(fn->params[i]);
                            if (dfit != fn->defaults.end() && dfit->second) {
                                args += emit_expression(dfit->second.get());
                            } else {
                                args += "0";
                            }
                        }
                    }
                    return prefix + "(" + args + ")";
                }
            }
            {
                auto ev = m_variant_to_enum.find(e->callee);
                if (ev != m_variant_to_enum.end()) {
                    auto& variants = m_enum_variants[ev->second];
                    for (auto& v : variants) {
                        if (v.name == e->callee) {
                            std::string en = ev->second;
                            std::string lcname = en;
                            for (auto& c_ : lcname) c_ = (char)tolower(c_);
                            std::string lvname = e->callee;
                            for (auto& c_ : lvname) c_ = (char)tolower(c_);
                            if (v.has_payload) {
                                std::string out = lcname + "_" + lvname + "(";
                                for (size_t i = 0; i < e->args.size(); ++i) {
                                    if (i > 0) out += ", ";
                                    out += emit_expression(e->args[i].get());
                                }
                                return out + ")";
                            } else {
                                return lcname + "_" + lvname + "()";
                            }
                        }
                    }
                }
            }
            std::string args;
            for (size_t i = 0; i < e->args.size(); ++i) {
                if (i > 0) args += ", ";
                args += emit_expression(e->args[i].get());
            }
            {
                auto dit = m_fn_defaults.find(e->callee);
                if (dit != m_fn_defaults.end()) {
                    auto* fn = dit->second;
                    for (size_t i = e->args.size(); i < fn->params.size(); ++i) {
                        if (!args.empty()) args += ", ";
                        auto dfit = fn->defaults.find(fn->params[i]);
                        if (dfit != fn->defaults.end() && dfit->second) {
                            args += emit_expression(dfit->second.get());
                        } else {
                            args += "0";
                        }
                    }
                }
            }
            if (m_structs.count(e->callee)) {
                std::string args;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    if (i > 0) args += ", ";
                    args += emit_expression(e->args[i].get());
                }
                return "(struct " + e->callee + "){" + args + "}";
            }
            if (m_classes.count(e->callee)) {
                auto pit = m_class_parent.find(e->callee);
                if (pit != m_class_parent.end()) {
                    auto& parents = pit->second;
                    auto& child_fields = m_class_fields[e->callee];
                    std::vector<std::string> parent_args_vec(parents.size());
                    std::string child_args;
                    for (size_t i = 0; i < e->args.size() && i < child_fields.size(); ++i) {
                        std::string arg_val;
                        if (e->args[i]->type == NodeType::STRING_EXPR) {
                            arg_val = "(const char*)" + emit_expression(e->args[i].get());
                        } else {
                            arg_val = emit_expression(e->args[i].get());
                        }
                        bool assigned = false;
                        for (size_t pi = 0; pi < parents.size(); ++pi) {
                            auto& pf = m_class_fields[parents[pi]];
                            for (auto& pfield : pf) {
                                if (child_fields[i].first == pfield.first) {
                                    if (!parent_args_vec[pi].empty()) parent_args_vec[pi] += ", ";
                                    parent_args_vec[pi] += arg_val;
                                    assigned = true;
                                    break;
                                }
                            }
                            if (assigned) break;
                        }
                        if (!assigned) {
                            if (!child_args.empty()) child_args += ", ";
                            child_args += arg_val;
                        }
                    }
                    std::string result = "(struct " + e->callee + "){";
                    for (size_t pi = 0; pi < parents.size(); ++pi) {
                        if (pi > 0) result += ", ";
                        result += "{" + parent_args_vec[pi] + "}";
                    }
                    if (!child_args.empty()) {
                        if (!parents.empty()) result += ", ";
                        result += child_args;
                    }
                    result += "}";
                    return result;
                }
                std::string args;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    if (i > 0) args += ", ";
                    if (e->args[i]->type == NodeType::STRING_EXPR) {
                        args += "(const char*)" + emit_expression(e->args[i].get());
                    } else {
                        args += emit_expression(e->args[i].get());
                    }
                }
                return "(struct " + e->callee + "){" + args + "}";
            }
            auto vit = m_variant_to_enum.find(e->callee);
            if (vit != m_variant_to_enum.end()) {
                auto& variants = m_enum_variants[vit->second];
                for (auto& v : variants) {
                    if (v.name == e->callee && v.has_payload) {
                        std::string en = vit->second;
                        std::string lcname = en;
                        for (auto& c_ : lcname) c_ = (char)tolower(c_);
                        std::string lvname = v.name;
                        for (auto& c_ : lvname) c_ = (char)tolower(c_);
                        return lcname + "_" + lvname + "(" + args + ")";
                    }
                }
            }
            if (!m_module_prefix.empty() && m_declared.find(e->callee) == m_declared.end()) {
                return m_module_prefix + e->callee + "(" + args + ")";
            }
            if (m_closure_vars.count(e->callee)) {
                return e->callee + ".fn(&" + e->callee + ", " + args + ")";
            }
            return e->callee + "(" + args + ")";
        }
        case NodeType::ARRAY_EXPR: {
            auto* e = static_cast<ArrayExpr*>(expr);
            bool is_2d = !e->elements.empty() && e->elements[0]->type == NodeType::ARRAY_EXPR;
            std::string items;
            for (size_t i = 0; i < e->elements.size(); ++i) {
                if (i > 0) items += ", ";
                if (is_2d) {
                    auto* row = static_cast<ArrayExpr*>(e->elements[i].get());
                    std::string row_items;
                    for (size_t j = 0; j < row->elements.size(); ++j) {
                        if (j > 0) row_items += ", ";
                        row_items += emit_expression(row->elements[j].get());
                    }
                    items += "{" + row_items + "}";
                } else {
                    items += emit_expression(e->elements[i].get());
                }
            }
            return "{" + items + "}";
        }
        case NodeType::DICT_EXPR: {
            auto* e = static_cast<DictExpr*>(expr);
            std::string var = "__dict_" + std::to_string(expr->line) + "_" + std::to_string(expr->column);
            std::string result = "(spy_dict_new())";
            for (size_t i = 0; i < e->keys.size(); ++i) {
                std::string key_str;
                if (e->keys[i]->type == NodeType::STRING_EXPR) {
                    key_str = static_cast<StringExpr*>(e->keys[i].get())->value;
                } else {
                    key_str = "__nonstring_key";
                }
                result = "({ SpyDict __d = " + result + "; spy_dict_set(&__d, \"" + key_str + "\", " + emit_expression(e->values[i].get()) + "); __d; })";
            }
            return result;
        }
        case NodeType::SET_EXPR: {
            auto* e = static_cast<SetExpr*>(expr);
            std::string result = "(spy_set_new())";
            for (size_t i = 0; i < e->elements.size(); ++i) {
                result = "({ SpySet __s = " + result + "; spy_set_add(&__s, " + emit_expression(e->elements[i].get()) + "); __s; })";
            }
            return result;
        }
        case NodeType::NAMED_TUPLE_EXPR: {
            auto* e = static_cast<NamedTupleExpr*>(expr);
            std::string result = "(spy_dict_new())";
            for (size_t i = 0; i < e->field_names.size(); ++i) {
                result = "({ SpyDict __d = " + result + "; spy_dict_set(&__d, \"" + e->field_names[i] + "\", " + emit_expression(e->field_values[i].get()) + "); __d; })";
            }
            return result;
        }
        case NodeType::DICT_ACCESS_EXPR: {
            auto* e = static_cast<DictAccessExpr*>(expr);
            return "spy_dict_get(" + emit_expression(e->object.get()) + ", " + emit_expression(e->key.get()) + ")";
        }
        case NodeType::INDEX_EXPR: {
            auto* e = static_cast<IndexExpr*>(expr);
            if (e->object->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(e->object.get());
                if (m_dict_vars.count(id->name)) {
                    return "spy_dict_get(" + id->name + ", " + emit_expression(e->index.get()) + ")";
                }
                if (m_list_vars.count(id->name)) {
                    return "spy_list_get(&" + id->name + ", (int)" + emit_expression(e->index.get()) + ")";
                }
            }
            if (e->object->type == NodeType::MEMBER_EXPR) {
                auto* mem = static_cast<MemberExpr*>(e->object.get());
                if (mem->object->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(mem->object.get());
                    if (id->name == "sys" && mem->member == "argv") {
                        return "__global_argv[(int)" + emit_expression(e->index.get()) + "]";
                    }
                }
            }
            return emit_expression(e->object.get()) + "[(int)" + emit_expression(e->index.get()) + "]";
        }
        case NodeType::MEMBER_EXPR: {
            auto* e = static_cast<MemberExpr*>(expr);
            if (e->member == "length" && e->object->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(e->object.get());
                if (m_string_vars.count(id->name)) {
                    return "(double)strlen(" + id->name + ")";
                }
            }
            if (e->object->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(e->object.get());
                if (id->name == "sys" && e->member == "argv") {
                    return "__global_argv";
                }
            }
            std::string obj = emit_expression(e->object.get());
            if (e->object->type == NodeType::IDENT_EXPR) {
                auto* id = static_cast<IdentExpr*>(e->object.get());
                if (m_dict_vars.count(id->name) || m_named_tuple_vars.count(id->name)) {
                    return "spy_dict_get(" + obj + ", \"" + e->member + "\")";
                }
                if (id->name == "self" && !m_current_class.empty()) {
                    auto pit = m_class_parent.find(m_current_class);
                    if (pit != m_class_parent.end()) {
                        for (auto& pname : pit->second) {
                            auto& parent_fields = m_class_fields[pname];
                            for (auto& pf : parent_fields) {
                                if (pf.first == e->member) {
                                    return "self->__parent_" + pname + "." + e->member;
                                }
                            }
                        }
                    }
                    return "self->" + e->member;
                }
                if (m_ptr_params.count(id->name) || (m_var_types.count(id->name) && !m_var_types[id->name].empty() && m_var_types[id->name].find('*') != std::string::npos)) {
                    return obj + "->" + e->member;
                }
                auto vit = m_var_class.find(id->name);
                if (vit != m_var_class.end()) {
                    auto pit = m_class_parent.find(vit->second);
                    if (pit != m_class_parent.end()) {
                        for (auto& pname : pit->second) {
                            auto& parent_fields = m_class_fields[pname];
                            for (auto& pf : parent_fields) {
                                if (pf.first == e->member) {
                                    return obj + ".__parent_" + pname + "." + e->member;
                                }
                            }
                        }
                    }
                }
            }
            return obj + "." + e->member;
        }
        case NodeType::SUPER_METHOD_CALL_EXPR: {
            auto* e = static_cast<SuperMethodCallExpr*>(expr);
            auto& parents = m_class_parent[m_current_class];
            std::string parent_name = parents.front();
            std::string prefix = parent_name + "_" + e->method;
            std::string args = "(struct " + parent_name + "*)&self->__parent_" + parent_name;
            for (size_t i = 0; i < e->args.size(); ++i) {
                args += ", " + emit_expression(e->args[i].get());
            }
            return prefix + "(" + args + ")";
        }
        case NodeType::METHOD_CALL_EXPR: {
            auto* e = static_cast<MethodCallExpr*>(expr);
            std::string obj_name;
            if (e->object->type == NodeType::IDENT_EXPR) {
                obj_name = static_cast<IdentExpr*>(e->object.get())->name;
            }
            if (m_dict_vars.count(obj_name)) {
                if (e->method == "keys") {
                    return "spy_dict_keys(" + obj_name + ")";
                } else if (e->method == "values") {
                    return "spy_dict_values(" + obj_name + ")";
                }
            }
            if (m_list_vars.count(obj_name)) {
                std::string obj = emit_expression(e->object.get());
                if (e->method == "push" && e->args.size() == 1)
                    return "spy_list_push(&" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "pop" && e->args.empty())
                    return "spy_list_pop(&" + obj + ")";
                if (e->method == "get" && e->args.size() == 1)
                    return "spy_list_get(&" + obj + ", (int)" + emit_expression(e->args[0].get()) + ")";
                if (e->method == "set" && e->args.size() == 2)
                    return "spy_list_set(&" + obj + ", (int)" + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
            }
            if (m_string_vars.count(obj_name) || is_string_expr(e->object.get())) {
                std::string obj = emit_expression(e->object.get());
                if (e->method == "upper") return "spy_str_upper(" + obj + ")";
                if (e->method == "lower") return "spy_str_lower(" + obj + ")";
                if (e->method == "strip") return "spy_str_strip(" + obj + ")";
                if (e->method == "replace" && e->args.size() == 2)
                    return "spy_str_replace(" + obj + ", " + emit_expression(e->args[0].get()) + ", " + emit_expression(e->args[1].get()) + ")";
                if (e->method == "find" && e->args.size() == 1)
                    return "spy_str_find(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "contains" && e->args.size() == 1)
                    return "spy_str_contains(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "split" && e->args.size() == 1)
                    return "spy_str_split(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "join" && e->args.size() == 1)
                    return "spy_str_join(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "startswith" && e->args.size() == 1)
                    return "spy_str_startswith(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
                if (e->method == "endswith" && e->args.size() == 1)
                    return "spy_str_endswith(" + obj + ", " + emit_expression(e->args[0].get()) + ")";
            }
            if (m_module_fns.count(obj_name) && m_module_fns[obj_name].count(e->method)) {
                std::string prefix = m_module_prefix + obj_name + "_" + e->method;
                std::string args;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    if (i > 0) args += ", ";
                    args += emit_expression(e->args[i].get());
                }
                return prefix + "(" + args + ")";
            }
            std::string obj = emit_expression(e->object.get());
            std::string class_name;
            bool is_static_call = false;
            if (m_classes.count(obj_name)) {
                class_name = obj_name;
                is_static_call = true;
            } else if (m_var_class.count(obj_name)) {
                class_name = m_var_class[obj_name];
            } else {
                class_name = obj_name;
            }
            std::string prefix = class_name + "_" + e->method;
            if (is_static_call || m_class_static_methods[class_name].count(e->method)) {
                std::string args;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    if (i > 0) args += ", ";
                    args += emit_expression(e->args[i].get());
                }
                return prefix + "(" + args + ")";
            }
            std::string args = "&" + obj;
            for (size_t i = 0; i < e->args.size(); ++i) {
                args += ", " + emit_expression(e->args[i].get());
            }
            return prefix + "(" + args + ")";
        }
        case NodeType::PIPE_EXPR: {
            auto* e = static_cast<PipeExpr*>(expr);
            std::string value = emit_expression(e->value.get());
            if (e->call->type == NodeType::CALL_EXPR) {
                auto* call = static_cast<CallExpr*>(e->call.get());
                std::string args = value;
                for (size_t i = 0; i < call->args.size(); ++i) {
                    args += ", " + emit_expression(call->args[i].get());
                }
                return call->callee + "(" + args + ")";
            } else {
                std::string callee = emit_expression(e->call.get());
                return callee + "(" + value + ")";
            }
        }
        case NodeType::LAMBDA_EXPR: {
            auto* e = static_cast<LambdaExpr*>(expr);
            for (auto& li : m_lambdas) {
                if (li.expr == e) {
                    return li.name;
                }
            }
            std::string name = "__lambda_" + std::to_string(m_lambda_counter++);
            LambdaInfo li;
            li.name = name;
            li.expr = e;
            li.captures = find_captures(e, nullptr);
            m_lambdas.push_back(li);
            return name;
        }
        case NodeType::TERNARY_EXPR: {
            auto* e = static_cast<TernaryExpr*>(expr);
            return "(" + emit_expression(e->condition.get()) + ") ? (" + emit_expression(e->then_expr.get()) + ") : (" + emit_expression(e->else_expr.get()) + ")";
        }
        default:
            return "/* unknown */";
    }
}

std::string Codegen::type_to_c(ASTNode* type_node) {
    if (!type_node) return "double";
    if (type_node->type == NodeType::TYPE_EXPR) {
        auto* t = static_cast<TypeExpr*>(type_node);
        if (t->name == "i8") return "int8_t";
        if (t->name == "i16") return "int16_t";
        if (t->name == "i32") return "int32_t";
        if (t->name == "i64") return "int64_t";
        if (t->name == "u8") return "uint8_t";
        if (t->name == "u16") return "uint16_t";
        if (t->name == "u32") return "uint32_t";
        if (t->name == "u64") return "uint64_t";
        if (t->name == "f32") return "float";
        if (t->name == "f64") return "double";
        if (t->name == "bool") return "int";
        if (t->name == "void") return "void";
        if (t->name == "char") return "char";
        if (t->name == "usize") return "size_t";
        if (t->name == "string") return "const char*";
        if (m_structs.count(t->name)) return "struct " + t->name;
        if (needs_struct_prefix(t->name)) return "struct " + t->name;
        return t->name;
    }
    if (type_node->type == NodeType::PTR_TYPE_EXPR) {
        auto* p = static_cast<PtrTypeExpr*>(type_node);
        std::string base = type_to_c(p->base_type.get());
        if (base == "char") return "const char*";
        return base + "*";
    }
    if (type_node->type == NodeType::IDENT_EXPR) {
        auto* id = static_cast<IdentExpr*>(type_node);
        if (m_structs.count(id->name)) return "struct " + id->name;
        if (needs_struct_prefix(id->name)) return "struct " + id->name;
    }
    if (type_node->type == NodeType::ARRAY_TYPE_EXPR) {
        auto* a = static_cast<ArrayTypeExpr*>(type_node);
        std::string elem = type_to_c(a->element_type.get());
        if (a->size) {
            std::string sz = emit_expression(a->size.get());
            return elem + " " + sz;
        }
        return elem + "*";
    }
    return "double";
}

void Codegen::emit_struct(StructStmt* node, std::string& out) {
    m_structs.insert(node->name);
    out += "struct " + node->name + " {\n";
    for (auto& field : node->fields) {
        std::string ctype = type_to_c(field.type.get());
        out += "    " + ctype + " " + field.name + ";\n";
        m_struct_defs[node->name].push_back({field.name, ctype});
    }
    out += "};\n\n";
}

void Codegen::emit_extern_fn(ExternFnStmt* node, std::string& out) {
    std::string ret_type = "double";
    if (node->return_type) {
        ret_type = type_to_c(node->return_type.get());
    }
    m_fn_return_types[node->name] = ret_type;
    if (!node->header.empty()) {
        out += "// extern: " + node->header + "\n";
    }
    out += ret_type + " " + node->name + "(";
    for (size_t i = 0; i < node->typed_params.size(); ++i) {
        if (i > 0) out += ", ";
        std::string param_type = "double";
        if (node->typed_params[i].type) {
            param_type = type_to_c(node->typed_params[i].type.get());
        }
        out += param_type + " " + node->typed_params[i].name;
    }
    out += ");\n";
}

std::string Codegen::get_var_type(const std::string& name) {
    auto it = m_var_types.find(name);
    if (it != m_var_types.end()) return it->second;
    return "double";
}

} // namespace spy
