#include "lexer.h"

namespace {

std::set<int> chars_range(char a, char b) {
    std::set<int> s;
    for (int c = static_cast<unsigned char>(a); c <= static_cast<unsigned char>(b); ++c) {
        s.insert(c);
    }
    return s;
}

std::set<int> set_union_copy(std::set<int> a, const std::set<int>& b) {
    a.insert(b.begin(), b.end());
    return a;
}

std::set<int> keyword_chars(char c) {
    std::set<int> s;
    s.insert(static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c))));
    s.insert(static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c))));
    return s;
}

} // namespace

Lexer::Lexer() {
    build_nfa();
    build_dfa();
    minimize_dfa();
}

int Lexer::new_nfa_state() {
    int id = static_cast<int>(nfa_.size());
    NFAState st;
    st.id = id;
    nfa_.push_back(st);
    return id;
}

std::pair<int, int> Lexer::literal_nfa(const std::string& s) {
    if (s.empty()) {
        int st = new_nfa_state();
        return {st, st};
    }
    int start = new_nfa_state();
    int cur = start;
    for (char ch : s) {
        int next = new_nfa_state();
        nfa_[cur].edges[static_cast<unsigned char>(ch)].insert(next);
        cur = next;
    }
    return {start, cur};
}

std::pair<int, int> Lexer::charset_nfa(const std::set<int>& chars) {
    int start = new_nfa_state();
    int end = new_nfa_state();
    for (int c : chars) {
        nfa_[start].edges[c].insert(end);
    }
    return {start, end};
}

std::pair<int, int> Lexer::concat(std::pair<int, int> a, std::pair<int, int> b) {
    nfa_[a.second].eps.insert(b.first);
    return {a.first, b.second};
}

std::pair<int, int> Lexer::star(std::pair<int, int> a) {
    int start = new_nfa_state();
    int end = new_nfa_state();
    nfa_[start].eps.insert(a.first);
    nfa_[start].eps.insert(end);
    nfa_[a.second].eps.insert(a.first);
    nfa_[a.second].eps.insert(end);
    return {start, end};
}

std::pair<int, int> Lexer::plus(std::pair<int, int> a) {
    int start = new_nfa_state();
    int end = new_nfa_state();
    nfa_[start].eps.insert(a.first);
    nfa_[a.second].eps.insert(a.first);
    nfa_[a.second].eps.insert(end);
    return {start, end};
}

void Lexer::add_token_nfa(const std::string& name, int priority, std::pair<int, int> frag) {
    nfa_[nfa_start_].eps.insert(frag.first);
    nfa_[frag.second].accept_priority = priority;
    nfa_[frag.second].accept_name = name;
}

void Lexer::build_nfa() {
    nfa_.clear();
    nfa_start_ = new_nfa_state();

    auto add_literal = [&](const std::string& name, int priority, const std::string& text) {
        add_token_nfa(name, priority, literal_nfa(text));
    };
    auto add_keyword = [&](const std::string& word, int priority) {
        int start = new_nfa_state();
        int cur = start;
        for (char ch : word) {
            int next = new_nfa_state();
            for (int c : keyword_chars(ch)) {
                nfa_[cur].edges[c].insert(next);
            }
            cur = next;
        }
        add_token_nfa("ID", priority, {start, cur});
    };

    add_keyword("int", 1);
    add_keyword("void", 2);
    add_keyword("return", 3);
    add_keyword("const", 4);
    add_keyword("main", 5);
    add_keyword("float", 6);
    add_keyword("if", 7);
    add_keyword("else", 8);

    add_literal("OP", 10, "==");
    add_literal("OP", 11, "<=");
    add_literal("OP", 12, ">=");
    add_literal("OP", 13, "!=");
    add_literal("OP", 14, "&&");
    add_literal("OP", 15, "||");
    add_literal("OP", 16, "+");
    add_literal("OP", 17, "-");
    add_literal("OP", 18, "*");
    add_literal("OP", 19, "/");
    add_literal("OP", 20, "%");
    add_literal("OP", 21, "=");
    add_literal("OP", 22, ">");
    add_literal("OP", 23, "<");
    add_literal("OP", 24, "!");

    add_literal("SE", 30, "(");
    add_literal("SE", 31, ")");
    add_literal("SE", 32, "{");
    add_literal("SE", 33, "}");
    add_literal("SE", 34, ";");
    add_literal("SE", 35, ",");

    std::set<int> letter = set_union_copy(chars_range('a', 'z'), chars_range('A', 'Z'));
    letter.insert('_');
    std::set<int> digit = chars_range('0', '9');
    std::set<int> letter_digit = set_union_copy(letter, digit);

    add_token_nfa("ID", 40, concat(charset_nfa(letter), star(charset_nfa(letter_digit))));
    add_token_nfa("FLOAT", 41, concat(concat(plus(charset_nfa(digit)), literal_nfa(".")), plus(charset_nfa(digit))));
    add_token_nfa("INT", 42, plus(charset_nfa(digit)));

    std::set<int> ws = {' ', '\t', '\r', '\n'};
    add_token_nfa("WS", 90, plus(charset_nfa(ws)));
}

