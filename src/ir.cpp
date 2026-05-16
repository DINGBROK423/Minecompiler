#include "ir.h"

#include "Constant.h"
#include "Function.h"
#include "GlobalVariable.h"
#include "IRbuilder.h"
#include "Module.h"

#include <cmath>
#include <iomanip>

namespace {

std::vector<AstPtr> collect_kind(const AstPtr& node, const std::string& kind) {
    std::vector<AstPtr> out;
    if (!node) return out;
    if (node->kind == kind) out.push_back(node);
    for (const auto& ch : node->children) {
        auto more = collect_kind(ch, kind);
        out.insert(out.end(), more.begin(), more.end());
    }
    return out;
}

bool has_kind(const AstPtr& node, const std::string& kind) {
    if (!node) return false;
    if (node->kind == kind) return true;
    for (const auto& ch : node->children) {
        if (has_kind(ch, kind)) return true;
    }
    return false;
}

bool uses_float(const AstPtr& root) {
    if (has_kind(root, "floatConst")) return true;
    for (const auto& btype : collect_kind(root, "bType")) {
        if (!btype->children.empty() && btype->children.front()->text == "float") return true;
    }
    return false;
}

void append_direct_call_args(const AstPtr& node, std::vector<AstPtr>& args) {
    if (!node) return;
    if (node->kind == "funcRParam") {
        for (const auto& ch : node->children) {
            if (ch->kind == "exp") {
                args.push_back(ch);
                return;
            }
        }
        return;
    }
    if (node->kind == "funcRParamsOpt" || node->kind == "funcRParams" || node->kind == "funcRParamsTail") {
        for (const auto& ch : node->children) append_direct_call_args(ch, args);
    }
}

std::vector<AstPtr> direct_call_args(const AstPtr& funcRParamsOpt) {
    std::vector<AstPtr> args;
    append_direct_call_args(funcRParamsOpt, args);
    return args;
}

void append_block_items(const AstPtr& node, std::vector<AstPtr>& items) {
    if (!node) return;
    if (node->kind == "blockItem") {
        items.push_back(node);
        return;
    }
    if (node->kind == "blockItemList") {
        for (const auto& ch : node->children) append_block_items(ch, items);
    }
}

std::vector<AstPtr> direct_block_items(const AstPtr& block) {
    std::vector<AstPtr> items;
    if (!block) return items;
    for (const auto& ch : block->children) {
        if (ch->kind == "blockItemList") append_block_items(ch, items);
    }
    return items;
}

std::string node_text(const AstPtr& node) {
    if (!node) return "";
    if (!node->text.empty()) return node->text;
    for (const auto& ch : node->children) {
        std::string t = node_text(ch);
        if (!t.empty()) return t;
    }
    return "";
}

std::string c_type_of(const AstPtr& type_node) {
    std::string t = node_text(type_node);
    if (t == "float") return "float";
    if (t == "void") return "void";
    return "int";
}

AstPtr direct_child(const AstPtr& node, const std::string& kind) {
    if (!node) return nullptr;
    for (const auto& ch : node->children) {
        if (ch->kind == kind) return ch;
    }
    return nullptr;
}

std::string direct_ident_name(const AstPtr& node) {
    if (!node) return "";
    if (node->kind == "Ident") return node->text;
    if (node->kind == "lVal") {
        for (const auto& ch : node->children) {
            if (ch->kind == "Ident") return ch->text;
        }
    }
    return "";
}

int eval_int_const_expr(const AstPtr& expr, const std::map<std::string, int>& constants = {});
double eval_float_const_expr(const AstPtr& expr,
                             const std::map<std::string, int>& int_constants = {},
                             const std::map<std::string, double>& float_constants = {});

int eval_int_leafy(const AstPtr& node, const std::map<std::string, int>& constants) {
    if (!node) return 0;
    if (node->kind == "IntConst") return std::stoi(node->text);
    if (node->kind == "floatConst") return static_cast<int>(std::stod(node->text));
    std::string name = direct_ident_name(node);
    if (!name.empty()) {
        auto it = constants.find(name);
        return it == constants.end() ? 0 : it->second;
    }
    if (node->children.size() == 1) return eval_int_const_expr(node->children[0], constants);
    return eval_int_const_expr(node, constants);
}

int eval_int_const_expr(const AstPtr& expr, const std::map<std::string, int>& constants) {
    if (!expr) return 0;
    if (expr->kind == "IntConst") return std::stoi(expr->text);
    if (expr->kind == "floatConst") return static_cast<int>(std::stod(expr->text));
    std::string name = direct_ident_name(expr);
    if (!name.empty()) {
        auto it = constants.find(name);
        return it == constants.end() ? 0 : it->second;
    }
    if (expr->children.size() == 1) return eval_int_const_expr(expr->children[0], constants);
    if (expr->children.size() == 2 && expr->children[0]->kind == "unaryOp") {
        std::string op = node_text(expr->children[0]);
        int v = eval_int_const_expr(expr->children[1], constants);
        if (op == "-") return -v;
        if (op == "!") return !v;
        return v;
    }
    if (expr->children.size() == 3 && expr->children[0]->text == "(" && expr->children[2]->text == ")") {
        return eval_int_const_expr(expr->children[1], constants);
    }
    if (expr->children.size() == 3) {
        int lhs = eval_int_const_expr(expr->children[0], constants);
        int rhs = eval_int_const_expr(expr->children[2], constants);
        std::string op = node_text(expr->children[1]);
        if (op == "+") return lhs + rhs;
        if (op == "-") return lhs - rhs;
        if (op == "*") return lhs * rhs;
        if (op == "/") return rhs == 0 ? 0 : lhs / rhs;
        if (op == "%") return rhs == 0 ? 0 : lhs % rhs;
        if (op == "<") return lhs < rhs;
        if (op == ">") return lhs > rhs;
        if (op == "<=") return lhs <= rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == "&&") return lhs && rhs;
        if (op == "||") return lhs || rhs;
    }
    for (const auto& ch : expr->children) {
        if (ch->kind != "(" && ch->kind != ")" && ch->kind != "," && ch->kind != ";") {
            return eval_int_leafy(ch, constants);
        }
    }
    return 0;
}

