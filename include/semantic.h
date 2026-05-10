#pragma once

#include "grammar.h"

struct SemanticSymbol {
    std::string name;
    std::string type;
    bool is_const = false;
    bool is_function = false;
    std::vector<std::string> params;
    SourceLoc loc;
};

struct SemanticResult {
    bool ok = true;
    std::vector<Diagnostic> diagnostics;
    std::string symbol_dump;
};

class SemanticAnalyzer {
public:
    SemanticResult analyze(const AstPtr& root);

private:
    std::vector<std::map<std::string, SemanticSymbol>> scopes_;
    std::vector<std::string> scope_names_;
    std::vector<std::pair<std::string, SemanticSymbol>> all_symbols_;
    std::vector<bool> return_seen_;
    SemanticResult result_;
    std::string current_return_type_;

    void enter_scope(const std::string& name = "block");
    void leave_scope();
    bool declare(const SemanticSymbol& sym);
    bool declared_in_current_scope(const std::string& name) const;
    const SemanticSymbol* lookup(const std::string& name) const;
    void error(SourceLoc loc, const std::string& message);
    void predeclare_functions(const AstPtr& root);
    void visit(const AstPtr& node);
    void visit_decl(const AstPtr& node);
    void visit_func_def(const AstPtr& node);
    void visit_stmt(const AstPtr& node);
    std::string infer_expr(const AstPtr& node);
    bool can_assign(const std::string& lhs, const std::string& rhs) const;
    std::vector<AstPtr> call_args(const AstPtr& funcRParamsOpt) const;
    std::string node_text(const AstPtr& node) const;
    std::vector<AstPtr> flatten(const AstPtr& node, const std::string& kind) const;
};