std::set<int> Lexer::epsilon_closure(const std::set<int>& states) const {
    std::set<int> closure = states;
    std::queue<int> q;
    for (int s : states) q.push(s);
    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        for (int nxt : nfa_[cur].eps) {
            if (!closure.count(nxt)) {
                closure.insert(nxt);
                q.push(nxt);
            }
        }
    }
    return closure;
}

std::set<int> Lexer::move_set(const std::set<int>& states, int c) const {
    std::set<int> result;
    for (int s : states) {
        auto it = nfa_[s].edges.find(c);
        if (it != nfa_[s].edges.end()) {
            result.insert(it->second.begin(), it->second.end());
        }
    }
    return result;
}

void Lexer::build_dfa() {
    dfa_.clear();
    std::map<std::set<int>, int> id;
    std::queue<std::set<int>> q;

    auto make_dfa_state = [&](const std::set<int>& subset) {
        DFAState st;
        st.id = static_cast<int>(dfa_.size());
        for (int n : subset) {
            if (!nfa_[n].accept_name.empty() && nfa_[n].accept_priority < st.accept_priority) {
                st.accept = true;
                st.accept_priority = nfa_[n].accept_priority;
                st.accept_name = nfa_[n].accept_name;
            }
        }
        dfa_.push_back(st);
        return st.id;
    };

    std::set<int> start = epsilon_closure({nfa_start_});
    id[start] = make_dfa_state(start);
    dfa_start_ = id[start];
    q.push(start);

    while (!q.empty()) {
        std::set<int> subset = q.front();
        q.pop();
        int cur_id = id[subset];
        std::set<int> chars;
        for (int s : subset) {
            for (const auto& kv : nfa_[s].edges) chars.insert(kv.first);
        }
        for (int c : chars) {
            std::set<int> nxt = epsilon_closure(move_set(subset, c));
            if (nxt.empty()) continue;
            if (!id.count(nxt)) {
                id[nxt] = make_dfa_state(nxt);
                q.push(nxt);
            }
            dfa_[cur_id].edges[c] = id[nxt];
        }
    }
}

void Lexer::minimize_dfa() {
    if (dfa_.empty()) return;
    std::map<std::pair<bool, std::string>, std::set<int>> groups;
    for (const auto& st : dfa_) {
        groups[{st.accept, st.accept_name}].insert(st.id);
    }
    std::vector<std::set<int>> parts;
    for (const auto& kv : groups) parts.push_back(kv.second);

    bool changed = true;
    while (changed) {
        changed = false;
        std::map<int, int> part_of;
        for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
            for (int s : parts[i]) part_of[s] = i;
        }
        std::vector<std::set<int>> next_parts;
        for (const auto& part : parts) {
            std::map<std::vector<int>, std::set<int>> buckets;
            for (int s : part) {
                std::vector<int> sig;
                for (int c = 0; c < 128; ++c) {
                    auto it = dfa_[s].edges.find(c);
                    sig.push_back(it == dfa_[s].edges.end() ? -1 : part_of[it->second]);
                }
                buckets[sig].insert(s);
            }
            if (buckets.size() > 1) changed = true;
            for (auto& kv : buckets) next_parts.push_back(kv.second);
        }
        parts = next_parts;
    }

    std::map<int, int> part_of;
    for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
        for (int s : parts[i]) part_of[s] = i;
    }
    std::vector<DFAState> minimized;
    minimized.resize(parts.size());
    for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
        int old = *parts[i].begin();
        minimized[i].id = i;
        minimized[i].accept = dfa_[old].accept;
        minimized[i].accept_priority = dfa_[old].accept_priority;
        minimized[i].accept_name = dfa_[old].accept_name;
        for (const auto& kv : dfa_[old].edges) {
            minimized[i].edges[kv.first] = part_of[kv.second];
        }
    }
    dfa_start_ = part_of[dfa_start_];
    dfa_ = minimized;
}

