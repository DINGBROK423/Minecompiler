#pragma once

#include "token.h"

struct NFAState {
    int id = 0;
    std::map<int, std::set<int>> edges;
    std::set<int> eps;
    int accept_priority = 1000000;
    std::string accept_name;
};

struct DFAState {
    int id = 0;
    std::map<int, int> edges;
    bool accept = false;
    int accept_priority = 1000000;
    std::string accept_name;
};

class Lexer {
public:
    Lexer();

    std::vector<Token> tokenize(const std::string& source);
    std::string format_tokens(const std::vector<Token>& tokens) const;
    std::string format_symbol_table() const;
    std::string dump_dfa() const;
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

private:
    std::vector<NFAState> nfa_;
    std::vector<DFAState> dfa_;
    std::vector<Diagnostic> diagnostics_;
    std::map<std::string, std::pair<SourceLoc, int>> identifiers_;
    int nfa_start_ = 0;
    int dfa_start_ = 0;

    int new_nfa_state();
    std::pair<int, int> literal_nfa(const std::string& s);
    std::pair<int, int> charset_nfa(const std::set<int>& chars);
    std::pair<int, int> concat(std::pair<int, int> a, std::pair<int, int> b);
    std::pair<int, int> star(std::pair<int, int> a);
    std::pair<int, int> plus(std::pair<int, int> a);
    void add_token_nfa(const std::string& name, int priority, std::pair<int, int> frag);
    void build_nfa();
    void build_dfa();
    std::set<int> epsilon_closure(const std::set<int>& states) const;
    std::set<int> move_set(const std::set<int>& states, int c) const;
    void minimize_dfa();
    Token make_token(const std::string& accept_name, const std::string& text, SourceLoc loc);
};

