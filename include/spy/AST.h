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

#ifndef SPY_AST_H
#define SPY_AST_H

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace spy {

enum class NodeType {
    PROGRAM,
    PRINT_STMT,
    LET_STMT,
    FN_STMT,
    RETURN_STMT,
    IF_STMT,
    WHILE_STMT,
    EXPR_STMT,
    INT_EXPR,
    FLOAT_EXPR,
    STRING_EXPR,
    IDENT_EXPR,
    BOOL_EXPR,
    NONE_EXPR,
    BINOP_EXPR,
    CALL_EXPR,
    IMPORT_STMT,
    PIPE_EXPR,
    MATCH_STMT,
    CONSTRUCTOR_PATTERN,
    FOR_STMT,
    ARRAY_EXPR,
    INDEX_EXPR,
    ASSIGN_STMT,
    CLASS_STMT,
    MEMBER_EXPR,
    METHOD_CALL_EXPR,
    SUPER_EXPR,
    SUPER_METHOD_CALL_EXPR,
    DICT_EXPR,
    DICT_ACCESS_EXPR,
    DICT_SET_EXPR,
    SET_EXPR,
    NAMED_TUPLE_EXPR,
    LAMBDA_EXPR,
    TRY_STMT,
    EXCEPT_HANDLER,
    BREAK_STMT,
    CONTINUE_STMT,
    TERNARY_EXPR,
    ENUM_STMT,
    LIST_COMP_EXPR,
    UNARY_EXPR,
    GLOBAL_STMT,
    YIELD_STMT,
    TYPE_EXPR,
    STRUCT_STMT,
    ADDRESS_OF_EXPR,
    DEREF_EXPR,
    SIZEOF_EXPR,
    CAST_EXPR,
    ARRAY_TYPE_EXPR,
    PTR_TYPE_EXPR,
    EXTERN_FN_STMT,
};

struct ASTNode {
    NodeType type;
    int line;
    int column;

    ASTNode(NodeType t, int l, int c) : type(t), line(l), column(c) {}
    virtual ~ASTNode() = default;
};

using ASTPtr = std::unique_ptr<ASTNode>;

struct Program : ASTNode {
    std::vector<ASTPtr> statements;
    Program() : ASTNode(NodeType::PROGRAM, 0, 0) {}
};

struct PrintStmt : ASTNode {
    std::vector<ASTPtr> expressions;
    PrintStmt(std::vector<ASTPtr> exprs, int l, int c)
        : ASTNode(NodeType::PRINT_STMT, l, c), expressions(std::move(exprs)) {}
};

struct LetStmt : ASTNode {
    std::string name;
    ASTPtr type_annotation;
    ASTPtr initializer;
    LetStmt(const std::string& n, ASTPtr type, ASTPtr init, int l, int c)
        : ASTNode(NodeType::LET_STMT, l, c), name(n), type_annotation(std::move(type)), initializer(std::move(init)) {}
};

struct GlobalStmt : ASTNode {
    std::string name;
    GlobalStmt(const std::string& n, int l, int c)
        : ASTNode(NodeType::GLOBAL_STMT, l, c), name(n) {}
};

struct FnParam {
    std::string name;
    ASTPtr type;
    FnParam(const std::string& n, ASTPtr t) : name(n), type(std::move(t)) {}
};

struct FnStmt : ASTNode {
    std::string name;
    std::vector<FnParam> typed_params;
    std::vector<std::string> params;
    ASTPtr return_type;
    std::vector<ASTPtr> body;
    std::map<std::string, ASTPtr> defaults;
    std::string decorator;
    FnStmt(const std::string& n, std::vector<FnParam> tp, std::vector<std::string> p, ASTPtr rt, std::vector<ASTPtr> b, std::map<std::string, ASTPtr> d, const std::string& dec, int l, int c)
        : ASTNode(NodeType::FN_STMT, l, c), name(n), typed_params(std::move(tp)), params(std::move(p)), return_type(std::move(rt)), body(std::move(b)), defaults(std::move(d)), decorator(dec) {}
};

struct ReturnStmt : ASTNode {
    ASTPtr expression;
    ReturnStmt(ASTPtr expr, int l, int c)
        : ASTNode(NodeType::RETURN_STMT, l, c), expression(std::move(expr)) {}
};

struct IfStmt : ASTNode {
    ASTPtr condition;
    std::vector<ASTPtr> then_body;
    std::vector<ASTPtr> else_body;
    bool has_else = false;
    IfStmt(ASTPtr cond, std::vector<ASTPtr> then_b, std::vector<ASTPtr> else_b, bool he, int l, int c)
        : ASTNode(NodeType::IF_STMT, l, c), condition(std::move(cond)),
          then_body(std::move(then_b)), else_body(std::move(else_b)), has_else(he) {}
};

struct WhileStmt : ASTNode {
    ASTPtr condition;
    std::vector<ASTPtr> body;
    std::vector<ASTPtr> else_body;
    WhileStmt(ASTPtr cond, std::vector<ASTPtr> b, std::vector<ASTPtr> eb, int l, int c)
        : ASTNode(NodeType::WHILE_STMT, l, c), condition(std::move(cond)), body(std::move(b)), else_body(std::move(eb)) {}
};

struct ExprStmt : ASTNode {
    ASTPtr expression;
    ExprStmt(ASTPtr expr, int l, int c)
        : ASTNode(NodeType::EXPR_STMT, l, c), expression(std::move(expr)) {}
};

struct ImportStmt : ASTNode {
    std::string header;
    std::string module_name;
    std::string alias_name;
    std::vector<std::string> names;
    bool is_c_header;
    ImportStmt(const std::string& h, int l, int c)
        : ASTNode(NodeType::IMPORT_STMT, l, c), header(h), is_c_header(true) {}
    ImportStmt(const std::string& mod, const std::vector<std::string>& n, const std::string& alias, int l, int c)
        : ASTNode(NodeType::IMPORT_STMT, l, c), module_name(mod), alias_name(alias), names(n), is_c_header(false) {}
};

struct IntExpr : ASTNode {
    long long value;
    IntExpr(long long v, int l, int c)
        : ASTNode(NodeType::INT_EXPR, l, c), value(v) {}
};

struct FloatExpr : ASTNode {
    double value;
    FloatExpr(double v, int l, int c)
        : ASTNode(NodeType::FLOAT_EXPR, l, c), value(v) {}
};

struct StringExpr : ASTNode {
    std::string value;
    StringExpr(const std::string& v, int l, int c)
        : ASTNode(NodeType::STRING_EXPR, l, c), value(v) {}
};

struct IdentExpr : ASTNode {
    std::string name;
    IdentExpr(const std::string& n, int l, int c)
        : ASTNode(NodeType::IDENT_EXPR, l, c), name(n) {}
};

struct BoolExpr : ASTNode {
    bool value;
    BoolExpr(bool v, int l, int c)
        : ASTNode(NodeType::BOOL_EXPR, l, c), value(v) {}
};

struct NoneExpr : ASTNode {
    NoneExpr(int l, int c) : ASTNode(NodeType::NONE_EXPR, l, c) {}
};

struct BinOpExpr : ASTNode {
    std::string op;
    ASTPtr left;
    ASTPtr right;
    BinOpExpr(const std::string& o, ASTPtr l, ASTPtr r, int line, int col)
        : ASTNode(NodeType::BINOP_EXPR, line, col), op(o), left(std::move(l)), right(std::move(r)) {}
};

struct CallExpr : ASTNode {
    std::string callee;
    std::vector<ASTPtr> args;
    CallExpr(const std::string& fn, std::vector<ASTPtr> a, int l, int c)
        : ASTNode(NodeType::CALL_EXPR, l, c), callee(fn), args(std::move(a)) {}
};

struct PipeExpr : ASTNode {
    ASTPtr value;
    ASTPtr call;
    PipeExpr(ASTPtr v, ASTPtr c, int l, int expr_col)
        : ASTNode(NodeType::PIPE_EXPR, l, expr_col), value(std::move(v)), call(std::move(c)) {}
};

struct MatchCase {
    ASTPtr pattern;
    std::vector<ASTPtr> body;
    MatchCase(ASTPtr p, std::vector<ASTPtr> b) : pattern(std::move(p)), body(std::move(b)) {}
};

struct MatchStmt : ASTNode {
    ASTPtr value;
    std::vector<MatchCase> cases;
    int default_index = -1;
    MatchStmt(ASTPtr v, std::vector<MatchCase> cs, int def, int l, int c)
        : ASTNode(NodeType::MATCH_STMT, l, c), value(std::move(v)), cases(std::move(cs)), default_index(def) {}
};

struct ForStmt : ASTNode {
    std::string var;
    ASTPtr start;
    ASTPtr end;
    ASTPtr iterable;
    std::vector<ASTPtr> body;
    std::vector<ASTPtr> else_body;
    ForStmt(const std::string& v, ASTPtr s, ASTPtr e, std::vector<ASTPtr> b, std::vector<ASTPtr> eb, int l, int c)
        : ASTNode(NodeType::FOR_STMT, l, c), var(v), start(std::move(s)), end(std::move(e)), body(std::move(b)), else_body(std::move(eb)) {}
    ForStmt(const std::string& v, ASTPtr iter, std::vector<ASTPtr> b, std::vector<ASTPtr> eb, int l, int c)
        : ASTNode(NodeType::FOR_STMT, l, c), var(v), iterable(std::move(iter)), body(std::move(b)), else_body(std::move(eb)) {}
};

struct ArrayExpr : ASTNode {
    std::vector<ASTPtr> elements;
    ArrayExpr(std::vector<ASTPtr> e, int l, int c)
        : ASTNode(NodeType::ARRAY_EXPR, l, c), elements(std::move(e)) {}
};

struct IndexExpr : ASTNode {
    ASTPtr object;
    ASTPtr index;
    IndexExpr(ASTPtr obj, ASTPtr idx, int l, int c)
        : ASTNode(NodeType::INDEX_EXPR, l, c), object(std::move(obj)), index(std::move(idx)) {}
};

struct AssignStmt : ASTNode {
    ASTPtr target;
    ASTPtr value;
    AssignStmt(ASTPtr t, ASTPtr v, int l, int c)
        : ASTNode(NodeType::ASSIGN_STMT, l, c), target(std::move(t)), value(std::move(v)) {}
};

struct ClassStmt : ASTNode {
    std::string name;
    std::vector<std::string> parents;
    std::vector<ASTPtr> methods;
    ClassStmt(const std::string& n, const std::vector<std::string>& p, std::vector<ASTPtr> m, int l, int c)
        : ASTNode(NodeType::CLASS_STMT, l, c), name(n), parents(p), methods(std::move(m)) {}
};

struct MemberExpr : ASTNode {
    ASTPtr object;
    std::string member;
    MemberExpr(ASTPtr obj, const std::string& m, int l, int c)
        : ASTNode(NodeType::MEMBER_EXPR, l, c), object(std::move(obj)), member(m) {}
};

struct MethodCallExpr : ASTNode {
    ASTPtr object;
    std::string method;
    std::vector<ASTPtr> args;
    MethodCallExpr(ASTPtr obj, const std::string& m, std::vector<ASTPtr> a, int l, int c)
        : ASTNode(NodeType::METHOD_CALL_EXPR, l, c), object(std::move(obj)), method(m), args(std::move(a)) {}
};

struct SuperExpr : ASTNode {
    SuperExpr(int l, int c) : ASTNode(NodeType::SUPER_EXPR, l, c) {}
};

struct SuperMethodCallExpr : ASTNode {
    std::string method;
    std::vector<ASTPtr> args;
    SuperMethodCallExpr(const std::string& m, std::vector<ASTPtr> a, int l, int c)
        : ASTNode(NodeType::SUPER_METHOD_CALL_EXPR, l, c), method(m), args(std::move(a)) {}
};

struct DictExpr : ASTNode {
    std::vector<ASTPtr> keys;
    std::vector<ASTPtr> values;
    DictExpr(std::vector<ASTPtr> k, std::vector<ASTPtr> v, int l, int c)
        : ASTNode(NodeType::DICT_EXPR, l, c), keys(std::move(k)), values(std::move(v)) {}
};

struct SetExpr : ASTNode {
    std::vector<ASTPtr> elements;
    SetExpr(std::vector<ASTPtr> e, int l, int c)
        : ASTNode(NodeType::SET_EXPR, l, c), elements(std::move(e)) {}
};

struct NamedTupleExpr : ASTNode {
    std::vector<std::string> field_names;
    std::vector<ASTPtr> field_values;
    NamedTupleExpr(std::vector<std::string> names, std::vector<ASTPtr> vals, int l, int c)
        : ASTNode(NodeType::NAMED_TUPLE_EXPR, l, c), field_names(std::move(names)), field_values(std::move(vals)) {}
};

struct DictAccessExpr : ASTNode {
    ASTPtr object;
    ASTPtr key;
    DictAccessExpr(ASTPtr obj, ASTPtr k, int l, int c)
        : ASTNode(NodeType::DICT_ACCESS_EXPR, l, c), object(std::move(obj)), key(std::move(k)) {}
};

struct DictSetExpr : ASTNode {
    ASTPtr object;
    ASTPtr key;
    ASTPtr value;
    DictSetExpr(ASTPtr obj, ASTPtr k, ASTPtr v, int l, int c)
        : ASTNode(NodeType::DICT_SET_EXPR, l, c), object(std::move(obj)), key(std::move(k)), value(std::move(v)) {}
};

struct LambdaExpr : ASTNode {
    std::vector<std::string> params;
    std::vector<ASTPtr> body;
    LambdaExpr(std::vector<std::string> p, std::vector<ASTPtr> b, int l, int c)
        : ASTNode(NodeType::LAMBDA_EXPR, l, c), params(std::move(p)), body(std::move(b)) {}
};

struct ExceptHandler : ASTNode {
    std::string var_name;
    std::vector<ASTPtr> body;
    ExceptHandler(const std::string& v, std::vector<ASTPtr> b, int l, int c)
        : ASTNode(NodeType::EXCEPT_HANDLER, l, c), var_name(v), body(std::move(b)) {}
};

struct TryStmt : ASTNode {
    std::vector<ASTPtr> body;
    std::vector<ASTPtr> handlers;
    TryStmt(std::vector<ASTPtr> b, std::vector<ASTPtr> h, int l, int c)
        : ASTNode(NodeType::TRY_STMT, l, c), body(std::move(b)), handlers(std::move(h)) {}
};

struct BreakStmt : ASTNode {
    BreakStmt(int l, int c) : ASTNode(NodeType::BREAK_STMT, l, c) {}
};

struct ContinueStmt : ASTNode {
    ContinueStmt(int l, int c) : ASTNode(NodeType::CONTINUE_STMT, l, c) {}
};

struct YieldStmt : ASTNode {
    ASTPtr value;
    YieldStmt(ASTPtr v, int l, int c) : ASTNode(NodeType::YIELD_STMT, l, c), value(std::move(v)) {}
};

struct TernaryExpr : ASTNode {
    ASTPtr condition;
    ASTPtr then_expr;
    ASTPtr else_expr;
    TernaryExpr(ASTPtr cond, ASTPtr then_e, ASTPtr else_e, int l, int c)
        : ASTNode(NodeType::TERNARY_EXPR, l, c), condition(std::move(cond)), then_expr(std::move(then_e)), else_expr(std::move(else_e)) {}
};

struct EnumField {
    std::string name;
    std::string type_name;
    EnumField(const std::string& n, const std::string& t) : name(n), type_name(t) {}
};

struct EnumVariant {
    std::string name;
    std::vector<EnumField> fields;
    bool has_payload;
    EnumVariant(const std::string& n, const std::vector<EnumField>& f)
        : name(n), fields(f), has_payload(!f.empty()) {}
};

struct EnumStmt : ASTNode {
    std::string name;
    std::vector<EnumVariant> variants;
    EnumStmt(const std::string& n, std::vector<EnumVariant> v, int l, int c)
        : ASTNode(NodeType::ENUM_STMT, l, c), name(n), variants(std::move(v)) {}
};

struct ConstructorPattern : ASTNode {
    std::string enum_name;
    std::string variant_name;
    std::vector<std::string> bindings;
    ConstructorPattern(const std::string& en, const std::string& vn, std::vector<std::string> b, int l, int c)
        : ASTNode(NodeType::CONSTRUCTOR_PATTERN, l, c), enum_name(en), variant_name(vn), bindings(std::move(b)) {}
};

struct ListCompExpr : ASTNode {
    ASTPtr element;
    std::string var;
    ASTPtr iterable;
    ListCompExpr(ASTPtr e, const std::string& v, ASTPtr iter, int l, int c)
        : ASTNode(NodeType::LIST_COMP_EXPR, l, c), element(std::move(e)), var(v), iterable(std::move(iter)) {}
};

struct UnaryExpr : public ASTNode {
    std::string op;
    ASTPtr operand;
    UnaryExpr(const std::string& o, ASTPtr operand, int l, int c)
        : ASTNode(NodeType::UNARY_EXPR, l, c), op(o), operand(std::move(operand)) {}
};

struct TypeExpr : ASTNode {
    std::string name;
    TypeExpr(const std::string& n, int l, int c)
        : ASTNode(NodeType::TYPE_EXPR, l, c), name(n) {}
};

struct PtrTypeExpr : ASTNode {
    ASTPtr base_type;
    PtrTypeExpr(ASTPtr base, int l, int c)
        : ASTNode(NodeType::PTR_TYPE_EXPR, l, c), base_type(std::move(base)) {}
};

struct ArrayTypeExpr : ASTNode {
    ASTPtr element_type;
    ASTPtr size;
    ArrayTypeExpr(ASTPtr et, ASTPtr sz, int l, int c)
        : ASTNode(NodeType::ARRAY_TYPE_EXPR, l, c), element_type(std::move(et)), size(std::move(sz)) {}
};

struct StructField {
    std::string name;
    ASTPtr type;
    StructField(const std::string& n, ASTPtr t) : name(n), type(std::move(t)) {}
};

struct StructStmt : ASTNode {
    std::string name;
    std::vector<StructField> fields;
    StructStmt(const std::string& n, std::vector<StructField> f, int l, int c)
        : ASTNode(NodeType::STRUCT_STMT, l, c), name(n), fields(std::move(f)) {}
};

struct ExternFnStmt : ASTNode {
    std::string name;
    std::vector<FnParam> typed_params;
    ASTPtr return_type;
    std::string header;
    ExternFnStmt(const std::string& n, std::vector<FnParam> tp, ASTPtr rt, const std::string& h, int l, int c)
        : ASTNode(NodeType::EXTERN_FN_STMT, l, c), name(n), typed_params(std::move(tp)), return_type(std::move(rt)), header(h) {}
};

struct AddressOfExpr : ASTNode {
    ASTPtr operand;
    AddressOfExpr(ASTPtr op, int l, int c)
        : ASTNode(NodeType::ADDRESS_OF_EXPR, l, c), operand(std::move(op)) {}
};

struct DerefExpr : ASTNode {
    ASTPtr operand;
    DerefExpr(ASTPtr op, int l, int c)
        : ASTNode(NodeType::DEREF_EXPR, l, c), operand(std::move(op)) {}
};

struct SizeofExpr : ASTNode {
    ASTPtr type;
    SizeofExpr(ASTPtr t, int l, int c)
        : ASTNode(NodeType::SIZEOF_EXPR, l, c), type(std::move(t)) {}
};

struct CastExpr : ASTNode {
    ASTPtr target_type;
    ASTPtr value;
    CastExpr(ASTPtr t, ASTPtr v, int l, int c)
        : ASTNode(NodeType::CAST_EXPR, l, c), target_type(std::move(t)), value(std::move(v)) {}
};

} // namespace spy

#endif // SPY_AST_H
