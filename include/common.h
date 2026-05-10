#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <cstdlib>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#include <vector>

struct SourceLoc {
    int line = 1;
    int col = 1;
};

struct Diagnostic {
    std::string phase;
    SourceLoc loc;
    std::string message;
};

inline std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open input file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline void write_file(const std::string& path, const std::string& text) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write output file: " + path);
    }
    out << text;
}

inline bool path_exists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

inline void ensure_dir(const std::string& path) {
    if (path.empty() || path_exists(path)) return;
    std::string cur;
    for (char c : path) {
        cur.push_back(c);
        if (c == '/') {
            if (cur.size() > 1 && !path_exists(cur)) mkdir(cur.c_str(), 0755);
        }
    }
    if (!path_exists(path)) mkdir(path.c_str(), 0755);
}

inline std::string basename_no_ext(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}

inline std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
