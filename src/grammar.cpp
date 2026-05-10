#include "grammar.h"

Grammar::Grammar() {
    add(augmented_start, {"Program"});
    add("Program", {"compUnit", "EOF"});
    add("compUnit", {});
    add("compUnit", {"compUnit", "compUnitItem"});
    add("compUnitItem", {"decl"});
    add("compUnitItem", {"funcDef"});
    add("decl", {"constDecl"});
    add("decl", {"varDecl"});
    add("constDecl", {"const", "bType", "constDef", "constDefTail", ";"});
    add("constDefTail", {});
    add("constDefTail", {",", "constDef", "constDefTail"});
    add("bType", {"int"});
    add("bType", {"float"});
    add("constDef", {"Ident", "=", "constInitVal"});
    add("constInitVal", {"constExp"});
    add("varDecl", {"bType", "varDef", "varDefTail", ";"});
    add("varDefTail", {});
    add("varDefTail", {",", "varDef", "varDefTail"});
    add("varDef", {"Ident"});
    add("varDef", {"Ident", "=", "initVal"});
    add("initVal", {"exp"});
    add("funcDef", {"bType", "funcName", "(", "funcFParamsOpt", ")", "block"});
    add("funcDef", {"void", "funcName", "(", "funcFParamsOpt", ")", "block"});
    add("funcName", {"Ident"});
    add("funcName", {"main"});
    add("funcFParamsOpt", {});
    add("funcFParamsOpt", {"funcFParams"});
    add("funcFParams", {"funcFParam", "funcFParamsTail"});
    add("funcFParamsTail", {});
    add("funcFParamsTail", {",", "funcFParam", "funcFParamsTail"});
    add("funcFParam", {"bType", "Ident"});
    add("block", {"{", "blockItemList", "}"});
    add("blockItemList", {});
    add("blockItemList", {"blockItemList", "blockItem"});
    add("blockItem", {"decl"});
    add("blockItem", {"stmt"});
    add("stmt", {"matchedStmt"});
    add("stmt", {"unmatchedStmt"});
    add("matchedStmt", {"simpleStmt"});
    add("matchedStmt", {"if", "(", "cond", ")", "matchedStmt", "else", "matchedStmt"});
    add("unmatchedStmt", {"if", "(", "cond", ")", "stmt"});
    add("unmatchedStmt", {"if", "(", "cond", ")", "matchedStmt", "else", "unmatchedStmt"});
    add("simpleStmt", {"lVal", "=", "exp", ";"});
    add("simpleStmt", {"exp", ";"});
    add("simpleStmt", {";"});
    add("simpleStmt", {"block"});
    add("simpleStmt", {"return", "returnExpOpt", ";"});
    add("returnExpOpt", {});
    add("returnExpOpt", {"exp"});
    add("exp", {"lOrExp"});
    add("cond", {"lOrExp"});
    add("lVal", {"Ident"});
    add("primaryExp", {"(", "exp", ")"});
    add("primaryExp", {"lVal"});
    add("primaryExp", {"number"});
    add("number", {"IntConst"});
    add("number", {"floatConst"});
    add("unaryExp", {"primaryExp"});
    add("unaryExp", {"Ident", "(", "funcRParamsOpt", ")"});
    add("unaryExp", {"unaryOp", "unaryExp"});
    add("unaryOp", {"+"});
    add("unaryOp", {"-"});
    add("unaryOp", {"!"});
    add("funcRParamsOpt", {});
    add("funcRParamsOpt", {"funcRParams"});
    add("funcRParams", {"funcRParam", "funcRParamsTail"});
    add("funcRParamsTail", {});
    add("funcRParamsTail", {",", "funcRParam", "funcRParamsTail"});
    add("funcRParam", {"exp"});
    add("mulExp", {"unaryExp"});
    add("mulExp", {"mulExp", "*", "unaryExp"});
    add("mulExp", {"mulExp", "/", "unaryExp"});
    add("mulExp", {"mulExp", "%", "unaryExp"});
    add("addExp", {"mulExp"});
    add("addExp", {"addExp", "+", "mulExp"});
    add("addExp", {"addExp", "-", "mulExp"});
    add("relExp", {"addExp"});
    add("relExp", {"relExp", "<", "addExp"});
    add("relExp", {"relExp", ">", "addExp"});
    add("relExp", {"relExp", "<=", "addExp"});
    add("relExp", {"relExp", ">=", "addExp"});
    add("eqExp", {"relExp"});
    add("eqExp", {"eqExp", "==", "relExp"});
    add("eqExp", {"eqExp", "!=", "relExp"});
    add("lAndExp", {"eqExp"});
    add("lAndExp", {"lAndExp", "&&", "eqExp"});
    add("lOrExp", {"lAndExp"});
    add("lOrExp", {"lOrExp", "||", "lAndExp"});
    add("constExp", {"addExp"});
    finalize();
}