double eval_float_leafy(const AstPtr& node,
                        const std::map<std::string, int>& int_constants,
                        const std::map<std::string, double>& float_constants) {
    if (!node) return 0.0;
    if (node->kind == "IntConst") return std::stod(node->text);
    if (node->kind == "floatConst") return std::stod(node->text);
    std::string name = direct_ident_name(node);
    if (!name.empty()) {
        auto fit = float_constants.find(name);
        if (fit != float_constants.end()) return fit->second;
        auto iit = int_constants.find(name);
        return iit == int_constants.end() ? 0.0 : static_cast<double>(iit->second);
    }
    if (node->children.size() == 1) return eval_float_const_expr(node->children[0], int_constants, float_constants);
    return eval_float_const_expr(node, int_constants, float_constants);
}

double eval_float_const_expr(const AstPtr& expr,
                             const std::map<std::string, int>& int_constants,
                             const std::map<std::string, double>& float_constants) {
    if (!expr) return 0.0;
    if (expr->kind == "IntConst") return std::stod(expr->text);
    if (expr->kind == "floatConst") return std::stod(expr->text);
    std::string name = direct_ident_name(expr);
    if (!name.empty()) {
        auto fit = float_constants.find(name);
        if (fit != float_constants.end()) return fit->second;
        auto iit = int_constants.find(name);
        return iit == int_constants.end() ? 0.0 : static_cast<double>(iit->second);
    }
    if (expr->children.size() == 1) return eval_float_const_expr(expr->children[0], int_constants, float_constants);
    if (expr->children.size() == 2 && expr->children[0]->kind == "unaryOp") {
        std::string op = node_text(expr->children[0]);
        double v = eval_float_const_expr(expr->children[1], int_constants, float_constants);
        if (op == "-") return -v;
        if (op == "!") return !v;
        return v;
    }
    if (expr->children.size() == 3 && expr->children[0]->text == "(" && expr->children[2]->text == ")") {
        return eval_float_const_expr(expr->children[1], int_constants, float_constants);
    }
    if (expr->children.size() == 3) {
        double lhs = eval_float_const_expr(expr->children[0], int_constants, float_constants);
        double rhs = eval_float_const_expr(expr->children[2], int_constants, float_constants);
        std::string op = node_text(expr->children[1]);
        if (op == "+") return lhs + rhs;
        if (op == "-") return lhs - rhs;
        if (op == "*") return lhs * rhs;
        if (op == "/") return rhs == 0.0 ? 0.0 : lhs / rhs;
        if (op == "%") return rhs == 0.0 ? 0.0 : std::fmod(lhs, rhs);
        if (op == "<") return lhs < rhs;
        if (op == ">") return lhs > rhs;
        if (op == "<=") return lhs <= rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == "&&") return lhs && rhs;
        if (op == "||") return lhs || rhs;
    }
    for (const auto& ch : expr->children) {
        if (ch->kind != "(" && ch->kind != ")" && ch->kind != "," && ch->kind != ";") {
            return eval_float_leafy(ch, int_constants, float_constants);
        }
    }
    return 0.0;
}

std::string format_float_const(double value) {
    std::ostringstream os;
    os << std::scientific << std::setprecision(6) << value;
    return os.str();
}

struct MiddleValue {
    Value* value = nullptr;
    Type* type = nullptr;
};

struct MiddleBinding {
    Value* ptr = nullptr;
    Type* type = nullptr;
    bool is_const = false;
};

class CompilerIRBackend {
public:
    CompilerIRBackend(const AstPtr& root, std::string source_name)
        : root_(root), source_name_(std::move(source_name)), module_("sysy2022_compiler") {}

    IRResult generate() {
        IRResult result;
        enter_scope();
        declare_runtime();
        predeclare_functions();
        emit_globals();
        emit_functions();
        std::ostringstream text;
        text << "; ModuleID = 'sysy2022_compiler'\n";
        text << "source_filename = \"" << source_name_ << "\"\n\n";
        text << module_.print();
        result.text = text.str();
        return result;
    }

private:
    AstPtr root_;
    std::string source_name_;
    Module module_;
    std::unique_ptr<IRBuilder> builder_;
    Function* current_function_ = nullptr;
    Type* current_return_type_ = nullptr;
    int local_id_ = 0;
    int block_id_ = 0;
    std::map<std::string, Function*> functions_;
    std::vector<std::map<std::string, MiddleBinding>> scopes_;
    std::map<std::string, int> const_values_;

    Type* int_type() { return module_.get_int32_type(); }
    Type* bool_type() { return module_.get_int1_type(); }
    Type* void_type() { return module_.get_void_type(); }

    Type* ir_type(const std::string& ctype) {
        return ctype == "void" ? void_type() : int_type();
    }

    ConstantInt* i32(int value) { return ConstantInt::get(value, &module_); }
    ConstantInt* i1(bool value) { return ConstantInt::get(value, &module_); }

    void enter_scope() {
        scopes_.push_back({});
    }

