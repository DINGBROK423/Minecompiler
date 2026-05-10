#pragma once

#include "common.h"

enum class TokenKind {
    KW,
    OP,
    SE,
    IDN,
    INT,
    FLOAT,
    END,
    ERROR
};

struct Token {
    TokenKind kind = TokenKind::ERROR;
    int code = -1;
    std::string lexeme;
    SourceLoc loc;
};

inline std::string token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::KW: return "KW";
        case TokenKind::OP: return "OP";
        case TokenKind::SE: return "SE";
        case TokenKind::IDN: return "IDN";
        case TokenKind::INT: return "INT";
        case TokenKind::FLOAT: return "FLOAT";
        case TokenKind::END: return "EOF";
        case TokenKind::ERROR: return "ERROR";
    }
    return "ERROR";
}

inline std::string parser_symbol(const Token& t) {
    if (t.kind == TokenKind::IDN) return "Ident";
    if (t.kind == TokenKind::INT) return "IntConst";
    if (t.kind == TokenKind::FLOAT) return "floatConst";
    if (t.kind == TokenKind::END) return "EOF";
    return to_lower(t.lexeme);
}

inline std::string token_output_line(const Token& t) {
    std::ostringstream out;
    if (t.kind == TokenKind::ERROR) {
        out << t.lexeme << "\t<ERROR," << t.loc.line << "," << t.loc.col << ">";
        return out.str();
    }
    if (t.kind == TokenKind::END) {
        return "";
    }
    out << t.lexeme << "\t<" << token_kind_name(t.kind) << ",";
    if (t.kind == TokenKind::KW || t.kind == TokenKind::OP || t.kind == TokenKind::SE) {
        out << t.code;
    } else {
        out << t.lexeme;
    }
    out << ">";
    return out.str();
}

