#include "semantic.h"

void SemanticAnalyzer::enter_scope(const std::string& name) {
    scopes_.push_back({});
    scope_names_.push_back(name);
}

void SemanticAnalyzer::leave_scope() {
    if (!scopes_.empty()) scopes_.pop_back();
    if (!scope_names_.empty()) scope_names_.pop_back();
}

bool SemanticAnalyzer::declared_in_current_scope(const std::string& name) const {
    return !scopes_.empty() && scopes_.back().count(name) > 0;
}

bool SemanticAnalyzer::declare(const SemanticSymbol& sym) {
    if (scopes_.empty()) enter_scope("global");
    auto& scope = scopes_.back();
    if (scope.count(sym.name)) {
        error(sym.loc, "redefinition of '" + sym.name + "'");
        return false;
    }
    scope[sym.name] = sym;
    all_symbols_.push_back({scope_names_.empty() ? "?" : scope_names_.back(), sym});
    return true;
}

const SemanticSymbol* SemanticAnalyzer::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

void SemanticAnalyzer::error(SourceLoc loc, const std::string& message) {
    result_.ok = false;
    result_.diagnostics.push_back({"semantic", loc, message});
}

std::string SemanticAnalyzer::node_text(const AstPtr& node) const {
    if (!node) return "";
    if (!node->text.empty()) return node->text;
    for (const auto& ch : node->children) {
        std::string t = node_text(ch);
        if (!t.empty()) return t;
    }
    return "";
}

std::vector<AstPtr> SemanticAnalyzer::flatten(const AstPtr& node, const std::string& kind) const {
    std::vector<AstPtr> out;
    if (!node) return out;
    if (node->kind == kind) out.push_back(node);
    for (const auto& ch : node->children) {
        auto more = flatten(ch, kind);
        out.insert(out.end(), more.begin(), more.end());
    }
    return out;
}

bool SemanticAnalyzer::can_assign(const std::string& lhs, const std::string& rhs) const {
    if (lhs == rhs) return true;
    return lhs == "float" && rhs == "int";
}

std::vector<AstPtr> SemanticAnalyzer::call_args(const AstPtr& funcRParamsOpt) const {
    std::vector<AstPtr> args;
    std::function<void(const AstPtr&)> walk = [&](const AstPtr& node) {
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
            for (const auto& ch : node->children) walk(ch);
        }
    };
    walk(funcRParamsOpt);
    return args;
}

void SemanticAnalyzer::predeclare_functions(const AstPtr& root) {
    for (const auto& fn_node : flatten(root, "funcDef")) {
        auto names = flatten(fn_node, "funcName");
        if (names.empty()) continue;
        SemanticSymbol fn;
        fn.name = node_text(names.front());
        fn.type = fn_node->children.empty() ? "int" : node_text(fn_node->children.front());
        fn.is_function = true;
        fn.loc = names.front()->loc;
        auto params = flatten(fn_node, "funcFParam");
        for (const auto& p : params) {
            auto bt = flatten(p, "bType");
            fn.params.push_back(bt.empty() ? "int" : node_text(bt.front()));
        }
        if (!declared_in_current_scope(fn.name)) declare(fn);
        else error(fn.loc, "redefinition of function '" + fn.name + "'");
    }
}

SemanticResult SemanticAnalyzer::analyze(const AstPtr& root) {
    result_ = {};
    scopes_.clear();
    scope_names_.clear();
    all_symbols_.clear();
    return_seen_.clear();
    current_return_type_.clear();
    enter_scope("global");
    predeclare_functions(root);
    visit(root);
    std::ostringstream dump;
    dump << "scope\tname\tkind\ttype\tconst\tparams\n";
    for (const auto& entry : all_symbols_) {
        const auto& sym = entry.second;
        dump << entry.first << "\t" << sym.name << "\t"
             << (sym.is_function ? "function" : "variable") << "\t"
             << sym.type << "\t" << (sym.is_const ? "yes" : "no") << "\t";
        for (size_t i = 0; i < sym.params.size(); ++i) {
            if (i) dump << ",";
            dump << sym.params[i];
        }
        dump << "\n";
    }
    result_.symbol_dump = dump.str();
    return result_;
}