    void leave_scope() {
        if (!scopes_.empty()) scopes_.pop_back();
    }

    void bind(const std::string& name, Value* ptr, Type* type, bool is_const = false) {
        if (scopes_.empty()) enter_scope();
        scopes_.back()[name] = {ptr, type, is_const};
    }

    MiddleBinding lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return {};
    }

    std::string unique_local(const std::string& name) {
        return name + "." + std::to_string(local_id_++);
    }

    std::string unique_block(const std::string& base) {
        return base + std::to_string(block_id_++);
    }

    void declare_runtime() {
        create_function_decl("getint", int_type(), {});
        create_function_decl("getch", int_type(), {});
        create_function_decl("getarray", int_type(), {module_.get_int32_ptr_type()});
        create_function_decl("putint", void_type(), {int_type()});
        create_function_decl("putch", void_type(), {int_type()});
        create_function_decl("putarray", void_type(), {int_type(), module_.get_int32_ptr_type()});
        create_function_decl("starttime", void_type(), {});
        create_function_decl("stoptime", void_type(), {});
    }

    Function* create_function_decl(const std::string& name, Type* ret, const std::vector<Type*>& params) {
        auto* fn_type = FunctionType::get(ret, params);
        auto* fn = Function::create(fn_type, name, &module_);
        functions_[name] = fn;
        return fn;
    }

    void predeclare_functions() {
        for (const auto& item : collect_kind(root_, "compUnitItem")) {
            if (item->children.empty() || item->children.front()->kind != "funcDef") continue;
            AstPtr fn_node = item->children.front();
            auto names = collect_kind(fn_node, "funcName");
            if (names.empty()) continue;
            std::vector<Type*> params;
            for (size_t i = 0; i < collect_kind(fn_node, "funcFParam").size(); ++i) params.push_back(int_type());
            Type* ret = fn_node->children.empty() ? int_type() : ir_type(c_type_of(fn_node->children.front()));
            create_function_decl(node_text(names.front()), ret, params);
        }
    }

    void emit_globals() {
        for (const auto& item : collect_kind(root_, "compUnitItem")) {
            if (item->children.empty() || item->children.front()->kind != "decl") continue;
            emit_global_decl(item->children.front());
        }
    }

    void emit_functions() {
        for (const auto& item : collect_kind(root_, "compUnitItem")) {
            if (item->children.empty() || item->children.front()->kind != "funcDef") continue;
            emit_function(item->children.front());
        }
    }

    void emit_global_decl(const AstPtr& decl) {
        if (!decl || decl->children.empty()) return;
        AstPtr real = decl->children.front();
        bool is_const = real->kind == "constDecl";
        auto defs = collect_kind(real, is_const ? "constDef" : "varDef");
        for (const auto& def : defs) {
            auto ids = collect_kind(def, "Ident");
            if (ids.empty()) continue;
            int init = 0;
            auto init_node = collect_kind(def, is_const ? "constInitVal" : "initVal");
            if (!init_node.empty()) init = eval_int_const_expr(init_node.front(), const_values_);
            auto* gv = GlobalVariable::create(ids.front()->text, &module_, int_type(), is_const, i32(init));
            bind(ids.front()->text, gv, int_type(), is_const);
            if (is_const) const_values_[ids.front()->text] = init;
        }
    }

    void emit_function(const AstPtr& func) {
        auto names = collect_kind(func, "funcName");
        if (names.empty()) return;
        std::string name = node_text(names.front());
        auto found = functions_.find(name);
        if (found == functions_.end()) return;
        current_function_ = found->second;
        current_return_type_ = current_function_->get_return_type();
        local_id_ = 0;
        BasicBlock* entry = BasicBlock::create(&module_, name + "_ENTRY", current_function_);
        builder_ = std::make_unique<IRBuilder>(entry, &module_);
        builder_->set_curFunc(current_function_);

        enter_scope();
        std::vector<std::string> param_names;
        for (const auto& p : collect_kind(func, "funcFParam")) {
            auto ids = collect_kind(p, "Ident");
            param_names.push_back(ids.empty() ? "arg" + std::to_string(param_names.size()) : ids.front()->text);
        }
        size_t idx = 0;
        for (auto it = current_function_->arg_begin(); it != current_function_->arg_end(); ++it, ++idx) {
            std::string pname = idx < param_names.size() ? param_names[idx] : "arg" + std::to_string(idx);
            (*it)->set_name(pname + ".arg");
            auto* ptr = builder_->create_alloca(int_type());
            ptr->set_name(unique_local(pname));
            builder_->create_store(*it, ptr);
            bind(pname, ptr, int_type());
        }

        auto blocks = collect_kind(func, "block");
        if (!blocks.empty()) emit_block(blocks.front());
        ensure_function_return();
        leave_scope();
        builder_.reset();
        current_function_ = nullptr;
        current_return_type_ = nullptr;
    }

    void ensure_function_return() {
        if (!builder_ || builder_->get_insert_block()->get_terminator()) return;
        if (current_return_type_ && current_return_type_->is_void_type()) builder_->create_void_ret();
        else builder_->create_ret(i32(0));
    }

    void emit_block(const AstPtr& block) {
        enter_scope();
        for (const auto& item : direct_block_items(block)) {
            if (!builder_ || builder_->get_insert_block()->get_terminator()) break;
            if (item->children.empty()) continue;
            if (item->children.front()->kind == "decl") emit_local_decl(item->children.front());
            else emit_stmt(item->children.front());
        }
        leave_scope();
    }

    void emit_local_decl(const AstPtr& decl) {
        if (!decl || decl->children.empty()) return;
        AstPtr real = decl->children.front();
        bool is_const = real->kind == "constDecl";
        auto defs = collect_kind(real, is_const ? "constDef" : "varDef");
        for (const auto& def : defs) {
            auto ids = collect_kind(def, "Ident");
            if (ids.empty()) continue;
            std::string name = ids.front()->text;
            auto* ptr = builder_->create_alloca(int_type());
            ptr->set_name(unique_local(name));
            bind(name, ptr, int_type(), is_const);
            auto init = collect_kind(def, is_const ? "constInitVal" : "initVal");
            if (!init.empty()) {
                MiddleValue val = emit_expr(init.front());
                builder_->create_store(as_i32(val), ptr);
            }
        }
    }

    void emit_stmt(const AstPtr& stmt) {
        if (!stmt || stmt->children.empty() || builder_->get_insert_block()->get_terminator()) return;
        if ((stmt->kind == "stmt" || stmt->kind == "matchedStmt" || stmt->kind == "unmatchedStmt")
            && stmt->children.size() == 1
            && (stmt->children[0]->kind == "matchedStmt" || stmt->children[0]->kind == "unmatchedStmt" || stmt->children[0]->kind == "simpleStmt")) {
            emit_stmt(stmt->children[0]);
            return;
        }
        if (stmt->children.size() >= 4 && stmt->children[0]->kind == "lVal" && stmt->children[1]->text == "=") {
            std::string name = node_text(stmt->children[0]);
            MiddleBinding binding = lookup(name);
            if (binding.ptr) builder_->create_store(as_i32(emit_expr(stmt->children[2])), binding.ptr);
            return;
        }
        if (stmt->children[0]->text == "return") {
            auto exp = direct_child(direct_child(stmt, "returnExpOpt"), "exp");
            if (current_return_type_ && current_return_type_->is_void_type()) {
                builder_->create_void_ret();
            } else {
                builder_->create_ret(exp ? as_i32(emit_expr(exp)) : i32(0));
            }
            return;
        }
        if (stmt->children[0]->text == "if") {
            emit_if(stmt);
            return;
        }
        if (stmt->children[0]->kind == "block") {
            emit_block(stmt->children[0]);
            return;
        }
        auto exp = direct_child(stmt, "exp");
        if (exp) (void)emit_expr(exp);
    }

    void emit_if(const AstPtr& stmt) {
        auto conds = collect_kind(stmt, "cond");
        Value* cond = conds.empty() ? i1(true) : as_bool(emit_expr(conds.front()));
        BasicBlock* then_bb = BasicBlock::create(&module_, unique_block("if_then"), current_function_);
        bool has_else = stmt->children.size() >= 7;
        BasicBlock* else_bb = has_else ? BasicBlock::create(&module_, unique_block("if_else"), current_function_) : nullptr;
        BasicBlock* merge_bb = has_else ? nullptr : BasicBlock::create(&module_, unique_block("if_end"), current_function_);
        builder_->create_cond_br(cond, then_bb, has_else ? else_bb : merge_bb);

        builder_->set_insert_point(then_bb);
        if (stmt->children.size() >= 5) emit_stmt(stmt->children[4]);
        if (!builder_->get_insert_block()->get_terminator()) {
            if (!merge_bb) merge_bb = BasicBlock::create(&module_, unique_block("if_end"), current_function_);
            builder_->create_br(merge_bb);
        }

        if (has_else) {
            builder_->set_insert_point(else_bb);
            emit_stmt(stmt->children[6]);
            if (!builder_->get_insert_block()->get_terminator()) {
                if (!merge_bb) merge_bb = BasicBlock::create(&module_, unique_block("if_end"), current_function_);
                builder_->create_br(merge_bb);
            }
        }

        if (merge_bb) builder_->set_insert_point(merge_bb);
    }

    MiddleValue emit_expr(const AstPtr& expr) {
        if (!expr) return {i32(0), int_type()};
        if (expr->kind == "IntConst") return {i32(std::stoi(expr->text)), int_type()};
        if (expr->kind == "lVal") {
            MiddleBinding binding = lookup(node_text(expr));
            if (!binding.ptr) return {i32(0), int_type()};
            return {builder_->create_load(binding.ptr), binding.type};
        }
        if (expr->kind == "Ident" && expr->children.empty()) {
            MiddleBinding binding = lookup(expr->text);
            if (!binding.ptr) return {i32(0), int_type()};
            return {builder_->create_load(binding.ptr), binding.type};
        }
        if (expr->kind == "unaryExp" && expr->children.size() == 4 && expr->children[0]->kind == "Ident") {
            std::string name = expr->children[0]->text;
            std::vector<Value*> args;
            for (const auto& arg : direct_call_args(expr->children[2])) args.push_back(as_i32(emit_expr(arg)));
            auto it = functions_.find(name);
            if (it == functions_.end()) return {i32(0), int_type()};
            auto* call = builder_->create_call(it->second, args);
            if (it->second->get_return_type()->is_void_type()) return {i32(0), int_type()};
            return {call, it->second->get_return_type()};
        }
        if (expr->children.size() == 1) return emit_expr(expr->children[0]);
        if (expr->children.size() == 2 && expr->children[0]->kind == "unaryOp") {
            std::string op = node_text(expr->children[0]);
            MiddleValue val = emit_expr(expr->children[1]);
            if (op == "-") return {builder_->create_isub(i32(0), as_i32(val)), int_type()};
            if (op == "!") {
                auto* cmp = builder_->create_icmp_eq(as_bool(val), i1(false));
                return {builder_->create_zext(cmp, int_type()), int_type()};
            }
            return {as_i32(val), int_type()};
        }
        if (expr->children.size() == 3 && expr->children[0]->text == "(" && expr->children[2]->text == ")") {
            return emit_expr(expr->children[1]);
        }
        if (expr->children.size() == 3) {
            std::string op = node_text(expr->children[1]);
            Value* lhs = as_i32(emit_expr(expr->children[0]));
            Value* rhs = as_i32(emit_expr(expr->children[2]));
            if (op == "+") return {builder_->create_iadd(lhs, rhs), int_type()};
            if (op == "-") return {builder_->create_isub(lhs, rhs), int_type()};
            if (op == "*") return {builder_->create_imul(lhs, rhs), int_type()};
            if (op == "/") return {builder_->create_isdiv(lhs, rhs), int_type()};
            if (op == "%") return {builder_->create_irem(lhs, rhs), int_type()};
            if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=") {
                Value* cmp = nullptr;
                if (op == "<") cmp = builder_->create_icmp_lt(lhs, rhs);
                else if (op == ">") cmp = builder_->create_icmp_gt(lhs, rhs);
                else if (op == "<=") cmp = builder_->create_icmp_le(lhs, rhs);
                else if (op == ">=") cmp = builder_->create_icmp_ge(lhs, rhs);
                else if (op == "==") cmp = builder_->create_icmp_eq(lhs, rhs);
                else cmp = builder_->create_icmp_ne(lhs, rhs);
                return {builder_->create_zext(cmp, int_type()), int_type()};
            }
            if (op == "&&" || op == "||") {
                Value* lz = builder_->create_zext(as_bool({lhs, int_type()}), int_type());
                Value* rz = builder_->create_zext(as_bool({rhs, int_type()}), int_type());
                Value* combined = op == "&&" ? static_cast<Value*>(builder_->create_imul(lz, rz))
                                             : static_cast<Value*>(builder_->create_iadd(lz, rz));
                Value* cmp = builder_->create_icmp_ne(combined, i32(0));
                return {builder_->create_zext(cmp, int_type()), int_type()};
            }
        }
        for (const auto& ch : expr->children) {
            if (ch->kind != "(" && ch->kind != ")" && ch->kind != "," && ch->kind != ";") return emit_expr(ch);
        }
        return {i32(0), int_type()};
    }

    Value* as_i32(MiddleValue value) {
        if (!value.value) return i32(0);
        if (value.type && value.type->is_int1_type()) return builder_->create_zext(value.value, int_type());
        return value.value;
    }

    Value* as_bool(MiddleValue value) {
        if (!value.value) return i1(false);
        if (value.type && value.type->is_int1_type()) return value.value;
        return builder_->create_icmp_ne(value.value, i32(0));
    }
};

} // namespace

