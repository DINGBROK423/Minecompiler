#include "ir.h"
#include "lexer.h"

namespace {

struct Options {
    bool lex = false;
    bool parse = false;
    bool ast = false;
    bool ast_dot = false;
    bool semantic = false;
    bool ir = false;
    bool dump_dfa = false;
    bool symtab = false;
    bool dump_first_follow = false;
    bool dump_first = false;
    bool dump_follow = false;
    bool dump_lr0 = false;
    bool dump_slr = false;
    bool dump_all = false;
    bool run_tests = false;
    std::string output;
    std::string input;
};

void usage() {
    std::cerr << "Usage: ./cmmc [--lex|--parse|--ast|--ast-dot|--semantic|--ir]\n"
              << "             [--symtab|--dump-dfa|--dump-first-follow|--dump-first|--dump-follow|--dump-lr0|--dump-slr|--dump-all]\n"
              << "             [--run-tests] [-o file_or_dir] input.sy\n";
}

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--lex") opt.lex = true;
        else if (a == "--parse") opt.parse = true;
        else if (a == "--ast") opt.ast = true;
        else if (a == "--ast-dot") opt.ast_dot = true;
        else if (a == "--semantic") opt.semantic = true;
        else if (a == "--ir") opt.ir = true;
        else if (a == "--symtab") opt.symtab = true;
        else if (a == "--dump-dfa") opt.dump_dfa = true;
        else if (a == "--dump-first-follow") opt.dump_first_follow = true;
        else if (a == "--dump-first") opt.dump_first = true;
        else if (a == "--dump-follow") opt.dump_follow = true;
        else if (a == "--dump-lr0") opt.dump_lr0 = true;
        else if (a == "--dump-slr") opt.dump_slr = true;
        else if (a == "--dump-all") opt.dump_all = true;
        else if (a == "--dump-tables") {
            opt.dump_first_follow = true;
            opt.dump_lr0 = true;
            opt.dump_slr = true;
        }
        else if (a == "--run-tests") opt.run_tests = true;
        else if (a == "-o" && i + 1 < argc) opt.output = argv[++i];
        else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option: " + a);
        else opt.input = a;
    }
    if (opt.run_tests) return opt;
    if (opt.dump_all) {
        opt.lex = opt.parse = opt.ast = opt.ast_dot = opt.semantic = opt.ir = true;
        opt.symtab = true;
        opt.dump_dfa = opt.dump_first = opt.dump_follow = opt.dump_lr0 = opt.dump_slr = true;
    }
    if (!opt.lex && !opt.parse && !opt.ast && !opt.ast_dot && !opt.semantic && !opt.ir
        && !opt.symtab && !opt.dump_dfa && !opt.dump_first_follow && !opt.dump_first && !opt.dump_follow
        && !opt.dump_lr0 && !opt.dump_slr) {
        opt.ir = true;
    }
    if (opt.input.empty()) throw std::runtime_error("missing input file");
    return opt;
}

void print_diagnostics(const std::vector<Diagnostic>& diagnostics) {
    for (const auto& d : diagnostics) {
        std::cerr << d.phase << ":" << d.loc.line << ":" << d.loc.col << ": " << d.message << "\n";
    }
}

std::string output_path(const Options& opt, const std::string& suffix) {
    std::string dir = opt.output.empty() ? "build/out" : opt.output;
    ensure_dir(dir);
    return dir + "/" + basename_no_ext(opt.input) + suffix;
}

