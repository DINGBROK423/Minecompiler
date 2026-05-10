#include "grammar.h"

SLRParser::SLRParser() : grammar_(), analyzer_(grammar_) {}

bool SLRParser::should_prefer_shift(int, const std::string&, const Action&, const Action&) const {
    return false;
}

ParseResult SLRParser::parse(const std::vector<Token>& tokens) {
    ParseResult result;
    result.table = analyzer_.build();
    std::vector<int> states;
    std::vector<AstPtr> nodes;
    states.push_back(0);
    size_t pos = 0;
    int step = 1;
    std::ostringstream trace;

    while (true) {
        int state = states.back();
        const Token& tok = pos < tokens.size() ? tokens[pos] : tokens.back();
        std::string sym = parser_symbol(tok);
        auto it = result.table.action.find({state, sym});
        if (it == result.table.action.end()) {
            trace << step << "\tstate" << state << "#" << sym << "\terror\n";
            result.diagnostics.push_back({"parser", tok.loc, "unexpected token '" + tok.lexeme + "' in state " + std::to_string(state)});
            result.ok = false;
            result.trace = trace.str();
            return result;
        }
        const Action& action = it->second;
        if (action.kind == Action::Kind::Shift) {
            trace << step++ << "\t" << sym << "#" << tok.lexeme << "\tmove\n";
            auto leaf = make_ast_node(sym, tok.lexeme, tok.loc);
            nodes.push_back(leaf);
            states.push_back(action.target);
            pos++;
        } else if (action.kind == Action::Kind::Reduce) {
            const Production& p = grammar_.productions[action.production];
            std::string rhs_head = p.rhs.empty() ? "$" : p.rhs.front();
            trace << step++ << "\t" << p.lhs << "#" << rhs_head << "\treduction\n";
            auto node = make_ast_node(p.lhs);
            std::vector<AstPtr> children;
            for (size_t i = 0; i < p.rhs.size(); ++i) {
                if (!nodes.empty()) {
                    children.push_back(nodes.back());
                    nodes.pop_back();
                }
                if (!states.empty()) states.pop_back();
            }
            std::reverse(children.begin(), children.end());
            node->children = std::move(children);
            if (!node->children.empty()) node->loc = node->children.front()->loc;
            int top = states.back();
            auto gt = result.table.go_to.find({top, p.lhs});
            if (gt == result.table.go_to.end()) {
                result.diagnostics.push_back({"parser", tok.loc, "missing goto for " + p.lhs});
                result.ok = false;
                result.trace = trace.str();
                return result;
            }
            nodes.push_back(node);
            states.push_back(gt->second);
        } else if (action.kind == Action::Kind::Accept) {
            trace << step++ << "\tProgram#EOF\taccept\n";
            result.ok = true;
            result.root = nodes.empty() ? nullptr : nodes.back();
            result.trace = trace.str();
            return result;
        } else {
            trace << step << "\tstate" << state << "#" << sym << "\terror\n";
            result.ok = false;
            result.trace = trace.str();
            return result;
        }
    }
}