IRResult IRGenerator::generate(const AstPtr& root, const std::string& source_name) {
    if (!uses_float(root)) {
        CompilerIRBackend backend(root, source_name);
        return backend.generate();
    }

    IRResult result;
    out_.str("");
    out_.clear();
    temp_id_ = 0;
    label_id_ = 0;
    values_.clear();
    function_returns_.clear();
    function_param_types_.clear();
    global_const_values_.clear();
    global_float_const_values_.clear();
    out_ << "; ModuleID = 'sysy2022_compiler'\n";
    out_ << "source_filename = \"" << source_name << "\"\n\n";
    emit_runtime();
    visit_top(root);
    result.text = out_.str();
    return result;
}

void IRGenerator::emit_runtime() {
    out_ << "declare i32 @getint()\n";
    out_ << "declare i32 @getch()\n";
    out_ << "declare i32 @getarray(i32*)\n";
    out_ << "declare void @putint(i32)\n";
    out_ << "declare void @putch(i32)\n";
    out_ << "declare void @putarray(i32, i32*)\n";
    out_ << "declare void @starttime()\n";
    out_ << "declare void @stoptime()\n\n";
}

void IRGenerator::visit_top(const AstPtr& root) {
    if (!root) return;
    auto items = collect_kind(root, "compUnitItem");
    enter_scope();
    for (const auto& item : items) {
        if (item->children.empty() || item->children.front()->kind != "funcDef") continue;
        auto names = collect_kind(item->children.front(), "funcName");
        if (names.empty()) continue;
        std::string name = token_text(names.front());
        std::string ret = item->children.front()->children.empty() ? "int" : c_type(item->children.front()->children.front());
        function_returns_[name] = ir_type(ret);
        std::vector<std::string> params;
        for (const auto& p : collect_kind(item->children.front(), "funcFParam")) {
            auto btypes = collect_kind(p, "bType");
            params.push_back(btypes.empty() ? "i32" : ir_type(c_type(btypes.front())));
        }
        function_param_types_[name] = params;
    }
    for (const auto& item : items) {
        if (item->children.empty()) continue;
        if (item->children.front()->kind == "decl") emit_global_decl(item->children.front());
    }
    for (const auto& item : items) {
        if (item->children.empty()) continue;
        if (item->children.front()->kind == "funcDef") emit_function(item->children.front());
    }
    leave_scope();
}

