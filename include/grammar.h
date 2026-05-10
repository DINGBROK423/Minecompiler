#pragma once

#include "ast.h"

struct Production {
    int id = 0;
    std::string lhs;
    std::vector<std::string> rhs;
};

struct Grammar {
    std::string start = "Program";
    std::string augmented_start = "S'";
    std::vector<Production> productions;
    std::map<std::string, std::vector<int>> by_lhs;
    std::set<std::string> nonterminals;
    std::set<std::string> terminals;

    Grammar();
    bool is_terminal(const std::string& s) const;
    bool is_nonterminal(const std::string& s) const;
    std::string format() const;

private:
    void add(const std::string& lhs, std::initializer_list<std::string> rhs);
    void finalize();
};

struct Item {
    int production = 0;
    int dot = 0;

    bool operator<(const Item& o) const {
        if (production != o.production) return production < o.production;
        return dot < o.dot;
    }
    bool operator==(const Item& o) const {
        return production == o.production && dot == o.dot;
    }
};

struct Action {
    enum class Kind { Shift, Reduce, Accept, Error };
    Kind kind = Kind::Error;
    int target = -1;
    int production = -1;
};

struct SLRTable {
    std::vector<std::set<Item>> states;
    std::map<std::pair<int, std::string>, int> transitions;
    std::map<std::pair<int, std::string>, Action> action;
    std::map<std::pair<int, std::string>, int> go_to;
    std::vector<std::string> conflicts;
    std::map<std::string, std::set<std::string>> first;
    std::map<std::string, std::set<std::string>> follow;
};

class GrammarAnalyzer {
public:
    explicit GrammarAnalyzer(const Grammar& grammar);
    SLRTable build();
    std::string dump_first_follow(const SLRTable& table) const;
    std::string dump_first(const SLRTable& table) const;
    std::string dump_follow(const SLRTable& table) const;
    std::string dump_lr0(const SLRTable& table) const;
    std::string dump_slr_csv(const SLRTable& table) const;

private:
    const Grammar& g_;
    std::map<std::string, std::set<std::string>> compute_first();
    std::map<std::string, std::set<std::string>> compute_follow(const std::map<std::string, std::set<std::string>>& first);
    std::set<Item> closure(const std::set<Item>& items) const;
    std::set<Item> go_to(const std::set<Item>& items, const std::string& sym) const;
    std::string item_text(const Item& item) const;
};

struct ParseResult {
    bool ok = false;
    AstPtr root;
    std::string trace;
    std::vector<Diagnostic> diagnostics;
    SLRTable table;
};

class SLRParser {
public:
    SLRParser();
    ParseResult parse(const std::vector<Token>& tokens);
    const Grammar& grammar() const { return grammar_; }

private:
    Grammar grammar_;
    GrammarAnalyzer analyzer_;

    bool should_prefer_shift(int state, const std::string& term, const Action& old_action, const Action& new_action) const;
};