void SemanticAnalyzer::visit(const AstPtr& node) {
    if (!node) return;
    if (node->kind == "decl") {
        visit_decl(node);
        return;
    }
    if (node->kind == "funcDef") {
        visit_func_def(node);
        return;
    }
    if (node->kind == "stmt" || node->kind == "matchedStmt" || node->kind == "unmatchedStmt" || node->kind == "simpleStmt") {
        visit_stmt(node);
        return;
    }
    for (const auto& ch : node->children) visit(ch);
}

void SemanticAnalyzer::visit_decl(const AstPtr& node) {
    if (node->children.empty()) return;
    AstPtr real = node->children.front();
    bool is_const = real->kind == "constDecl";
    std::string type = "int";
    auto btypes = flatten(real, "bType");
    if (!btypes.empty()) type = node_text(btypes.front());
    auto defs = flatten(real, is_const ? "constDef" : "varDef");
    for (const auto& def : defs) {
        auto ids = flatten(def, "Ident");
        if (ids.empty()) continue;
        SemanticSymbol sym;
        sym.name = ids.front()->text;
        sym.type = type;
        sym.is_const = is_const;
        sym.loc = ids.front()->loc;
        declare(sym);
        if (is_const && flatten(def, "constInitVal").empty()) {
            error(sym.loc, "const variable '" + sym.name + "' must be initialized");
        }
        auto init = flatten(def, is_const ? "constInitVal" : "initVal");
        if (!init.empty()) {
            std::string rhs = infer_expr(init.front());
            if (!can_assign(type, rhs)) {
                error(sym.loc, "cannot initialize '" + sym.name + "' of type " + type + " with " + rhs);
            }
        }
    }
}

void SemanticAnalyzer::visit_func_def(const AstPtr& node) {
    std::string ret_type = node->children.empty() ? "int" : node_text(node->children.front());
    auto names = flatten(node, "funcName");
    if (names.empty()) return;
    std::string fn_name = node_text(names.front());

    enter_scope("function:" + fn_name);
    std::string old_return = current_return_type_;
    current_return_type_ = ret_type;
    return_seen_.push_back(false);

    auto params = flatten(node, "funcFParam");
    for (const auto& p : params) {
        auto bt = flatten(p, "bType");
        auto pid = flatten(p, "Ident");
        if (pid.empty()) continue;
        SemanticSymbol ps;
        ps.name = pid.front()->text;
        ps.type = bt.empty() ? "int" : node_text(bt.front());
        ps.loc = pid.front()->loc;
        declare(ps);
    }
    for (const auto& ch : node->children) {
        if (ch->kind == "block") visit(ch);
    }
    if (ret_type != "void" && !return_seen_.empty() && !return_seen_.back()) {
        error(names.front()->loc, "non-void function '" + fn_name + "' may not return a value");
    }
    return_seen_.pop_back();
    current_return_type_ = old_return;
    leave_scope();
}