std::string IRGenerator::token_text(const AstPtr& node) const {
    if (!node) return "";
    if (!node->text.empty()) return node->text;
    for (const auto& ch : node->children) {
        std::string t = token_text(ch);
        if (!t.empty()) return t;
    }
    return "";
}

std::string IRGenerator::c_type(const AstPtr& type_node) const {
    std::string t = token_text(type_node);
    if (t == "float") return "float";
    if (t == "void") return "void";
    return "int";
}

std::string IRGenerator::ir_type(const std::string& ctype) const {
    if (ctype == "float") return "float";
    if (ctype == "void") return "void";
    return "i32";
}

std::string IRGenerator::zero_value(const std::string& ty) const {
    if (ty == "float") return "0.000000e+00";
    if (ty == "void") return "";
    return "0";
}

void IRGenerator::emit_global_decl(const AstPtr& decl) {
    if (!decl || decl->children.empty()) return;
    AstPtr real = decl->children.front();
    bool is_const = real->kind == "constDecl";
    std::string ty = "i32";
    auto btypes = collect_kind(real, "bType");
    if (!btypes.empty()) ty = ir_type(c_type(btypes.front()));
    auto defs = collect_kind(real, is_const ? "constDef" : "varDef");
    for (const auto& def : defs) {
        auto ids = collect_kind(def, "Ident");
        if (ids.empty()) continue;
        std::string init = zero_value(ty);
        double float_init = 0.0;
        auto init_node = collect_kind(def, is_const ? "constInitVal" : "initVal");
        if (!init_node.empty()) {
            if (ty == "float") {
                float_init = eval_float_const_expr(init_node.front(), global_const_values_, global_float_const_values_);
                init = format_float_const(float_init);
            } else {
                init = std::to_string(eval_int_const_expr(init_node.front(), global_const_values_));
            }
        }
        out_ << "@" << ids.front()->text << " = " << (is_const ? "constant " : "global ") << ty << " " << init << "\n";
        bind(ids.front()->text, "@" + ids.front()->text, ty);
        if (is_const && ty == "i32") global_const_values_[ids.front()->text] = std::stoi(init);
        if (is_const && ty == "float") global_float_const_values_[ids.front()->text] = float_init;
    }
}

