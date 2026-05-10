#include "ast.h"

namespace {

void format_ast_rec(const AstPtr& node, int depth, std::ostringstream& out) {
    if (!node) return;
    for (int i = 0; i < depth; ++i) out << "  ";
    out << node->kind;
    if (!node->text.empty()) out << ": " << node->text;
    if (node->loc.line > 0) out << " @" << node->loc.line << ":" << node->loc.col;
    out << "\n";
    for (const auto& ch : node->children) format_ast_rec(ch, depth + 1, out);
}

std::string dot_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') r.push_back('\\');
        if (c == '\n') r += "\\n";
        else r.push_back(c);
    }
    return r;
}

int dot_rec(const AstPtr& node, int& next_id, std::ostringstream& out) {
    int id = next_id++;
    std::string label = node->kind;
    if (!node->text.empty()) label += "\\n" + node->text;
    out << "  n" << id << " [label=\"" << dot_escape(label) << "\"];\n";
    for (const auto& ch : node->children) {
        int cid = dot_rec(ch, next_id, out);
        out << "  n" << id << " -> n" << cid << ";\n";
    }
    return id;
}

} // namespace

std::string format_ast(const AstPtr& root) {
    std::ostringstream out;
    format_ast_rec(root, 0, out);
    return out.str();
}

std::string ast_to_dot(const AstPtr& root) {
    std::ostringstream out;
    out << "digraph AST {\n";
    out << "  node [shape=box,fontname=\"Menlo\"];\n";
    int next_id = 0;
    if (root) dot_rec(root, next_id, out);
    out << "}\n";
    return out.str();
}