void SemanticAnalyzer::visit_stmt(const AstPtr& node) {
    if (!node || node->children.empty()) return;
    if ((node->kind == "stmt" || node->kind == "matchedStmt" || node->kind == "unmatchedStmt")
        && node->children.size() == 1
        && (node->children[0]->kind == "matchedStmt" || node->children[0]->kind == "unmatchedStmt" || node->children[0]->kind == "simpleStmt")) {
        visit_stmt(node->children[0]);
        return;
    }
    if (node->children.size() >= 4 && node->children[0]->kind == "lVal" && node->children[1]->text == "=") {
        std::string name = node_text(node->children[0]);
        const SemanticSymbol* sym = lookup(name);
        if (!sym) error(node->children[0]->loc, "assignment to undefined variable '" + name + "'");
        else if (sym->is_const) error(node->children[0]->loc, "cannot assign to const variable '" + name + "'");
        std::string rhs = infer_expr(node->children[2]);
        if (sym && !can_assign(sym->type, rhs)) {
            error(node->children[0]->loc, "cannot assign " + rhs + " to " + sym->type + " variable '" + name + "'");
        }
        return;
    }
    if (node->children[0]->text == "return") {
        auto exps = flatten(node, "exp");
        if (!return_seen_.empty()) return_seen_.back() = true;
        if (current_return_type_ == "void" && !exps.empty()) {
            error(node->children[0]->loc, "void function should not return a value");
        }
        if (current_return_type_ != "void" && exps.empty()) {
            error(node->children[0]->loc, "non-void function should return a value");
        }
        if (!exps.empty()) {
            std::string rt = infer_expr(exps.front());
            if (!current_return_type_.empty() && !can_assign(current_return_type_, rt)) {
                error(node->children[0]->loc, "return type " + rt + " does not match " + current_return_type_);
            }
        }
        return;
    }
    if (node->children[0]->text == "if") {
        auto conds = flatten(node, "cond");
        if (!conds.empty()) infer_expr(conds.front());
        for (const auto& ch : node->children) {
            if (ch->kind == "stmt" || ch->kind == "matchedStmt" || ch->kind == "unmatchedStmt" || ch->kind == "simpleStmt") {
                visit_stmt(ch);
            }
        }
        return;
    }
    if (node->children[0]->kind == "block") {
        enter_scope("block");
        visit(node->children[0]);
        leave_scope();
        return;
    }
    for (const auto& ch : node->children) {
        if (ch->kind == "exp") infer_expr(ch);
        else visit(ch);
    }
}

std::string SemanticAnalyzer::infer_expr(const AstPtr& node) {
    if (!node) return "int";
    if (node->kind == "IntConst") return "int";
    if (node->kind == "floatConst") return "float";
    if (node->kind == "lVal") {
        std::string name = node_text(node);
        const SemanticSymbol* sym = lookup(name);
        if (!sym) {
            error(node->loc, "use of undefined variable '" + name + "'");
            return "int";
        }
        if (sym->is_function) {
            error(node->loc, "function '" + name + "' used as variable");
            return sym->type;
        }
        return sym->type;
    }
    if (node->kind == "Ident" && node->children.empty()) {
        std::string name = node->text;
        const SemanticSymbol* sym = lookup(name);
        if (!sym) {
            error(node->loc, "use of undefined variable or function '" + name + "'");
            return "int";
        }
        return sym->type;
    }
    if (node->kind == "unaryExp" && node->children.size() == 4 && node->children[0]->kind == "Ident") {
        std::string name = node->children[0]->text;
        const SemanticSymbol* sym = lookup(name);
        if (!sym || !sym->is_function) {
            error(node->children[0]->loc, "call to undefined function '" + name + "'");
            return "int";
        }
        auto args = call_args(node->children[2]);
        if (args.size() != sym->params.size()) {
            error(node->children[0]->loc, "function '" + name + "' expects " + std::to_string(sym->params.size()) + " argument(s), got " + std::to_string(args.size()));
        }
        size_t n = std::min(args.size(), sym->params.size());
        for (size_t i = 0; i < n; ++i) {
            std::string actual = infer_expr(args[i]);
            if (!can_assign(sym->params[i], actual)) {
                error(args[i]->loc, "argument " + std::to_string(i + 1) + " of '" + name + "' expects " + sym->params[i] + ", got " + actual);
            }
        }
        return sym->type;
    }
    std::string type = "int";
    for (const auto& ch : node->children) {
        std::string ct = infer_expr(ch);
        if (ct == "float") type = "float";
    }
    if (node->kind == "relExp" || node->kind == "eqExp" || node->kind == "lAndExp" || node->kind == "lOrExp") {
        if (node->children.size() == 3) return "bool";
    }
    return type;
}