void IRGenerator::emit_function(const AstPtr& func) {
    auto names = collect_kind(func, "funcName");
    if (names.empty()) return;
    std::string name = token_text(names.front());
    current_function_ = name;
    emitted_return_ = false;
    current_return_ir_type_ = func->children.empty() ? "i32" : ir_type(c_type(func->children.front()));

    std::vector<std::pair<std::string, std::string>> params;
    for (const auto& p : collect_kind(func, "funcFParam")) {
        auto ids = collect_kind(p, "Ident");
        auto btypes = collect_kind(p, "bType");
        if (ids.empty()) continue;
        params.push_back({ids.front()->text, btypes.empty() ? "i32" : ir_type(c_type(btypes.front()))});
    }

    out_ << "\ndefine " << current_return_ir_type_ << " @" << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i) out_ << ", ";
        out_ << params[i].second << " %" << params[i].first << ".arg";
    }
    out_ << ") {\n";
    out_ << name << "_ENTRY:\n";
    enter_scope();
    for (const auto& p : params) {
        std::string ptr = "%" + p.first;
        out_ << "  " << ptr << " = alloca " << p.second << "\n";
        out_ << "  store " << p.second << " %" << p.first << ".arg, " << p.second << "* " << ptr << "\n";
        bind(p.first, ptr, p.second);
    }
    auto blocks = collect_kind(func, "block");
    if (!blocks.empty()) emit_block(blocks.front());
    if (!emitted_return_) {
        if (current_return_ir_type_ == "void") out_ << "  ret void\n";
        else out_ << "  ret " << current_return_ir_type_ << " " << zero_value(current_return_ir_type_) << "\n";
    }
    leave_scope();
    out_ << "}\n";
}

void IRGenerator::emit_block(const AstPtr& block) {
    enter_scope();
    for (const auto& item : direct_block_items(block)) {
        if (item->children.empty()) continue;
        if (item->children.front()->kind == "decl") {
            AstPtr decl = item->children.front();
            AstPtr real = decl->children.empty() ? nullptr : decl->children.front();
            bool is_const = real && real->kind == "constDecl";
            std::string ty = "i32";
            auto btypes = collect_kind(decl, "bType");
            if (!btypes.empty()) ty = ir_type(c_type(btypes.front()));
            auto defs = collect_kind(decl, is_const ? "constDef" : "varDef");
            for (const auto& def : defs) {
                auto ids = collect_kind(def, "Ident");
                if (ids.empty()) continue;
                std::string ptr = "%" + ids.front()->text + "." + std::to_string(temp_id_++);
                out_ << "  " << ptr << " = alloca " << ty << "\n";
                bind(ids.front()->text, ptr, ty);
                auto inits = collect_kind(def, is_const ? "constInitVal" : "initVal");
                if (!inits.empty()) {
                    IRValue val = emit_expr_value(inits.front());
                    std::string casted = cast_value(val, ty);
                    out_ << "  store " << ty << " " << casted << ", " << ty << "* " << ptr << "\n";
                }
            }
        } else {
            emit_stmt(item->children.front());
        }
    }
    leave_scope();
}

