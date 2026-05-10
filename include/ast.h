#pragma once

#include "token.h"

struct AstNode {
    std::string kind;
    std::string text;
    SourceLoc loc;
    std::vector<std::shared_ptr<AstNode>> children;
};

using AstPtr = std::shared_ptr<AstNode>;

inline AstPtr make_ast_node(const std::string& kind, const std::string& text = "", SourceLoc loc = {}) {
    auto node = std::make_shared<AstNode>();
    node->kind = kind;
    node->text = text;
    node->loc = loc;
    return node;
}

std::string format_ast(const AstPtr& root);
std::string ast_to_dot(const AstPtr& root);

