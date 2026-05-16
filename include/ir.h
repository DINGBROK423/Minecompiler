#pragma once

#include "semantic.h"

struct IRResult {
    bool ok = true;
    std::string text;
    std::vector<Diagnostic> diagnostics;
};

struct IRValue {
    std::string repr;
    std::string type = "i32";
};

struct IRBinding {
    std::string ptr;
    std::string type = "i32";
};

class IRGenerator {
public:
    IRResult generate(const AstPtr& root, const std::string& source_name);

private:
    std::ostringstream out_;
    int temp_id_ = 0;
    int label_id_ = 0;
    std::vector<std::map<std::string, IRBinding>> values_;
    std::map<std::string, std::string> function_returns_;
    std::map<std::string, std::vector<std::string>> function_param_types_;
    std::map<std::string, int> global_const_values_;
    std::map<std::string, double> global_float_const_values_;
    std::string current_function_;
    std::string current_return_ir_type_ = "i32";
    bool emitted_return_ = false;

    void emit_runtime();
    void visit_top(const AstPtr& root);
    void emit_global_decl(const AstPtr& decl);
    void emit_function(const AstPtr& func);
    void emit_block(const AstPtr& block);
    void emit_stmt(const AstPtr& stmt);
    IRValue emit_expr_value(const AstPtr& expr);
    std::string emit_expr(const AstPtr& expr);
    std::string emit_lval_name(const AstPtr& lval);
    std::string c_type(const AstPtr& type_node) const;
    std::string ir_type(const std::string& ctype) const;
    std::string zero_value(const std::string& ir_type) const;
    std::string cast_value(IRValue value, const std::string& target_type);
    std::string new_temp();
    std::string new_label(const std::string& base);
    void enter_scope();
    void leave_scope();
    void bind(const std::string& name, const std::string& ptr, const std::string& type);
    IRBinding lookup_binding(const std::string& name) const;
    std::string lookup_ptr(const std::string& name) const;
    std::string token_text(const AstPtr& node) const;
};