void IRGenerator::emit_stmt(const AstPtr& stmt) {
    if (!stmt || stmt->children.empty()) return;
    if ((stmt->kind == "stmt" || stmt->kind == "matchedStmt" || stmt->kind == "unmatchedStmt")
        && stmt->children.size() == 1
        && (stmt->children[0]->kind == "matchedStmt" || stmt->children[0]->kind == "unmatchedStmt" || stmt->children[0]->kind == "simpleStmt")) {
        emit_stmt(stmt->children[0]);
        return;
    }
    if (stmt->children.size() >= 4 && stmt->children[0]->kind == "lVal" && stmt->children[1]->text == "=") {
        std::string name = emit_lval_name(stmt->children[0]);
        IRBinding binding = lookup_binding(name);
        if (binding.ptr.empty()) binding = {"@" + name, "i32"};
        IRValue val = emit_expr_value(stmt->children[2]);
        std::string casted = cast_value(val, binding.type);
        out_ << "  store " << binding.type << " " << casted << ", " << binding.type << "* " << binding.ptr << "\n";
        return;
    }
    if (stmt->children[0]->text == "return") {
        auto exps = collect_kind(stmt, "exp");
        if (current_return_ir_type_ == "void") {
            out_ << "  ret void\n";
        } else {
            IRValue val = exps.empty() ? IRValue{zero_value(current_return_ir_type_), current_return_ir_type_} : emit_expr_value(exps.front());
            out_ << "  ret " << current_return_ir_type_ << " " << cast_value(val, current_return_ir_type_) << "\n";
        }
        emitted_return_ = true;
        return;
    }
    if (stmt->children[0]->text == "if") {
        auto conds = collect_kind(stmt, "cond");
        IRValue cond = conds.empty() ? IRValue{"1", "i32"} : emit_expr_value(conds.front());
        std::string cmp = new_temp();
        if (cond.type == "float") out_ << "  " << cmp << " = fcmp one float " << cond.repr << ", 0.000000e+00\n";
        else out_ << "  " << cmp << " = icmp ne i32 " << cond.repr << ", 0\n";
        std::string then_label = new_label("if_then");
        std::string else_label = new_label("if_else");
        std::string end_label = new_label("if_end");
        bool before = emitted_return_;
        out_ << "  br i1 " << cmp << ", label %" << then_label << ", label %" << else_label << "\n";
        out_ << then_label << ":\n";
        emitted_return_ = false;
        if (stmt->children.size() >= 5) emit_stmt(stmt->children[4]);
        bool then_returned = emitted_return_;
        if (!then_returned) out_ << "  br label %" << end_label << "\n";
        out_ << else_label << ":\n";
        emitted_return_ = false;
        if (stmt->children.size() >= 7) emit_stmt(stmt->children[6]);
        bool else_returned = emitted_return_;
        if (!else_returned) out_ << "  br label %" << end_label << "\n";
        if (!then_returned || !else_returned) out_ << end_label << ":\n";
        emitted_return_ = before || (then_returned && else_returned);
        return;
    }
    if (stmt->children[0]->kind == "block") {
        emit_block(stmt->children[0]);
        return;
    }
    auto exps = collect_kind(stmt, "exp");
    if (!exps.empty()) (void)emit_expr_value(exps.front());
}

