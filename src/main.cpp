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

#include "spy/Lexer.h"
#include "spy/Parser.h"
#include "spy/Codegen.h"
#include "spy/CppCodegen.h"
#include "spy/IrCodegen.h"
#include "spy/AST.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <vector>

void print_ast(spy::ASTNode* node, int indent = 0) {
    std::string pad(indent * 2, ' ');

    switch (node->type) {
        case spy::NodeType::PROGRAM: {
            auto* p = static_cast<spy::Program*>(node);
            std::cout << pad << "Program(" << p->statements.size() << " statements)" << std::endl;
            for (auto& s : p->statements) {
                if (s) print_ast(s.get(), indent + 1);
            }
            break;
        }
        case spy::NodeType::PRINT_STMT: {
            auto* p = static_cast<spy::PrintStmt*>(node);
            std::cout << pad << "PrintStmt(" << p->expressions.size() << " args)" << std::endl;
            for (auto& e : p->expressions) print_ast(e.get(), indent + 1);
            break;
        }
        case spy::NodeType::LET_STMT: {
            auto* p = static_cast<spy::LetStmt*>(node);
            std::cout << pad << "LetStmt(name=\"" << p->name << "\")" << std::endl;
            print_ast(p->initializer.get(), indent + 1);
            break;
        }
        case spy::NodeType::FN_STMT: {
            auto* p = static_cast<spy::FnStmt*>(node);
            std::string dec = p->decorator.empty() ? "" : "@" + p->decorator + " ";
            std::cout << pad << dec << "FnStmt(name=\"" << p->name << "\", params=" << p->params.size() << ")" << std::endl;
            for (auto& s : p->body) {
                if (s) print_ast(s.get(), indent + 1);
            }
            break;
        }
        case spy::NodeType::RETURN_STMT: {
            auto* p = static_cast<spy::ReturnStmt*>(node);
            std::cout << pad << "ReturnStmt" << std::endl;
            if (p->expression) print_ast(p->expression.get(), indent + 1);
            break;
        }
        case spy::NodeType::IF_STMT: {
            auto* p = static_cast<spy::IfStmt*>(node);
            std::cout << pad << "IfStmt(has_else=" << (p->has_else ? "true" : "false") << ")" << std::endl;
            std::cout << pad << "  condition:" << std::endl;
            print_ast(p->condition.get(), indent + 2);
            std::cout << pad << "  then:" << std::endl;
            for (auto& s : p->then_body) if (s) print_ast(s.get(), indent + 2);
            if (p->has_else) {
                std::cout << pad << "  else:" << std::endl;
                for (auto& s : p->else_body) if (s) print_ast(s.get(), indent + 2);
            }
            break;
        }
        case spy::NodeType::WHILE_STMT: {
            auto* p = static_cast<spy::WhileStmt*>(node);
            std::cout << pad << "WhileStmt" << std::endl;
            std::cout << pad << "  condition:" << std::endl;
            print_ast(p->condition.get(), indent + 2);
            std::cout << pad << "  body:" << std::endl;
            for (auto& s : p->body) if (s) print_ast(s.get(), indent + 2);
            break;
        }
        case spy::NodeType::EXPR_STMT: {
            auto* p = static_cast<spy::ExprStmt*>(node);
            std::cout << pad << "ExprStmt" << std::endl;
            print_ast(p->expression.get(), indent + 1);
            break;
        }
        case spy::NodeType::INT_EXPR: {
            auto* p = static_cast<spy::IntExpr*>(node);
            std::cout << pad << "IntExpr(" << p->value << ")" << std::endl;
            break;
        }
        case spy::NodeType::FLOAT_EXPR: {
            auto* p = static_cast<spy::FloatExpr*>(node);
            std::cout << pad << "FloatExpr(" << p->value << ")" << std::endl;
            break;
        }
        case spy::NodeType::STRING_EXPR: {
            auto* p = static_cast<spy::StringExpr*>(node);
            std::cout << pad << "StringExpr(\"" << p->value << "\")" << std::endl;
            break;
        }
        case spy::NodeType::IDENT_EXPR: {
            auto* p = static_cast<spy::IdentExpr*>(node);
            std::cout << pad << "IdentExpr(\"" << p->name << "\")" << std::endl;
            break;
        }
        case spy::NodeType::BOOL_EXPR: {
            auto* p = static_cast<spy::BoolExpr*>(node);
            std::cout << pad << "BoolExpr(" << (p->value ? "True" : "False") << ")" << std::endl;
            break;
        }
        case spy::NodeType::NONE_EXPR: {
            std::cout << pad << "NoneExpr" << std::endl;
            break;
        }
        case spy::NodeType::BINOP_EXPR: {
            auto* p = static_cast<spy::BinOpExpr*>(node);
            std::cout << pad << "BinOpExpr(\"" << p->op << "\")" << std::endl;
            print_ast(p->left.get(), indent + 1);
            print_ast(p->right.get(), indent + 1);
            break;
        }
        case spy::NodeType::CALL_EXPR: {
            auto* p = static_cast<spy::CallExpr*>(node);
            std::cout << pad << "CallExpr(\"" << p->callee << "\", " << p->args.size() << " args)" << std::endl;
            for (auto& a : p->args) {
                print_ast(a.get(), indent + 1);
            }
            break;
        }
        case spy::NodeType::IMPORT_STMT: {
            auto* p = static_cast<spy::ImportStmt*>(node);
            std::cout << pad << "ImportStmt(\"" << p->header << "\")" << std::endl;
            break;
        }
        case spy::NodeType::PIPE_EXPR: {
            auto* p = static_cast<spy::PipeExpr*>(node);
            std::cout << pad << "PipeExpr" << std::endl;
            print_ast(p->value.get(), indent + 1);
            print_ast(p->call.get(), indent + 1);
            break;
        }
        case spy::NodeType::MATCH_STMT: {
            auto* p = static_cast<spy::MatchStmt*>(node);
            std::cout << pad << "MatchStmt(default=" << p->default_index << ")" << std::endl;
            print_ast(p->value.get(), indent + 1);
            break;
        }
        case spy::NodeType::FOR_STMT: {
            auto* p = static_cast<spy::ForStmt*>(node);
            std::cout << pad << "ForStmt(var=\"" << p->var << "\")" << std::endl;
            print_ast(p->end.get(), indent + 1);
            break;
        }
        case spy::NodeType::ARRAY_EXPR: {
            auto* p = static_cast<spy::ArrayExpr*>(node);
            std::cout << pad << "ArrayExpr(" << p->elements.size() << " elements)" << std::endl;
            for (auto& e : p->elements) print_ast(e.get(), indent + 1);
            break;
        }
        case spy::NodeType::INDEX_EXPR: {
            auto* p = static_cast<spy::IndexExpr*>(node);
            std::cout << pad << "IndexExpr" << std::endl;
            print_ast(p->object.get(), indent + 1);
            print_ast(p->index.get(), indent + 1);
            break;
        }
        case spy::NodeType::ASSIGN_STMT: {
            auto* p = static_cast<spy::AssignStmt*>(node);
            std::cout << pad << "AssignStmt" << std::endl;
            print_ast(p->target.get(), indent + 1);
            print_ast(p->value.get(), indent + 1);
            break;
        }
        case spy::NodeType::CLASS_STMT: {
            auto* p = static_cast<spy::ClassStmt*>(node);
            std::string plist;
            for (size_t i = 0; i < p->parents.size(); ++i) {
                if (i > 0) plist += ", ";
                plist += p->parents[i];
            }
            std::cout << pad << "ClassStmt(\"" << p->name << "\", parents=[" << plist << "], " << p->methods.size() << " methods)" << std::endl;
            for (auto& m : p->methods) print_ast(m.get(), indent + 1);
            break;
        }
        case spy::NodeType::MEMBER_EXPR: {
            auto* p = static_cast<spy::MemberExpr*>(node);
            std::cout << pad << "MemberExpr(\"" << p->member << "\")" << std::endl;
            print_ast(p->object.get(), indent + 1);
            break;
        }
        case spy::NodeType::METHOD_CALL_EXPR: {
            auto* p = static_cast<spy::MethodCallExpr*>(node);
            std::cout << pad << "MethodCallExpr(\"" << p->method << "\", " << p->args.size() << " args)" << std::endl;
            print_ast(p->object.get(), indent + 1);
            for (auto& a : p->args) print_ast(a.get(), indent + 1);
            break;
        }
        case spy::NodeType::SUPER_METHOD_CALL_EXPR: {
            auto* p = static_cast<spy::SuperMethodCallExpr*>(node);
            std::cout << pad << "SuperMethodCallExpr(\"" << p->method << "\", " << p->args.size() << " args)" << std::endl;
            for (auto& a : p->args) print_ast(a.get(), indent + 1);
            break;
        }
        case spy::NodeType::TRY_STMT: {
            auto* p = static_cast<spy::TryStmt*>(node);
            std::cout << pad << "TryStmt(" << p->handlers.size() << " handlers)" << std::endl;
            std::cout << pad << "  body:" << std::endl;
            for (auto& s : p->body) if (s) print_ast(s.get(), indent + 2);
            for (auto& h : p->handlers) {
                auto* handler = static_cast<spy::ExceptHandler*>(h.get());
                std::cout << pad << "  ExceptHandler(var=\"" << handler->var_name << "\"):" << std::endl;
                for (auto& s : handler->body) if (s) print_ast(s.get(), indent + 2);
            }
            break;
        }
        case spy::NodeType::BREAK_STMT:
            std::cout << pad << "BreakStmt" << std::endl;
            break;
        case spy::NodeType::CONTINUE_STMT:
            std::cout << pad << "ContinueStmt" << std::endl;
            break;
        case spy::NodeType::TERNARY_EXPR: {
            auto* p = static_cast<spy::TernaryExpr*>(node);
            std::cout << pad << "TernaryExpr" << std::endl;
            std::cout << pad << "  condition:" << std::endl;
            print_ast(p->condition.get(), indent + 2);
            std::cout << pad << "  then:" << std::endl;
            print_ast(p->then_expr.get(), indent + 2);
            std::cout << pad << "  else:" << std::endl;
            print_ast(p->else_expr.get(), indent + 2);
            break;
        }
        case spy::NodeType::ENUM_STMT: {
            auto* p = static_cast<spy::EnumStmt*>(node);
            std::cout << pad << "EnumStmt(\"" << p->name << "\", " << p->variants.size() << " variants)" << std::endl;
            for (auto& v : p->variants) std::cout << pad << "  " << v.name << "(" << v.fields.size() << " fields)" << std::endl;
            break;
        }
        case spy::NodeType::LIST_COMP_EXPR: {
            auto* p = static_cast<spy::ListCompExpr*>(node);
            std::cout << pad << "ListCompExpr(var=\"" << p->var << "\")" << std::endl;
            std::cout << pad << "  element:" << std::endl;
            print_ast(p->element.get(), indent + 2);
            std::cout << pad << "  iterable:" << std::endl;
            print_ast(p->iterable.get(), indent + 2);
            break;
        }
        case spy::NodeType::UNARY_EXPR: {
            auto* p = static_cast<spy::UnaryExpr*>(node);
            std::cout << pad << "UnaryExpr(op=\"" << p->op << "\")" << std::endl;
            std::cout << pad << "  operand:" << std::endl;
            print_ast(p->operand.get(), indent + 2);
            break;
        }
        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Spy Compiler v0.1" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  spy <file.spy>                       Compile and run (C backend)" << std::endl;
        std::cerr << "  spy <file.spy> --target cpp          Compile with C++ backend (SpyUI)" << std::endl;
        std::cerr << "  spy <file.spy> --output <name>       Compile to named executable" << std::endl;
        std::cerr << "  spy <file.spy> --ast                 Show AST" << std::endl;
        std::cerr << "  spy <file.spy> --tokens              Show tokens" << std::endl;
        std::cerr << "  spy <file.spy> --c                   Show generated C code" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::string output_name;
    std::string target = "c";
    std::string mode;
    std::vector<std::string> include_paths;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_name = argv[i + 1];
            i++;
        } else if (arg == "--target" && i + 1 < argc) {
            target = argv[i + 1];
            i++;
        } else if (arg == "--include-path" && i + 1 < argc) {
            include_paths.push_back(argv[i + 1]);
            i++;
        } else {
            mode = arg;
        }
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << filename << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    spy::Lexer lexer(source, filename);
    std::vector<spy::Token> tokens = lexer.tokenize();

    if (mode == "--tokens") {
        for (const auto& token : tokens) {
            std::cout << token << std::endl;
        }
        return 0;
    }

    spy::Parser parser(tokens);
    spy::ASTPtr ast;
    try {
        ast = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    }

    if (mode == "--ast") {
        print_ast(ast.get());
        return 0;
    }

    bool is_cpp = (target == "cpp");
    bool is_ir = (target == "ir");

    // Derive default search path from main file's directory
    std::string main_dir;
    size_t sep = filename.find_last_of("/\\");
    if (sep != std::string::npos) {
        main_dir = filename.substr(0, sep);
    }

    std::string generated_code;
    std::string error_msg;
    if (is_cpp) {
        spy::CppCodegen cpp_codegen;
        if (!main_dir.empty()) cpp_codegen.add_search_path(main_dir);
        for (auto& p : include_paths) cpp_codegen.add_search_path(p);
        generated_code = cpp_codegen.generate(ast.get());
        error_msg = cpp_codegen.get_last_error();
    } else if (is_ir) {
        spy::IrCodegen ir_codegen;
        generated_code = ir_codegen.generate(ast.get());
        error_msg = ir_codegen.get_last_error();
    } else {
        spy::Codegen codegen;
        if (!main_dir.empty()) codegen.add_search_path(main_dir);
        for (auto& p : include_paths) codegen.add_search_path(p);
        generated_code = codegen.generate(ast.get());
        error_msg = codegen.get_last_error();
    }

    if (!error_msg.empty()) {
        std::cerr << "Codegen error: " << error_msg << std::endl;
        return 1;
    }

    if (mode == "--c") {
        std::cout << generated_code;
        return 0;
    }

    std::string ext = is_ir ? ".ll" : (is_cpp ? ".cpp" : ".c");
    std::string src_file = "spy_output" + ext;
    std::string exe_file = "spy_output";

    if (!output_name.empty()) {
        exe_file = output_name;
    }

    std::ofstream out(src_file);
    out << generated_code;
    out.close();

    if (is_ir) {
        // Generate runtime C file containing Spy runtime helpers
        std::string runtime_file = "spy_ir_runtime.c";
        // Write a small runtime with needed helper functions
        std::ofstream rt_out(runtime_file);
        rt_out << R"(#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
const char* spy_substr(const char* s, int start, int len) {
    size_t slen = strlen(s);
    if (start < 0) start = 0;
    if ((size_t)start > slen) start = (int)slen;
    if (len < 0) len = 0;
    if ((size_t)(start + len) > slen) len = (int)(slen - start);
    char* r = (char*)malloc(len + 1);
    memcpy(r, s + start, len);
    r[len] = '\0';
    return r;
}
const char* spy_chr(int n) {
    char* r = (char*)malloc(2);
    r[0] = (char)n;
    r[1] = '\0';
    return r;
}
const char* spy_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (len > 0) fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}
const char* spy_strcat(const char* a, const char* b) {
    size_t alen = strlen(a), blen = strlen(b);
    char* r = (char*)malloc(alen + blen + 1);
    memcpy(r, a, alen);
    memcpy(r + alen, b, blen);
    r[alen + blen] = '\0';
    return r;
}
const char* spy_format(const char* fmt, ...) {
    static char bufs[8][4096];
    static int idx = 0;
    char* buf = bufs[idx & 7]; idx++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return buf;
}
void* spy_alloc(size_t size) { return malloc(size); }
double spy_strlen(const char* s) { return (double)strlen(s); }
const char* spy_str_from_double(double val) {
    char* r = (char*)malloc(64);
    snprintf(r, 64, "%g", val);
    return r;
}
)";
        rt_out.close();

        std::string compile_cmd;
        compile_cmd = "clang -o " + exe_file + " " + src_file + " " + runtime_file;
        int result = std::system(compile_cmd.c_str());
        if (result != 0) {
            std::cerr << "Compilation failed!" << std::endl;
            return 1;
        }
        if (output_name.empty()) {
            std::string run_cmd = exe_file;
            for (int i = 2; i < argc; ++i) {
                if (argv[i][0] != '-') {
                    run_cmd += " " + std::string(argv[i]);
                }
            }
            result = std::system(run_cmd.c_str());
            return result;
        }
        std::cerr << "Compiled to " << exe_file << std::endl;
        return 0;
    }

    std::string compile_cmd;
    if (is_cpp) {
        compile_cmd = "clang++ -std=c++17 -Ispyui -o " + exe_file + " " + src_file + " spyui/spyui.cpp";
    } else {
        compile_cmd = "clang -o " + exe_file + " " + src_file;
    }
    int result = std::system(compile_cmd.c_str());

    if (result != 0) {
        std::cerr << "Compilation failed!" << std::endl;
        return 1;
    }

    if (!output_name.empty()) {
        std::cerr << "Compiled to " << exe_file << std::endl;
        return 0;
    }

    std::string run_cmd = "./" + exe_file;
    for (int i = 2; i < argc; ++i) {
        if (argv[i][0] != '-') {
            run_cmd += " " + std::string(argv[i]);
        }
    }
    result = std::system(run_cmd.c_str());

    return result;
}