Token Lexer::make_token(const std::string& accept_name, const std::string& text, SourceLoc loc) {
    static const std::map<std::string, int> kw = {
        {"int", 1}, {"void", 2}, {"return", 3}, {"const", 4},
        {"main", 5}, {"float", 6}, {"if", 7}, {"else", 8}
    };
    static const std::map<std::string, int> op = {
        {"+", 6}, {"-", 7}, {"*", 8}, {"/", 9}, {"%", 10}, {"=", 11},
        {">", 12}, {"<", 13}, {"==", 14}, {"<=", 15}, {">=", 16},
        {"!=", 17}, {"&&", 18}, {"||", 19}, {"!", 20}
    };
    static const std::map<std::string, int> se = {
        {"(", 20}, {")", 21}, {"{", 22}, {"}", 23}, {";", 24}, {",", 25}
    };

    Token t;
    t.lexeme = text;
    t.loc = loc;
    if (accept_name == "ID") {
        std::string low = to_lower(text);
        auto it = kw.find(low);
        if (it != kw.end()) {
            t.kind = TokenKind::KW;
            t.code = it->second;
            t.lexeme = text;
        } else {
            t.kind = TokenKind::IDN;
            t.code = 0;
            if (!identifiers_.count(text)) identifiers_[text] = {loc, 0};
            identifiers_[text].second++;
        }
    } else if (accept_name == "INT") {
        t.kind = TokenKind::INT;
        t.code = 0;
    } else if (accept_name == "FLOAT") {
        t.kind = TokenKind::FLOAT;
        t.code = 0;
    } else if (accept_name == "OP") {
        t.kind = TokenKind::OP;
        t.code = op.at(text);
    } else if (accept_name == "SE") {
        t.kind = TokenKind::SE;
        t.code = se.at(text);
    }
    return t;
}

std::vector<Token> Lexer::tokenize(const std::string& source) {
    diagnostics_.clear();
    identifiers_.clear();
    std::vector<Token> tokens;
    SourceLoc loc;
    size_t i = 0;
    while (i < source.size()) {
        if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') {
                ++i;
                ++loc.col;
            }
            continue;
        }

        int cur = dfa_start_;
        int last_accept = -1;
        std::string last_name;
        size_t last_len = 0;
        size_t j = i;
        while (j < source.size()) {
            int c = static_cast<unsigned char>(source[j]);
            auto it = dfa_[cur].edges.find(c);
            if (it == dfa_[cur].edges.end()) break;
            cur = it->second;
            if (dfa_[cur].accept) {
                last_accept = cur;
                last_name = dfa_[cur].accept_name;
                last_len = j - i + 1;
            }
            ++j;
        }

        if (last_accept < 0 || last_len == 0) {
            Token err;
            err.kind = TokenKind::ERROR;
            err.lexeme = std::string(1, source[i]);
            err.loc = loc;
            tokens.push_back(err);
            diagnostics_.push_back({"lexer", loc, "illegal character '" + err.lexeme + "'"});
            if (source[i] == '\n') {
                loc.line++;
                loc.col = 1;
            } else {
                loc.col++;
            }
            ++i;
            continue;
        }

        std::string text = source.substr(i, last_len);
        SourceLoc start_loc = loc;
        for (char ch : text) {
            if (ch == '\n') {
                loc.line++;
                loc.col = 1;
            } else {
                loc.col++;
            }
        }
        i += last_len;
        if (last_name == "WS") continue;
        tokens.push_back(make_token(last_name, text, start_loc));
    }
    Token eof;
    eof.kind = TokenKind::END;
    eof.lexeme = "EOF";
    eof.loc = loc;
    tokens.push_back(eof);
    return tokens;
}

std::string Lexer::format_tokens(const std::vector<Token>& tokens) const {
    std::ostringstream out;
    for (const auto& t : tokens) {
        if (t.kind == TokenKind::END) continue;
        out << token_output_line(t) << "\n";
    }
    return out.str();
}

std::string Lexer::format_symbol_table() const {
    std::ostringstream out;
    out << "name\tfirst_line\tfirst_col\tcount\n";
    for (const auto& kv : identifiers_) {
        out << kv.first << "\t" << kv.second.first.line << "\t"
            << kv.second.first.col << "\t" << kv.second.second << "\n";
    }
    return out.str();
}

std::string Lexer::dump_dfa() const {
    std::ostringstream out;
    out << "state,accept,token,transitions\n";
    for (const auto& st : dfa_) {
        out << st.id << "," << (st.accept ? "yes" : "no") << "," << st.accept_name << ",";
        bool first = true;
        for (const auto& kv : st.edges) {
            if (kv.first < 32 || kv.first > 126) continue;
            if (!first) out << " ";
            first = false;
            out << "'" << static_cast<char>(kv.first) << "'->" << kv.second;
        }
        out << "\n";
    }
    return out.str();
}