IRValue IRGenerator::emit_expr_value(const AstPtr& expr) {
    if (!expr) return {"0", "i32"};
    if (expr->kind == "IntConst") return {expr->text, "i32"};
    if (expr->kind == "floatConst") return {expr->text, "float"};
    if (expr->kind == "lVal") {
        std::string name = emit_lval_name(expr);
        IRBinding binding = lookup_binding(name);
        if (binding.ptr.empty()) binding = {"@" + name, "i32"};
        std::string tmp = new_temp();
        out_ << "  " << tmp << " = load " << binding.type << ", " << binding.type << "* " << binding.ptr << "\n";
        return {tmp, binding.type};
    }
    if (expr->kind == "Ident" && expr->children.empty()) {
        IRBinding binding = lookup_binding(expr->text);
        if (binding.ptr.empty()) binding = {"@" + expr->text, "i32"};
        std::string tmp = new_temp();
        out_ << "  " << tmp << " = load " << binding.type << ", " << binding.type << "* " << binding.ptr << "\n";
        return {tmp, binding.type};
    }
    if (expr->kind == "unaryExp" && expr->children.size() == 4 && expr->children[0]->kind == "Ident") {
        std::string name = expr->children[0]->text;
        auto args = direct_call_args(expr->children[2]);
        std::vector<IRValue> vals;
        for (const auto& arg : args) vals.push_back(emit_expr_value(arg));
        std::string ret_ty = function_returns_.count(name) ? function_returns_[name] : "i32";
        auto expected_params = function_param_types_.find(name);
        std::ostringstream call;
        if (ret_ty != "void") {
            std::string tmp = new_temp();
            call << "  " << tmp << " = call " << ret_ty << " @" << name << "(";
            for (size_t i = 0; i < vals.size(); ++i) {
                if (i) call << ", ";
                std::string arg_ty = vals[i].type;
                std::string arg_repr = vals[i].repr;
                if (expected_params != function_param_types_.end() && i < expected_params->second.size()) {
                    arg_ty = expected_params->second[i];
                    arg_repr = cast_value(vals[i], arg_ty);
                }
                call << arg_ty << " " << arg_repr;
            }
            call << ")\n";
            out_ << call.str();
            return {tmp, ret_ty};
        }
        call << "  call void @" << name << "(";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i) call << ", ";
            std::string arg_ty = vals[i].type;
            std::string arg_repr = vals[i].repr;
            if (expected_params != function_param_types_.end() && i < expected_params->second.size()) {
                arg_ty = expected_params->second[i];
                arg_repr = cast_value(vals[i], arg_ty);
            }
            call << arg_ty << " " << arg_repr;
        }
        call << ")\n";
        out_ << call.str();
        return {"0", "i32"};
    }
    if (expr->children.size() == 1) return emit_expr_value(expr->children[0]);
    if (expr->children.size() == 2 && expr->children[0]->kind == "unaryOp") {
        IRValue val = emit_expr_value(expr->children[1]);
        std::string op = token_text(expr->children[0]);
        if (op == "-") {
            std::string tmp = new_temp();
            if (val.type == "float") out_ << "  " << tmp << " = fsub float 0.000000e+00, " << val.repr << "\n";
            else out_ << "  " << tmp << " = sub i32 0, " << val.repr << "\n";
            return {tmp, val.type};
        }
        if (op == "!") {
            std::string cmp = new_temp();
            std::string zext = new_temp();
            if (val.type == "float") out_ << "  " << cmp << " = fcmp oeq float " << val.repr << ", 0.000000e+00\n";
            else out_ << "  " << cmp << " = icmp eq i32 " << val.repr << ", 0\n";
            out_ << "  " << zext << " = zext i1 " << cmp << " to i32\n";
            return {zext, "i32"};
        }
        return val;
    }
    if (expr->children.size() == 3 && expr->children[0]->text == "(" && expr->children[2]->text == ")") {
        return emit_expr_value(expr->children[1]);
    }
    if (expr->children.size() == 3) {
        std::string op = token_text(expr->children[1]);
        IRValue lhs = emit_expr_value(expr->children[0]);
        IRValue rhs = emit_expr_value(expr->children[2]);
        std::string ty = (lhs.type == "float" || rhs.type == "float") ? "float" : "i32";
        lhs.repr = cast_value(lhs, ty);
        rhs.repr = cast_value(rhs, ty);
        lhs.type = rhs.type = ty;
        std::string tmp = new_temp();
        if (op == "+") out_ << "  " << tmp << " = " << (ty == "float" ? "fadd float " : "add i32 ") << lhs.repr << ", " << rhs.repr << "\n";
        else if (op == "-") out_ << "  " << tmp << " = " << (ty == "float" ? "fsub float " : "sub i32 ") << lhs.repr << ", " << rhs.repr << "\n";
        else if (op == "*") out_ << "  " << tmp << " = " << (ty == "float" ? "fmul float " : "mul i32 ") << lhs.repr << ", " << rhs.repr << "\n";
        else if (op == "/") out_ << "  " << tmp << " = " << (ty == "float" ? "fdiv float " : "sdiv i32 ") << lhs.repr << ", " << rhs.repr << "\n";
        else if (op == "%") out_ << "  " << tmp << " = srem i32 " << lhs.repr << ", " << rhs.repr << "\n";
        else if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=") {
            std::string pred = op == "<" ? (ty == "float" ? "olt" : "slt")
                : op == ">" ? (ty == "float" ? "ogt" : "sgt")
                : op == "<=" ? (ty == "float" ? "ole" : "sle")
                : op == ">=" ? (ty == "float" ? "oge" : "sge")
                : op == "==" ? "eq" : "ne";
            std::string zext = new_temp();
            out_ << "  " << tmp << " = " << (ty == "float" ? "fcmp " : "icmp ") << pred << " " << ty << " " << lhs.repr << ", " << rhs.repr << "\n";
            out_ << "  " << zext << " = zext i1 " << tmp << " to i32\n";
            return {zext, "i32"};
        } else if (op == "&&" || op == "||") {
            std::string lcmp = new_temp();
            std::string rcmp = new_temp();
            std::string btmp = new_temp();
            std::string zext = new_temp();
            out_ << "  " << lcmp << " = icmp ne i32 " << cast_value(lhs, "i32") << ", 0\n";
            out_ << "  " << rcmp << " = icmp ne i32 " << cast_value(rhs, "i32") << ", 0\n";
            out_ << "  " << btmp << " = " << (op == "&&" ? "and" : "or") << " i1 " << lcmp << ", " << rcmp << "\n";
            out_ << "  " << zext << " = zext i1 " << btmp << " to i32\n";
            return {zext, "i32"};
        } else {
            out_ << "  " << tmp << " = add i32 " << cast_value(lhs, "i32") << ", 0\n";
        }
        return {tmp, ty};
    }
    for (const auto& ch : expr->children) {
        if (ch->kind != "(" && ch->kind != ")" && ch->kind != "," && ch->kind != ";") {
            return emit_expr_value(ch);
        }
    }
    return {"0", "i32"};
}

std::string IRGenerator::emit_expr(const AstPtr& expr) {
    return emit_expr_value(expr).repr;
}

std::string IRGenerator::emit_lval_name(const AstPtr& lval) {
    auto ids = collect_kind(lval, "Ident");
    return ids.empty() ? "" : ids.front()->text;
}

std::string IRGenerator::cast_value(IRValue value, const std::string& target_type) {
    if (value.type == target_type) return value.repr;
    std::string tmp = new_temp();
    if (value.type == "i32" && target_type == "float") {
        out_ << "  " << tmp << " = sitofp i32 " << value.repr << " to float\n";
        return tmp;
    }
    if (value.type == "float" && target_type == "i32") {
        out_ << "  " << tmp << " = fptosi float " << value.repr << " to i32\n";
        return tmp;
    }
    return value.repr;
}

std::string IRGenerator::new_temp() {
    return "%op" + std::to_string(temp_id_++);
}

std::string IRGenerator::new_label(const std::string& base) {
    return base + std::to_string(label_id_++);
}

void IRGenerator::enter_scope() {
    values_.push_back({});
}

void IRGenerator::leave_scope() {
    if (!values_.empty()) values_.pop_back();
}

void IRGenerator::bind(const std::string& name, const std::string& ptr, const std::string& type) {
    if (values_.empty()) enter_scope();
    values_.back()[name] = {ptr, type};
}

IRBinding IRGenerator::lookup_binding(const std::string& name) const {
    for (auto it = values_.rbegin(); it != values_.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }
    return {};
}

std::string IRGenerator::lookup_ptr(const std::string& name) const {
    return lookup_binding(name).ptr;
}
