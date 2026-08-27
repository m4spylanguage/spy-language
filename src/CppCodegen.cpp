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

#include "spy/CppCodegen.h"
#include "spy/Lexer.h"
#include "spy/Parser.h"
#include <sstream>
#include <algorithm>
#include <fstream>

namespace spy {

static std::string escape_cpp_string(const std::string& s) {
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

static std::string indent_str(int indent) {
    return std::string(indent * 4, ' ');
}

// Known SpyUI types
static const std::set<std::string> KNOWN_SPYUI_TYPES = {
    "SWindow", "SWidget", "SToolButton", "SInput", "SLabel", "SFrame",
    "STabs", "SListView", "SSplitter", "SHBoxLayout", "SVBoxLayout",
    "SFormLayout", "SFileSystemModel", "SStandardItemModel",
    "SModelIndex", "SDelegate", "SPainter", "SColor", "SFont", "SRect",
    "SPoint", "SSize", "SEvent", "SEventType",
    "SpyApp", "SpyObject", "SpySignal", "SpyProcess", "SpyStorage",
    "SpyPaths", "SpyDir", "SpyStyle"
};

// Value types — stack-allocated, no "new"
static const std::set<std::string> VALUE_TYPES = {
    "SColor", "SRect", "SPoint", "SSize", "SEvent"
};

// Parent class map for method inheritance
static const std::map<std::string, std::string> TYPE_PARENT = {
    {"SWindow", "SWidget"}, {"SToolButton", "SWidget"}, {"SInput", "SWidget"},
    {"SLabel", "SWidget"}, {"SFrame", "SWidget"}, {"STabs", "SWidget"},
    {"SListView", "SWidget"}, {"SSplitter", "SWidget"},
    {"SHBoxLayout", "SLayout"}, {"SVBoxLayout", "SLayout"}
};

// Map Spy method names to C++ SpyUI method names
static const std::map<std::string, std::map<std::string, std::string>> METHOD_MAP = {
    {"SWindow", {
        {"set_title", "setTitle"}, {"set_size", "setSize"}, {"set_central", "setCentral"},
        {"set_layout", "setLayout"}, {"set_menu", "setMenu"}, {"show", "show"},
        {"close", "close"}, {"run", "run"}, {"width", "width"}, {"height", "height"}
    }},
    {"SToolButton", {
        {"set_text", "setText"}, {"set_size", "setSize"}, {"set_fixed_size", "setFixedSize"},
        {"text", "text"}
    }},
    {"SInput", {
        {"set_text", "setText"}, {"text", "text"}, {"set_placeholder", "setPlaceholder"}
    }},
    {"SLabel", {
        {"set_text", "setText"}, {"text", "text"}, {"set_color", "setColor"}
    }},
    {"STabs", {
        {"set_tab_text", "setTabText"}, {"current_index", "currentIndex"},
        {"tab_count", "tabCount"}, {"set_current_index", "setCurrentIndex"},
        {"remove_tab", "removeTab"}
    }},
    {"SListView", {
        {"set_model", "setModel"}, {"set_root_index", "setRootIndex"},
        {"set_icon_mode", "setIconMode"}, {"set_view_mode", "setViewMode"}
    }},
    {"SSplitter", {
        {"set_handle_width", "setHandleWidth"}
    }},
    {"SFileSystemModel", {
        {"set_root_path", "setRootPath"}, {"refresh", "refresh"},
        {"file_path", "filePath"}, {"root_path", "rootPath"}
    }},
    {"SHBoxLayout", {
        {"add", "addWidget"}, {"set_spacing", "setSpacing"}
    }},
    {"SVBoxLayout", {
        {"add", "addWidget"}, {"set_spacing", "setSpacing"}
    }},
    {"SWidget", {
        {"set_bounds", "setBounds"}, {"set_position", "setPosition"},
        {"set_size", "setSize"}, {"set_width", "setWidth"}, {"set_height", "setHeight"},
        {"show", "show"}, {"hide", "hide"}, {"set_visible", "setVisible"},
        {"set_enabled", "setEnabled"}, {"set_object_name", "setObjectName"},
        {"addWidget", "addWidget"}, {"width", "width"}, {"height", "height"}
    }},
    {"SpyProcess", {
        {"start_detached", "startDetached"}, {"run", "run"}
    }},
    {"SpyPaths", {
        {"home", "home"}, {"desktop", "desktop"}, {"downloads", "downloads"},
        {"documents", "documents"}, {"temp", "temp"}
    }},
    {"SpyDir", {
        {"dir_name", "dirName"}, {"home_path", "homePath"},
        {"exists", "exists"}, {"is_dir", "isDir"}, {"entry_list", "entryList"}
    }},
    {"SpyStorage", {
        {"mounted_volumes", "mountedVolumes"}
    }},
    {"SpyStyle", {
        {"apply", "apply"}, {"set_dark_neon", "setDarkNeon"}
    }},
    {"SPainter", {
        {"draw_rect", "drawRect"}, {"draw_text", "drawText"}, {"fill_rect", "fillRect"}
    }}
};

void CppCodegen::add_search_path(const std::string& path) {
    m_search_paths.push_back(path);
}

std::string CppCodegen::find_module_file(const std::string& mod) const {
    for (auto& p : m_search_paths) {
        std::string full = p + "/" + mod + ".spy";
        std::ifstream test(full);
        if (test.is_open()) {
            test.close();
            return full;
        }
    }
    std::string fallback = mod + ".spy";
    std::ifstream test(fallback);
    if (test.is_open()) {
        test.close();
        return fallback;
    }
    return "";
}

bool CppCodegen::is_spyui_type(const std::string& name) const {
    return KNOWN_SPYUI_TYPES.count(name) > 0;
}

std::string CppCodegen::spy_method_to_cpp(const std::string& type, const std::string& method) const {
    auto it = METHOD_MAP.find(type);
    if (it != METHOD_MAP.end()) {
        auto m = it->second.find(method);
        if (m != it->second.end()) return m->second;
    }
    return method;
}

std::string CppCodegen::spy_type_to_cpp(const std::string& spy_type) const {
    if (spy_type == "string" || spy_type == "str") return "std::string";
    if (spy_type == "i8") return "int8_t";
    if (spy_type == "i16") return "int16_t";
    if (spy_type == "i32") return "int32_t";
    if (spy_type == "i64") return "int64_t";
    if (spy_type == "u8") return "uint8_t";
    if (spy_type == "u16") return "uint16_t";
    if (spy_type == "u32") return "uint32_t";
    if (spy_type == "u64") return "uint64_t";
    if (spy_type == "f32") return "float";
    if (spy_type == "f64" || spy_type == "double") return "double";
    if (spy_type == "bool") return "bool";
    if (spy_type == "char") return "char";
    if (spy_type == "usize" || spy_type == "int") return "int";
    if (spy_type == "void") return "void";
    if (is_spyui_type(spy_type)) return "spy::" + spy_type;
    return spy_type;
}

std::string CppCodegen::type_to_cpp(ASTNode* type_node) {
    if (!type_node) return "auto";
    if (type_node->type == NodeType::TYPE_EXPR) {
        auto* t = static_cast<TypeExpr*>(type_node);
        return spy_type_to_cpp(t->name);
    }
    if (type_node->type == NodeType::PTR_TYPE_EXPR) {
        auto* p = static_cast<PtrTypeExpr*>(type_node);
        return type_to_cpp(p->base_type.get()) + "*";
    }
    return "auto";
}

std::string CppCodegen::get_expr_type(ASTNode* node) {
    if (!node) return "auto";
    switch (node->type) {
        case NodeType::INT_EXPR: return "int";
        case NodeType::FLOAT_EXPR: return "double";
        case NodeType::STRING_EXPR: return "std::string";
        case NodeType::BOOL_EXPR: return "bool";
        case NodeType::NONE_EXPR: return "nullptr";
        case NodeType::IDENT_EXPR: {
            auto* id = static_cast<IdentExpr*>(node);
            {
                auto ev = m_variant_to_enum.find(id->name);
                if (ev != m_variant_to_enum.end()) return ev->second;
            }
            auto it = m_var_types.find(id->name);
            if (it != m_var_types.end()) return it->second;
            if (is_spyui_type(id->name)) return "spy::" + id->name;
            return "auto";
        }
        case NodeType::CALL_EXPR: {
            auto* call = static_cast<CallExpr*>(node);
            auto ev = m_variant_to_enum.find(call->callee);
            if (ev != m_variant_to_enum.end()) return ev->second;
            auto it = m_all_fns.find(call->callee);
            if (it != m_all_fns.end() && it->second->return_type) {
                return get_expr_type(it->second->return_type.get());
            }
            if (is_spyui_type(call->callee)) return "spy::" + call->callee;
            if (call->callee == "read_file" || call->callee == "input" || call->callee == "exec" || call->callee == "get_cwd"
                || call->callee == "chr" || call->callee == "substr" || call->callee == "str") return "std::string";
            return "auto";
        }
        default: return "auto";
    }
}

std::string CppCodegen::get_ident_name(ASTNode* node) {
    if (!node) return "";
    if (node->type == NodeType::IDENT_EXPR) return static_cast<IdentExpr*>(node)->name;
    return "";
}

bool CppCodegen::is_string_expr(ASTNode* node) {
    if (!node) return false;
    if (node->type == NodeType::STRING_EXPR) return true;
    if (node->type == NodeType::IDENT_EXPR) {
        auto it = m_var_types.find(static_cast<IdentExpr*>(node)->name);
        if (it != m_var_types.end()) return it->second == "std::string";
    }
    return false;
}

std::string CppCodegen::emit_expression(ASTNode* expr) {
    if (!expr) return "";

    switch (expr->type) {
        case NodeType::INT_EXPR: return std::to_string(static_cast<IntExpr*>(expr)->value);
        case NodeType::FLOAT_EXPR: return std::to_string(static_cast<FloatExpr*>(expr)->value);
        case NodeType::STRING_EXPR: return "\"" + escape_cpp_string(static_cast<StringExpr*>(expr)->value) + "\"";
        case NodeType::BOOL_EXPR: return static_cast<BoolExpr*>(expr)->value ? "true" : "false";
        case NodeType::NONE_EXPR: return "nullptr";

        case NodeType::IDENT_EXPR: {
            auto* id = static_cast<IdentExpr*>(expr);
            if (id->name == "true") return "true";
            if (id->name == "false") return "false";
            if (id->name == "none" || id->name == "None") return "nullptr";
            if (id->name == "print") return "std::cout";
            {
                auto ev = m_variant_to_enum.find(id->name);
                if (ev != m_variant_to_enum.end()) {
                    auto& variants = m_enum_variants[ev->second];
                    for (auto& v : variants) {
                        if (v.name == id->name && !v.has_payload) {
                            std::string lcname = ev->second;
                            for (auto& c_ : lcname) c_ = (char)tolower(c_);
                            std::string lname = id->name;
                            for (auto& c_ : lname) c_ = (char)tolower(c_);
                            return lcname + "_" + lname + "()";
                        }
                    }
                }
            }
            return id->name;
        }

        case NodeType::MEMBER_EXPR: {
            auto* m = static_cast<MemberExpr*>(expr);
            std::string obj = emit_expression(m->object.get());
            std::string obj_type = get_expr_type(m->object.get());
            bool ptr = (obj_type.size() > 1 && obj_type.back() == '*');
            return obj + (ptr ? "->" : ".") + m->member;
        }

        case NodeType::METHOD_CALL_EXPR: {
            return emit_method_call(static_cast<MethodCallExpr*>(expr));
        }

        case NodeType::CALL_EXPR: {
            return emit_call(static_cast<CallExpr*>(expr));
        }

        case NodeType::BINOP_EXPR: {
            return emit_binop(static_cast<BinOpExpr*>(expr));
        }

        case NodeType::UNARY_EXPR: {
            return emit_unary(static_cast<UnaryExpr*>(expr));
        }

        case NodeType::TERNARY_EXPR: {
            auto* t = static_cast<TernaryExpr*>(expr);
            return "(" + emit_expression(t->condition.get()) + " ? " +
                   emit_expression(t->then_expr.get()) + " : " +
                   emit_expression(t->else_expr.get()) + ")";
        }

        case NodeType::INDEX_EXPR: {
            auto* idx = static_cast<IndexExpr*>(expr);
            std::string obj = emit_expression(idx->object.get());
            std::string i = emit_expression(idx->index.get());
            return obj + "[" + i + "]";
        }

        case NodeType::ARRAY_EXPR: {
            auto* arr = static_cast<ArrayExpr*>(expr);
            std::string out = "std::vector<double>{";
            for (size_t i = 0; i < arr->elements.size(); ++i) {
                if (i > 0) out += ", ";
                out += emit_expression(arr->elements[i].get());
            }
            out += "}";
            return out;
        }

        case NodeType::LIST_COMP_EXPR: {
            auto* lc = static_cast<ListCompExpr*>(expr);
            return "[&]() { std::vector<double> __r; for (auto& " +
                   lc->var + " : " + emit_expression(lc->iterable.get()) +
                   ") { __r.push_back(" + emit_expression(lc->element.get()) +
                   "); } return __r; }()";
        }

        case NodeType::PIPE_EXPR: {
            auto* p = static_cast<PipeExpr*>(expr);
            std::string val = emit_expression(p->value.get());
            if (p->call->type == NodeType::CALL_EXPR) {
                auto* c = static_cast<CallExpr*>(p->call.get());
                std::string args = val;
                for (auto& a : c->args) args += ", " + emit_expression(a.get());
                return c->callee + "(" + args + ")";
            }
            return emit_expression(p->call.get()) + "(" + val + ")";
        }

        case NodeType::LAMBDA_EXPR: {
            auto* l = static_cast<LambdaExpr*>(expr);
            std::string out = "[&](";
            for (size_t i = 0; i < l->params.size(); ++i) {
                if (i > 0) out += ", ";
                out += "auto " + l->params[i];
            }
            out += ") -> auto { ";
            for (auto& s : l->body) {
                if (s->type == NodeType::RETURN_STMT) {
                    out += "return " + emit_expression(static_cast<ReturnStmt*>(s.get())->expression.get()) + "; ";
                } else if (s->type == NodeType::EXPR_STMT) {
                    out += emit_expression(static_cast<ExprStmt*>(s.get())->expression.get()) + "; ";
                } else {
                    out += emit_expression(s.get()) + "; ";
                }
            }
            out += "}";
            return out;
        }

        case NodeType::ASSIGN_STMT: {
            auto* a = static_cast<AssignStmt*>(expr);
            std::string target = emit_expression(a->target.get());
            std::string val = emit_expression(a->value.get());
            return "(" + target + " = " + val + ")";
        }

        default: return "/* unhandled expr */";
    }
}

std::string CppCodegen::emit_call(CallExpr* node) {
    std::string callee = node->callee;

    if (is_spyui_type(callee)) {
        std::string out;
        if (VALUE_TYPES.count(callee)) {
            out = "spy::" + callee + "(";
        } else {
            out = "new spy::" + callee + "(";
        }
        for (size_t i = 0; i < node->args.size(); ++i) {
            if (i > 0) out += ", ";
            out += emit_expression(node->args[i].get());
        }
        out += ")";
        return out;
    }

    if (callee == "print") {
        std::string out = "std::cout";
        for (size_t i = 0; i < node->args.size(); ++i) {
            if (i > 0) out += " << \" \"";
            auto* arg = node->args[i].get();
            if (arg->type == NodeType::STRING_EXPR) {
                out += " << \"" + escape_cpp_string(static_cast<StringExpr*>(arg)->value) + "\"";
            } else {
                out += " << " + emit_expression(arg);
            }
        }
        out += " << \"\\n\"";
        return out;
    }

    if (callee == "len") {
        std::string arg = emit_expression(node->args[0].get());
        return "(int)(" + arg + ".size())";
    }

    if (callee == "str") {
        return "std::to_string(" + emit_expression(node->args[0].get()) + ")";
    }

    if (callee == "int") {
        return "(int)(" + emit_expression(node->args[0].get()) + ")";
    }

    if (callee == "float") {
        return "(double)(" + emit_expression(node->args[0].get()) + ")";
    }

    if (callee == "bool") {
        std::string arg = emit_expression(node->args[0].get());
        std::string type = get_expr_type(node->args[0].get());
        if (type == "std::string") {
            if (node->args[0]->type == NodeType::STRING_EXPR)
                return "(double)(strlen(" + arg + ") > 0)";
            return "(double)(!" + arg + ".empty())";
        }
        if (type == "std::vector<double>")
            return "(double)(!" + arg + ".empty())";
        return "(double)((" + arg + ") != 0.0)";
    }

    if (callee == "abs") {
        return "std::abs(" + emit_expression(node->args[0].get()) + ")";
    }

    if (callee == "chr" && node->args.size() == 1) {
        return "std::string(1, (char)(int)(" + emit_expression(node->args[0].get()) + "))";
    }

    if (callee == "substr" && node->args.size() == 3) {
        return "(" + emit_expression(node->args[0].get()) + ".substr((int)(" + emit_expression(node->args[1].get()) + "), (int)(" + emit_expression(node->args[2].get()) + ")))";
    }

    if (callee == "list") {
        std::string out = "std::vector<double>{";
        for (size_t i = 0; i < node->args.size(); ++i) {
            if (i > 0) out += ", ";
            out += emit_expression(node->args[i].get());
        }
        out += "}";
        return out;
    }

    if (callee == "read_file" && node->args.size() == 1) {
        return "spyui_read_file(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "write_file" && node->args.size() == 2) {
        return "spyui_write_file(" + emit_expression(node->args[0].get()) + ", " + emit_expression(node->args[1].get()) + ")";
    }
    if (callee == "file_exists" && node->args.size() == 1) {
        return "spyui_file_exists(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "file_size" && node->args.size() == 1) {
        return "spyui_file_size(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "mkdir" && node->args.size() == 1) {
        return "spyui_mkdir(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "rm" && node->args.size() == 1) {
        return "spyui_remove(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "rename_file" && node->args.size() == 2) {
        return "spyui_rename(" + emit_expression(node->args[0].get()) + ", " + emit_expression(node->args[1].get()) + ")";
    }
    if (callee == "get_cwd" && node->args.size() == 0) {
        return "spyui_get_cwd()";
    }
    if (callee == "exit" && node->args.size() == 1) {
        return "exit((int)(" + emit_expression(node->args[0].get()) + "))";
    }
    if (callee == "error" && node->args.size() == 1) {
        return "throw std::runtime_error(" + emit_expression(node->args[0].get()) + ")";
    }
    if (callee == "cp" && node->args.size() == 2) {
        std::string src = emit_expression(node->args[0].get());
        std::string dst = emit_expression(node->args[1].get());
        return "spyui_write_file(" + dst + ", spyui_read_file(" + src + "))";
    }

    auto ev = m_variant_to_enum.find(callee);
    if (ev != m_variant_to_enum.end()) {
        auto& variants = m_enum_variants[ev->second];
        for (auto& v : variants) {
            if (v.name == callee) {
                std::string en = ev->second;
                std::string lcname = en;
                for (auto& c_ : lcname) c_ = (char)tolower(c_);
                if (v.has_payload) {
                    std::string lvname = callee;
                    for (auto& c_ : lvname) c_ = (char)tolower(c_);
                    std::string out = lcname + "_" + lvname + "(";
                    for (size_t i = 0; i < node->args.size(); ++i) {
                        if (i > 0) out += ", ";
                        out += emit_expression(node->args[i].get());
                    }
                    return out + ")";
                } else {
                    return en + "()";
                }
            }
        }
    }

    // Resolve from-import calls: module.func() or func() when using "from module import func"
    for (auto& [mod, fns] : m_module_from_fns) {
        if (fns.count(callee)) {
            std::string prefix = m_module_prefix + mod + "_" + callee;
            std::string args;
            for (size_t i = 0; i < node->args.size(); ++i) {
                if (i > 0) args += ", ";
                args += emit_expression(node->args[i].get());
            }
            auto dit = m_fn_defaults.find(prefix);
            if (dit != m_fn_defaults.end()) {
                auto* fn = dit->second;
                for (size_t i = node->args.size(); i < fn->params.size(); ++i) {
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

    // Module prefix for internal module calls
    if (!m_module_prefix.empty()) {
        std::string args;
        for (size_t i = 0; i < node->args.size(); ++i) {
            if (i > 0) args += ", ";
            args += emit_expression(node->args[i].get());
        }
        return m_module_prefix + callee + "(" + args + ")";
    }

    std::string out = callee + "(";
    for (size_t i = 0; i < node->args.size(); ++i) {
        if (i > 0) out += ", ";
        out += emit_expression(node->args[i].get());
    }
    out += ")";
    return out;
}

std::string CppCodegen::emit_method_call(MethodCallExpr* node) {
    std::string obj = emit_expression(node->object.get());
    std::string obj_type = get_expr_type(node->object.get());
    std::string method = node->method;

    // Module method call: module.func()
    std::string obj_name = get_ident_name(node->object.get());
    if (m_module_fns.count(obj_name) && m_module_fns[obj_name].count(method)) {
        std::string prefix = m_module_prefix + obj_name + "_" + method;
        std::string args;
        for (size_t i = 0; i < node->args.size(); ++i) {
            if (i > 0) args += ", ";
            args += emit_expression(node->args[i].get());
        }
        auto dit = m_fn_defaults.find(prefix);
        if (dit != m_fn_defaults.end()) {
            auto* fn = dit->second;
            for (size_t i = node->args.size(); i < fn->params.size(); ++i) {
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

    bool is_ptr = (obj_type.size() > 1 && obj_type.back() == '*');
    std::string base_type = is_ptr ? obj_type.substr(0, obj_type.size() - 1) : obj_type;

    auto it = METHOD_MAP.find(base_type);
    bool found_method = false;
    if (it != METHOD_MAP.end()) {
        auto m = it->second.find(method);
        if (m != it->second.end()) { method = m->second; found_method = true; }
    }
    if (!found_method) {
        auto pit = TYPE_PARENT.find(base_type);
        if (pit != TYPE_PARENT.end()) {
            auto pit2 = METHOD_MAP.find(pit->second);
            if (pit2 != METHOD_MAP.end()) {
                auto m = pit2->second.find(method);
                if (m != pit2->second.end()) { method = m->second; found_method = true; }
            }
        }
    }

    std::string sep = is_ptr ? "->" : ".";

    if (obj_type == "std::string") {
        if (method == "upper") return "([](const std::string& s) { auto r = s; std::transform(r.begin(), r.end(), r.begin(), ::toupper); return r; })(" + obj + ")";
        if (method == "lower") return "([](const std::string& s) { auto r = s; std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r; })(" + obj + ")";
        if (method == "strip") return "([](const std::string& s) { auto b = s.find_first_not_of(\" \\t\\n\\r\"); auto e = s.find_last_not_of(\" \\t\\n\\r\"); return (b == std::string::npos) ? \"\" : s.substr(b, e - b + 1); })(" + obj + ")";
        if (method == "find") return "(" + obj + ".find(" + emit_expression(node->args[0].get()) + "))";
        if (method == "contains") return "(" + obj + ".find(" + emit_expression(node->args[0].get()) + ") != std::string::npos)";
        if (method == "startswith") return "(" + obj + ".substr(0, " + emit_expression(node->args[0].get()) + ".size()) == " + emit_expression(node->args[0].get()) + ")";
        if (method == "endswith") {
            std::string suffix = emit_expression(node->args[0].get());
            return "(" + obj + ".size() >= " + suffix + ".size() && " + obj + ".compare(" + obj + ".size() - " + suffix + ".size(), " + suffix + ".size(), " + suffix + ") == 0)";
        }
        if (method == "replace") {
            return "(" + obj + ".replace(...))";
        }
        if (method == "append") return "(" + obj + " += " + emit_expression(node->args[0].get()) + ")";
    }

    if (obj_type == "auto" || obj_type == "std::vector<double>") {
        if (method == "push" || method == "append") return "(" + obj + ".push_back(" + emit_expression(node->args[0].get()) + "), 0)";
        if (method == "pop") {
            return "([&](){ auto __v = " + obj + ".back(); " + obj + ".pop_back(); return __v; }())";
        }
        if (method == "size") return "(int)(" + obj + ".size())";
        if (method == "get" && node->args.size() == 1)
            return obj + "[(int)(" + emit_expression(node->args[0].get()) + ")]";
        if (method == "set" && node->args.size() == 2)
            return "(" + obj + "[(int)(" + emit_expression(node->args[0].get()) + ")] = " + emit_expression(node->args[1].get()) + ", 0)";
    }

    std::string out = obj + sep + method + "(";
    for (size_t i = 0; i < node->args.size(); ++i) {
        if (i > 0) out += ", ";
        out += emit_expression(node->args[i].get());
    }
    out += ")";
    return out;
}

std::string CppCodegen::emit_binop(BinOpExpr* node) {
    std::string l = emit_expression(node->left.get());
    std::string r = emit_expression(node->right.get());

    if (node->op == "and") return l + " && " + r;
    if (node->op == "or") return l + " || " + r;
    if (node->op == "+") {
        auto lt = get_expr_type(node->left.get());
        auto rt = get_expr_type(node->right.get());
        if (lt == "std::string" || rt == "std::string") {
            return l + " + " + r;
        }
    }
    return l + " " + node->op + " " + r;
}

std::string CppCodegen::emit_unary(UnaryExpr* node) {
    std::string op = node->op;
    std::string operand = emit_expression(node->operand.get());
    if (op == "not") return "!" + operand;
    if (op == "-") return "-" + operand;
    return op + operand;
}

void CppCodegen::emit_block(const std::vector<ASTPtr>& stmts, std::string& out, int indent) {
    for (auto& s : stmts) {
        if (s) emit_node(s.get(), out, indent);
    }
}

void CppCodegen::emit_let(LetStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    std::string name = node->name;
    std::string init = emit_expression(node->initializer.get());

    if (node->initializer && node->initializer->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExpr*>(node->initializer.get());
        if (call->callee == "list") {
            m_var_types[name] = "std::vector<double>";
            out += pad + "auto " + name + " = " + init + ";\n";
            return;
        }
        auto ev = m_variant_to_enum.find(call->callee);
        if (ev != m_variant_to_enum.end()) {
            m_var_types[name] = ev->second;
            out += pad + "auto " + name + " = " + init + ";\n";
            return;
        }
        if (is_spyui_type(call->callee)) {
            m_var_types[name] = call->callee + "*";
            out += pad + "auto* " + name + " = " + init + ";\n";
            return;
        }
    }

    std::string type = get_expr_type(node->initializer.get());
    m_var_types[name] = type;

    if (type == "auto" || type == "void") {
        out += pad + "auto " + name + " = " + init + ";\n";
    } else if (type == "std::string") {
        out += pad + "std::string " + name + " = " + init + ";\n";
    } else if (type == "bool") {
        out += pad + "bool " + name + " = " + init + ";\n";
    } else if (type == "int" || type == "i32" || type == "usize") {
        out += pad + "int " + name + " = " + init + ";\n";
    } else if (type == "double" || type == "f64") {
        out += pad + "double " + name + " = " + init + ";\n";
    } else if (is_spyui_type(type)) {
        out += pad + "auto " + name + " = " + init + ";\n";
    } else {
        out += pad + "auto " + name + " = " + init + ";\n";
    }
}

void CppCodegen::emit_fn(FnStmt* node, std::string& out) {
    std::string ret_type = "auto";
    if (node->return_type) {
        if (node->return_type->type == NodeType::IDENT_EXPR)
            ret_type = spy_type_to_cpp(static_cast<IdentExpr*>(node->return_type.get())->name);
    }
    m_all_fns[node->name] = node;

    std::string sig = ret_type + " " + node->name + "(";
    for (size_t i = 0; i < node->params.size(); ++i) {
        if (i > 0) sig += ", ";
        sig += "double " + node->params[i];
    }
    sig += ")";
    out += sig + " {\n";
    emit_block(node->body, out, 1);
    if (ret_type == "void") {
        out += "    return;\n";
    } else if (ret_type == "auto") {
        bool has_return = false;
        for (auto& s : node->body) {
            if (s && s->type == NodeType::RETURN_STMT) {
                has_return = true;
                break;
            }
        }
        if (!has_return) {
            out += "    return;\n";
        }
    }
    out += "}\n\n";
}

void CppCodegen::emit_class(ClassStmt* node, std::string& out) {
    m_current_class = node->name;
    if (!node->parents.empty()) m_class_parent_name[node->name] = node->parents[0];

    std::string parent;
    if (!node->parents.empty()) parent = " : public " + spy_type_to_cpp(node->parents[0]);
    out += "class " + node->name + parent + " {\npublic:\n";

    for (auto& m : node->methods) {
        auto* fn = static_cast<FnStmt*>(m.get());
        std::string ret = "void";
        if (fn->return_type) {
            if (fn->return_type->type == NodeType::IDENT_EXPR)
                ret = spy_type_to_cpp(static_cast<IdentExpr*>(fn->return_type.get())->name);
        }
        out += "    " + ret + " " + fn->name + "(";
        bool first = true;
        for (auto& p : fn->params) {
            if (!first) out += ", ";
            out += "auto " + p;
            first = false;
        }
        out += ") {\n";
        emit_block(fn->body, out, 2);
        out += "    }\n";
    }
    out += "};\n\n";
    m_current_class.clear();
}

void CppCodegen::emit_return(ReturnStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    if (node->expression) {
        out += pad + "return " + emit_expression(node->expression.get()) + ";\n";
    } else {
        out += pad + "return;\n";
    }
}

void CppCodegen::emit_if(IfStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    out += pad + "if (" + emit_expression(node->condition.get()) + ") {\n";
    emit_block(node->then_body, out, indent + 1);
    if (node->has_else) {
        out += pad + "} else {\n";
        emit_block(node->else_body, out, indent + 1);
    }
    out += pad + "}\n";
}

void CppCodegen::emit_while(WhileStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    out += pad + "while (" + emit_expression(node->condition.get()) + ") {\n";
    emit_block(node->body, out, indent + 1);
    out += pad + "}\n";
}

void CppCodegen::emit_for(ForStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    if (node->iterable) {
        std::string iterable = emit_expression(node->iterable.get());
        out += pad + "for (auto& " + node->var + " : " + iterable + ") {\n";
    } else if (node->end) {
        std::string start_val = node->start ? emit_expression(node->start.get()) : "0";
        std::string end_val = emit_expression(node->end.get());
        out += pad + "for (int " + node->var + " = " + start_val + "; " + node->var + " < " + end_val + "; ++" + node->var + ") {\n";
    }
    emit_block(node->body, out, indent + 1);
    out += pad + "}\n";
}

void CppCodegen::emit_print(PrintStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    out += pad + "std::cout";
    for (size_t i = 0; i < node->expressions.size(); ++i) {
        auto* arg = node->expressions[i].get();
        std::string expr = emit_expression(arg);
        std::string type = get_expr_type(arg);
        if (type == "std::vector<double>") {
            out += " << [&](){ std::cout << \"[\"; bool __f = true; for (auto& __v : " + expr + ") { if (!__f) std::cout << \", \"; __f = false; std::cout << __v; } std::cout << \"]\"; return \"\"; }()";
        } else if (arg->type == NodeType::STRING_EXPR) {
            out += " << \"" + escape_cpp_string(static_cast<StringExpr*>(arg)->value) + "\"";
        } else {
            out += " << " + expr;
        }
        if (i + 1 < node->expressions.size()) out += " << \" \"";
    }
    out += " << \"\\n\";\n";
}

void CppCodegen::emit_expr_stmt(ExprStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    std::string expr = emit_expression(node->expression.get());
    out += pad + expr + ";\n";
}

void CppCodegen::emit_try(TryStmt* node, std::string& out, int indent) {
    std::string pad = indent_str(indent);
    out += pad + "try {\n";
    emit_block(node->body, out, indent + 1);
    for (auto& h : node->handlers) {
        auto* handler = static_cast<ExceptHandler*>(h.get());
        out += pad + "} catch (std::exception& " + handler->var_name + ") {\n";
        emit_block(handler->body, out, indent + 1);
    }
    out += pad + "}\n";
}

void CppCodegen::emit_match(MatchStmt* node, std::string& out, int indent) {
    std::string val = emit_expression(node->value.get());
    std::string pad = indent_str(indent);
    bool has_ctor = false;
    for (auto& c : node->cases) {
        if (c.pattern && c.pattern->type == NodeType::CONSTRUCTOR_PATTERN) {
            has_ctor = true; break;
        }
    }
    if (has_ctor) {
        std::string enum_type = "int";
        for (auto& c : node->cases) {
            if (c.pattern->type == NodeType::CONSTRUCTOR_PATTERN) {
                auto* cp = static_cast<ConstructorPattern*>(c.pattern.get());
                auto it = m_variant_to_enum.find(cp->variant_name);
                if (it != m_variant_to_enum.end()) {
                    enum_type = it->second;
                    break;
                }
            }
        }
        out += pad + "switch (" + val + ".tag) {\n";
        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& c = node->cases[i];
            if (c.pattern->type == NodeType::NONE_EXPR) continue;
            if (c.pattern->type == NodeType::CONSTRUCTOR_PATTERN) {
                auto* cp = static_cast<ConstructorPattern*>(c.pattern.get());
                auto it = m_variant_to_enum.find(cp->variant_name);
                if (it != m_variant_to_enum.end()) {
                    out += pad + "    case " + it->second + "_Tag_" + cp->variant_name + ": {\n";
                    auto& variants = m_enum_variants[it->second];
                    for (auto& v : variants) {
                        if (v.name == cp->variant_name && v.has_payload) {
                            std::string lname = cp->variant_name;
                            for (auto& c_ : lname) c_ = (char)tolower(c_);
                            for (size_t fi = 0; fi < v.fields.size() && fi < cp->bindings.size(); ++fi) {
                                std::string ft = v.fields[fi].type_name;
                                if (ft == "i32") ft = "int";
                                else if (ft == "f64") ft = "double";
                                else if (ft == "bool") ft = "bool";
                                else if (ft == "string") ft = "std::string";
                                else if (ft.size() > 2 && ft[0] == '[' && ft.back() == ']') {
                                    ft = ft.substr(1, ft.size() - 2) + "*";
                                } else ft = ft + "*";
                                out += pad + "        " + ft + " " + cp->bindings[fi] + " = " + val + ".data." + lname + "." + v.fields[fi].name + ";\n";
                            }
                            break;
                        }
                    }
                    for (auto& s : c.body) if (s) emit_node(s.get(), out, indent + 2);
                    out += pad + "        break;\n";
                    out += pad + "    }\n";
                }
            } else {
                std::string cond;
                if (c.pattern->type == NodeType::INT_EXPR) {
                    cond = std::to_string(static_cast<IntExpr*>(c.pattern.get())->value);
                } else if (c.pattern->type == NodeType::IDENT_EXPR) {
                    auto* id = static_cast<IdentExpr*>(c.pattern.get());
                    auto pit = m_variant_to_enum.find(id->name);
                    if (pit != m_variant_to_enum.end()) {
                        cond = pit->second + "_Tag_" + id->name;
                    } else {
                        cond = emit_expression(c.pattern.get());
                    }
                } else {
                    cond = emit_expression(c.pattern.get());
                }
                out += pad + "    case " + cond + ": {\n";
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
        bool first = true;
        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& c = node->cases[i];
            if (c.pattern && c.pattern->type == NodeType::NONE_EXPR) continue;
            std::string cond = (c.pattern && c.pattern->type == NodeType::INT_EXPR)
                ? std::to_string(static_cast<IntExpr*>(c.pattern.get())->value)
                : emit_expression(c.pattern.get());
            if (first) {
                out += pad + "if (" + val + " == " + cond + ") {\n";
                first = false;
            } else {
                out += pad + "} else if (" + val + " == " + cond + ") {\n";
            }
            emit_block(c.body, out, indent + 1);
        }
        if (node->default_index >= 0) {
            out += pad + "} else {\n";
            auto& dc = node->cases[node->default_index];
            emit_block(dc.body, out, indent + 1);
        }
        out += pad + "}\n";
    }
}

void CppCodegen::emit_import(ImportStmt* node, std::string& out) {
    if (node->header == "spyui.h") {
        m_has_spyui_import = true;
        out += "#include <spyui.h>\n";
        out += "#include <iostream>\n";
        out += "#include <string>\n";
        out += "#include <vector>\n";
        out += "#include <map>\n";
        out += "#include <cmath>\n";
        out += "#include <algorithm>\n";
        out += "using namespace spy;\n\n";
    } else if (node->is_c_header) {
        out += "#include <" + node->header + ">\n";
    } else {
        // .spy module import
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

        // Handle nested imports
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
                                CppCodegen ninner;
                                ninner.m_module_prefix = m_module_prefix + mod + "_" + nested_mod + "_";
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

        // Emit module functions
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
                CppCodegen inner;
                inner.m_module_prefix = m_module_prefix + mod + "_";
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
}

void CppCodegen::emit_struct(StructStmt* node, std::string& out) {
    out += "struct " + node->name + " {\n";
    for (auto& f : node->fields) {
        std::string ftype = type_to_cpp(f.type.get());
        out += "    " + ftype + " " + f.name + ";\n";
    }
    out += "};\n\n";
}

void CppCodegen::emit_extern_fn(ExternFnStmt* node, std::string& out) {
    std::string ret = "void";
    if (node->return_type) {
        if (node->return_type->type == NodeType::IDENT_EXPR)
            ret = spy_type_to_cpp(static_cast<IdentExpr*>(node->return_type.get())->name);
    }
    out += node->header + " " + ret + " " + node->name + "(";
    for (size_t i = 0; i < node->typed_params.size(); ++i) {
        if (i > 0) out += ", ";
        std::string ptype = "auto";
        if (node->typed_params[i].type && node->typed_params[i].type->type == NodeType::IDENT_EXPR)
            ptype = spy_type_to_cpp(static_cast<IdentExpr*>(node->typed_params[i].type.get())->name);
        out += ptype + " " + node->typed_params[i].name;
    }
    out += ");\n";
}

void CppCodegen::emit_enum(EnumStmt* node, std::string& out) {
    bool has_payload = false;
    for (auto& v : node->variants) if (v.has_payload) { has_payload = true; break; }
    if (has_payload) {
        std::string lcname = node->name;
        for (auto& c : lcname) c = (char)tolower(c);
        out += "enum " + node->name + "_Tag { ";
        for (size_t i = 0; i < node->variants.size(); ++i) {
            if (i > 0) out += ", ";
            out += node->name + "_Tag_" + node->variants[i].name + " = " + std::to_string(i);
        }
        out += " };\n";
        for (auto& v : node->variants) {
            if (!v.has_payload) continue;
            out += "struct " + node->name + "_" + v.name + "_Data { ";
            for (auto& f : v.fields) {
                std::string ft = v.fields[0].type_name;
                if (ft == "i32") ft = "int";
                else if (ft == "f64") ft = "double";
                else if (ft == "bool") ft = "bool";
                else if (ft == "string") ft = "std::string";
                else if (ft.size() > 2 && ft[0] == '[' && ft.back() == ']') {
                    ft = ft.substr(1, ft.size() - 2) + "*";
                } else ft = ft + "*";
                out += ft + " " + f.name + "; ";
            }
            out += "};\n";
        }
        out += "struct " + node->name + " {\n";
        out += "    " + node->name + "_Tag tag;\n";
        out += "    union {\n";
        for (auto& v : node->variants) {
            if (!v.has_payload) continue;
            std::string lname = v.name;
            for (auto& c : lname) c = (char)tolower(c);
            out += "        " + node->name + "_" + v.name + "_Data " + lname + ";\n";
        }
        out += "    } data;\n";
        out += "};\n\n";
        for (auto& v : node->variants) {
            if (!v.has_payload) continue;
            std::string lname = v.name;
            for (auto& c : lname) c = (char)tolower(c);
            out += node->name + " " + lcname + "_" + lname + "(";
            for (size_t i = 0; i < v.fields.size(); ++i) {
                if (i > 0) out += ", ";
                std::string ft = v.fields[i].type_name;
                if (ft == "i32") ft = "int";
                else if (ft == "f64") ft = "double";
                else if (ft == "bool") ft = "bool";
                else if (ft == "string") ft = "const std::string&";
                else if (ft.size() > 2 && ft[0] == '[' && ft.back() == ']') {
                    ft = ft.substr(1, ft.size() - 2) + "*";
                } else ft = ft + "*";
                out += ft + " " + v.fields[i].name;
            }
            out += ") {\n    " + node->name + " node;\n";
            out += "    node.tag = " + node->name + "_Tag_" + v.name + ";\n";
            for (auto& f : v.fields) {
                out += "    node.data." + lname + "." + f.name + " = " + f.name + ";\n";
            }
            out += "    return node;\n}\n\n";
        }
        for (auto& v : node->variants) {
            if (v.has_payload) continue;
            std::string lname = v.name;
            for (auto& c : lname) c = (char)tolower(c);
            out += node->name + " " + lcname + "_" + lname + "() {\n    " + node->name + " node;\n";
            out += "    node.tag = " + node->name + "_Tag_" + v.name + ";\n";
            out += "    return node;\n}\n\n";
        }
    } else {
        out += "enum " + node->name + " {\n";
        for (size_t i = 0; i < node->variants.size(); ++i) {
            out += "    " + node->name + "_" + node->variants[i].name + " = " + std::to_string(i);
            if (i + 1 < node->variants.size()) out += ",";
            out += "\n";
        }
        out += "};\n\n";
    }
}

void CppCodegen::emit_node(ASTNode* node, std::string& out, int indent) {
    switch (node->type) {
        case NodeType::PRINT_STMT: emit_print(static_cast<PrintStmt*>(node), out, indent); break;
        case NodeType::LET_STMT: emit_let(static_cast<LetStmt*>(node), out, indent); break;
        case NodeType::FN_STMT: emit_fn(static_cast<FnStmt*>(node), out); break;
        case NodeType::RETURN_STMT: emit_return(static_cast<ReturnStmt*>(node), out, indent); break;
        case NodeType::IF_STMT: emit_if(static_cast<IfStmt*>(node), out, indent); break;
        case NodeType::WHILE_STMT: emit_while(static_cast<WhileStmt*>(node), out, indent); break;
        case NodeType::IMPORT_STMT: emit_import(static_cast<ImportStmt*>(node), out); break;
        case NodeType::EXPR_STMT: emit_expr_stmt(static_cast<ExprStmt*>(node), out, indent); break;
        case NodeType::MATCH_STMT: emit_match(static_cast<MatchStmt*>(node), out, indent); break;
        case NodeType::FOR_STMT: emit_for(static_cast<ForStmt*>(node), out, indent); break;
        case NodeType::TRY_STMT: emit_try(static_cast<TryStmt*>(node), out, indent); break;
        case NodeType::CLASS_STMT: emit_class(static_cast<ClassStmt*>(node), out); break;
        case NodeType::STRUCT_STMT: emit_struct(static_cast<StructStmt*>(node), out); break;
        case NodeType::ENUM_STMT: emit_enum(static_cast<EnumStmt*>(node), out); break;
        case NodeType::EXTERN_FN_STMT: emit_extern_fn(static_cast<ExternFnStmt*>(node), out); break;
        case NodeType::BREAK_STMT: out += indent_str(indent) + "break;\n"; break;
        case NodeType::CONTINUE_STMT: out += indent_str(indent) + "continue;\n"; break;
        case NodeType::YIELD_STMT: break;
        case NodeType::GLOBAL_STMT: break;
        default: {
            std::string pad = indent_str(indent);
            out += pad + emit_expression(node) + ";\n";
            break;
        }
    }
}

std::string CppCodegen::generate(ASTNode* program) {
    std::string out;
    auto* prog = static_cast<Program*>(program);

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            m_all_fns[fn->name] = fn;
        }
    }

    out += "#include <spyui.h>\n";
    out += "#include <iostream>\n";
    out += "#include <string>\n";
    out += "#include <vector>\n";
    out += "#include <map>\n";
    out += "#include <cmath>\n";
    out += "#include <algorithm>\n";
    out += "#include <functional>\n";
    out += "using namespace spy;\n\n";

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportStmt*>(stmt.get());
            if (imp->is_c_header && imp->header != "spyui.h") {
                out += "#include <" + imp->header + ">\n";
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::ENUM_STMT) {
            auto* e = static_cast<EnumStmt*>(stmt.get());
            m_enum_variants[e->name] = e->variants;
            for (auto& v : e->variants) {
                m_variant_to_enum[v.name] = e->name;
            }
            emit_enum(e, out);
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::STRUCT_STMT) {
            emit_struct(static_cast<StructStmt*>(stmt.get()), out);
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::EXTERN_FN_STMT) {
            emit_extern_fn(static_cast<ExternFnStmt*>(stmt.get()), out);
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::CLASS_STMT) {
            emit_class(static_cast<ClassStmt*>(stmt.get()), out);
        }
    }

    // Process .spy module imports before functions
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportStmt*>(stmt.get());
            if (!imp->is_c_header) {
                emit_import(imp, out);
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name != "main") {
                emit_fn(fn, out);
            }
        }
    }

    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type != NodeType::IMPORT_STMT && stmt->type != NodeType::FN_STMT &&
            stmt->type != NodeType::CLASS_STMT && stmt->type != NodeType::STRUCT_STMT &&
            stmt->type != NodeType::ENUM_STMT && stmt->type != NodeType::EXTERN_FN_STMT) {
            emit_node(stmt.get(), out, 0);
        }
    }

    bool has_main_fn = false;
    for (auto& stmt : prog->statements) {
        if (stmt && stmt->type == NodeType::FN_STMT) {
            auto* fn = static_cast<FnStmt*>(stmt.get());
            if (fn->name == "main") {
                has_main_fn = true;
                out += "int main(int argc, char** argv) {\n";
                emit_block(fn->body, out, 1);
                out += "    return 0;\n";
                out += "}\n";
            }
        }
    }

    if (!has_main_fn) {
        out += "int main(int argc, char** argv) {\n";
        for (auto& stmt : prog->statements) {
            if (stmt && stmt->type != NodeType::IMPORT_STMT && stmt->type != NodeType::FN_STMT &&
                stmt->type != NodeType::CLASS_STMT && stmt->type != NodeType::STRUCT_STMT &&
                stmt->type != NodeType::ENUM_STMT && stmt->type != NodeType::EXTERN_FN_STMT) {
                emit_node(stmt.get(), out, 1);
            }
        }
        out += "    return 0;\n";
        out += "}\n";
    }

    return out;
}

std::string CppCodegen::get_last_error() const { return m_error; }

} // namespace spy