void Grammar::add(const std::string& lhs, std::initializer_list<std::string> rhs) {
    Production p;
    p.id = static_cast<int>(productions.size());
    p.lhs = lhs;
    p.rhs.assign(rhs.begin(), rhs.end());
    productions.push_back(p);
    nonterminals.insert(lhs);
}

void Grammar::finalize() {
    by_lhs.clear();
    terminals.clear();
    for (const auto& p : productions) {
        by_lhs[p.lhs].push_back(p.id);
    }
    for (const auto& p : productions) {
        for (const auto& s : p.rhs) {
            if (!nonterminals.count(s)) terminals.insert(s);
        }
    }
}

bool Grammar::is_terminal(const std::string& s) const {
    return terminals.count(s) > 0;
}

bool Grammar::is_nonterminal(const std::string& s) const {
    return nonterminals.count(s) > 0;
}

std::string Grammar::format() const {
    std::ostringstream out;
    for (const auto& p : productions) {
        out << p.id << ". " << p.lhs << " ->";
        if (p.rhs.empty()) out << " $";
        for (const auto& s : p.rhs) out << " " << s;
        out << "\n";
    }
    return out.str();
}

GrammarAnalyzer::GrammarAnalyzer(const Grammar& grammar) : g_(grammar) {}

std::map<std::string, std::set<std::string>> GrammarAnalyzer::compute_first() {
    std::map<std::string, std::set<std::string>> first;
    for (const auto& t : g_.terminals) first[t].insert(t);
    for (const auto& nt : g_.nonterminals) first[nt];
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& p : g_.productions) {
            bool nullable_prefix = true;
            if (p.rhs.empty()) {
                changed |= first[p.lhs].insert("$").second;
                continue;
            }
            for (const auto& sym : p.rhs) {
                for (const auto& x : first[sym]) {
                    if (x != "$") changed |= first[p.lhs].insert(x).second;
                }
                if (!first[sym].count("$")) {
                    nullable_prefix = false;
                    break;
                }
            }
            if (nullable_prefix) changed |= first[p.lhs].insert("$").second;
        }
    }
    return first;
}

std::map<std::string, std::set<std::string>> GrammarAnalyzer::compute_follow(const std::map<std::string, std::set<std::string>>& first) {
    std::map<std::string, std::set<std::string>> follow;
    for (const auto& nt : g_.nonterminals) follow[nt];
    follow[g_.start].insert("EOF");
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& p : g_.productions) {
            for (size_t i = 0; i < p.rhs.size(); ++i) {
                const std::string& b = p.rhs[i];
                if (!g_.is_nonterminal(b)) continue;
                bool suffix_nullable = true;
                for (size_t j = i + 1; j < p.rhs.size(); ++j) {
                    const std::string& beta = p.rhs[j];
                    for (const auto& x : first.at(beta)) {
                        if (x != "$") changed |= follow[b].insert(x).second;
                    }
                    if (!first.at(beta).count("$")) {
                        suffix_nullable = false;
                        break;
                    }
                }
                if (suffix_nullable) {
                    for (const auto& x : follow[p.lhs]) {
                        changed |= follow[b].insert(x).second;
                    }
                }
            }
        }
    }
    return follow;
}

std::set<Item> GrammarAnalyzer::closure(const std::set<Item>& items) const {
    std::set<Item> result = items;
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<Item> snapshot(result.begin(), result.end());
        for (const auto& item : snapshot) {
            const Production& p = g_.productions[item.production];
            if (item.dot >= static_cast<int>(p.rhs.size())) continue;
            const std::string& sym = p.rhs[item.dot];
            if (!g_.is_nonterminal(sym)) continue;
            auto it = g_.by_lhs.find(sym);
            if (it == g_.by_lhs.end()) continue;
            for (int pid : it->second) {
                changed |= result.insert({pid, 0}).second;
            }
        }
    }
    return result;
}

std::set<Item> GrammarAnalyzer::go_to(const std::set<Item>& items, const std::string& sym) const {
    std::set<Item> moved;
    for (const auto& item : items) {
        const Production& p = g_.productions[item.production];
        if (item.dot < static_cast<int>(p.rhs.size()) && p.rhs[item.dot] == sym) {
            moved.insert({item.production, item.dot + 1});
        }
    }
    if (moved.empty()) return moved;
    return closure(moved);
}