bool single_output_mode(const Options& opt) {
    int count = 0;
    count += opt.lex;
    count += opt.parse;
    count += opt.ast;
    count += opt.ast_dot;
    count += opt.semantic;
    count += opt.ir;
    count += opt.symtab;
    count += opt.dump_dfa;
    count += opt.dump_first_follow;
    count += opt.dump_first;
    count += opt.dump_follow;
    count += opt.dump_lr0;
    count += opt.dump_slr;
    return count == 1 && !opt.dump_all;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Options opt = parse_args(argc, argv);
        if (opt.run_tests) {
            int rc = std::system("./scripts/run_tests.sh");
            return rc == 0 ? 0 : 1;
        }
        std::string source = read_file(opt.input);
        Lexer lexer;
        auto tokens = lexer.tokenize(source);
        SLRParser parser;
        GrammarAnalyzer analyzer(parser.grammar());
        SLRTable table = analyzer.build();

        if (opt.dump_dfa) {
            std::string text = lexer.dump_dfa();
            if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".dfa.csv"), text);
            else std::cout << text;
        }
        if (opt.lex) {
            std::string text = lexer.format_tokens(tokens);
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".tokens"), text);
            else std::cout << text;
            if (!lexer.diagnostics().empty()) {
                print_diagnostics(lexer.diagnostics());
                return 1;
            }
            if (single_output_mode(opt)) return 0;
        }
        if (opt.symtab) {
            std::string text = lexer.format_symbol_table();
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".symtab"), text);
            else std::cout << text;
            if (single_output_mode(opt)) return 0;
        }
        if (opt.dump_first_follow || opt.dump_first || opt.dump_follow || opt.dump_lr0 || opt.dump_slr) {
            if (opt.dump_first_follow) {
                std::string text = analyzer.dump_first_follow(table);
                if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".first_follow"), text);
                else std::cout << text;
            }
            if (opt.dump_first) {
                std::string text = analyzer.dump_first(table);
                if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".first"), text);
                else std::cout << text;
            }
            if (opt.dump_follow) {
                std::string text = analyzer.dump_follow(table);
                if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".follow"), text);
                else std::cout << text;
            }
            if (opt.dump_lr0) {
                std::string text = analyzer.dump_lr0(table);
                if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".lr0"), text);
                else std::cout << text;
            }
            if (opt.dump_slr) {
                std::string text = analyzer.dump_slr_csv(table);
                if (!opt.output.empty() && !single_output_mode(opt)) write_file(output_path(opt, ".slr.csv"), text);
                else std::cout << text;
            }
            if (single_output_mode(opt)) return 0;
        }

        ParseResult parsed = parser.parse(tokens);
        if (opt.parse) {
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".parse"), parsed.trace);
            else std::cout << parsed.trace;
            if (!parsed.ok) {
                print_diagnostics(parsed.diagnostics);
                return 1;
            }
            if (single_output_mode(opt)) return 0;
        }
        if (!parsed.ok) {
            print_diagnostics(parsed.diagnostics);
            return 1;
        }
        if (opt.ast) {
            std::string text = format_ast(parsed.root);
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".ast"), text);
            else std::cout << text;
            if (single_output_mode(opt)) return 0;
        }
        if (opt.ast_dot) {
            std::string text = ast_to_dot(parsed.root);
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".ast.dot"), text);
            else std::cout << text;
            if (single_output_mode(opt)) return 0;
        }

        SemanticAnalyzer sem;
        SemanticResult sem_result = sem.analyze(parsed.root);
        if (opt.semantic) {
            std::ostringstream text;
            text << sem_result.symbol_dump;
            for (const auto& d : sem_result.diagnostics) {
                text << d.phase << ":" << d.loc.line << ":" << d.loc.col << ": " << d.message << "\n";
            }
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".semantic.log"), text.str());
            else {
                std::cout << sem_result.symbol_dump;
                print_diagnostics(sem_result.diagnostics);
            }
            if (single_output_mode(opt)) return sem_result.ok ? 0 : 1;
        }
        if (!sem_result.ok) {
            print_diagnostics(sem_result.diagnostics);
            return 1;
        }

        if (opt.ir) {
            IRGenerator ir;
            IRResult ir_result = ir.generate(parsed.root, opt.input);
            if (!opt.output.empty()) write_file(single_output_mode(opt) ? opt.output : output_path(opt, ".ll"), ir_result.text);
            else std::cout << ir_result.text;
            if (single_output_mode(opt)) return ir_result.ok ? 0 : 1;
        }
    } catch (const std::exception& e) {
        usage();
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}