SLRTable GrammarAnalyzer::build() {
    SLRTable table;
    table.first = compute_first();
    table.follow = compute_follow(table.first);

    std::map<std::set<Item>, int> state_id;
    std::queue<std::set<Item>> q;
    std::set<Item> start = closure({{0, 0}});
    state_id[start] = 0;
    table.states.push_back(start);
    q.push(start);

    std::set<std::string> symbols = g_.terminals;
    symbols.insert(g_.nonterminals.begin(), g_.nonterminals.end());
    while (!q.empty()) {
        auto state = q.front();
        q.pop();
        int sid = state_id[state];
        for (const auto& sym : symbols) {
            auto nxt = go_to(state, sym);
            if (nxt.empty()) continue;
            if (!state_id.count(nxt)) {
                int nid = static_cast<int>(table.states.size());
                state_id[nxt] = nid;
                table.states.push_back(nxt);
                q.push(nxt);
            }
            table.transitions[{sid, sym}] = state_id[nxt];
        }
    }

    auto set_action = [&](int state, const std::string& term, Action action) {
        auto key = std::make_pair(state, term);
        auto it = table.action.find(key);
        if (it != table.action.end()) {
            std::ostringstream msg;
            msg << "conflict at state " << state << ", terminal " << term;
            table.conflicts.push_back(msg.str());
            if (it->second.kind == Action::Kind::Shift && action.kind == Action::Kind::Reduce && term == "else") {
                return;
            }
            if (it->second.kind == Action::Kind::Reduce && action.kind == Action::Kind::Shift && term == "else") {
                it->second = action;
                return;
            }
            return;
        }
        table.action[key] = action;
    };

    for (int sid = 0; sid < static_cast<int>(table.states.size()); ++sid) {
        for (const auto& kv : table.transitions) {
            if (kv.first.first != sid) continue;
            const std::string& sym = kv.first.second;
            if (g_.is_terminal(sym)) {
                Action a;
                a.kind = Action::Kind::Shift;
                a.target = kv.second;
                set_action(sid, sym, a);
            } else if (g_.is_nonterminal(sym)) {
                table.go_to[{sid, sym}] = kv.second;
            }
        }

        for (const auto& item : table.states[sid]) {
            const Production& p = g_.productions[item.production];
            if (item.dot != static_cast<int>(p.rhs.size())) continue;
            if (p.lhs == g_.augmented_start) {
                Action a;
                a.kind = Action::Kind::Accept;
                set_action(sid, "EOF", a);
            } else {
                for (const auto& term : table.follow[p.lhs]) {
                    Action a;
                    a.kind = Action::Kind::Reduce;
                    a.production = p.id;
                    set_action(sid, term, a);
                }
            }
        }
    }
    return table;
}

std::string GrammarAnalyzer::item_text(const Item& item) const {
    const Production& p = g_.productions[item.production];
    std::ostringstream out;
    out << p.lhs << " ->";
    for (int i = 0; i <= static_cast<int>(p.rhs.size()); ++i) {
        if (i == item.dot) out << " .";
        if (i < static_cast<int>(p.rhs.size())) out << " " << p.rhs[i];
    }
    return out.str();
}

std::string GrammarAnalyzer::dump_first_follow(const SLRTable& table) const {
    std::ostringstream out;
    out << "FIRST\n" << dump_first(table);
    out << "\nFOLLOW\n" << dump_follow(table);
    return out.str();
}

std::string GrammarAnalyzer::dump_first(const SLRTable& table) const {
    std::ostringstream out;
    for (const auto& nt : g_.nonterminals) {
        out << nt << ":";
        for (const auto& x : table.first.at(nt)) out << " " << x;
        out << "\n";
    }
    return out.str();
}

std::string GrammarAnalyzer::dump_follow(const SLRTable& table) const {
    std::ostringstream out;
    for (const auto& nt : g_.nonterminals) {
        out << nt << ":";
        for (const auto& x : table.follow.at(nt)) out << " " << x;
        out << "\n";
    }
    return out.str();
}

std::string GrammarAnalyzer::dump_lr0(const SLRTable& table) const {
    std::ostringstream out;
    for (int i = 0; i < static_cast<int>(table.states.size()); ++i) {
        out << "I" << i << ":\n";
        for (const auto& item : table.states[i]) out << "  " << item_text(item) << "\n";
        out << "\n";
    }
    if (!table.conflicts.empty()) {
        out << "Conflicts:\n";
        for (const auto& c : table.conflicts) out << "  " << c << "\n";
    }
    return out.str();
}

std::string GrammarAnalyzer::dump_slr_csv(const SLRTable& table) const {
    std::ostringstream out;
    out << "state,symbol,action\n";
    for (const auto& kv : table.action) {
        out << kv.first.first << "," << kv.first.second << ",";
        const Action& a = kv.second;
        if (a.kind == Action::Kind::Shift) out << "s" << a.target;
        else if (a.kind == Action::Kind::Reduce) out << "r" << a.production;
        else if (a.kind == Action::Kind::Accept) out << "acc";
        else out << "err";
        out << "\n";
    }
    for (const auto& kv : table.go_to) {
        out << kv.first.first << "," << kv.first.second << "," << kv.second << "\n";
    }
    return out.str();
}
