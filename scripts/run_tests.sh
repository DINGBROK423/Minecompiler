#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[lex] valid/basic.sy"
./cmmc --lex tests/valid/basic.sy >/tmp/cmmc_basic.tokens
grep -q $'int\t<KW,1>' /tmp/cmmc_basic.tokens
./cmmc --symtab tests/valid/basic.sy >/tmp/cmmc_basic.symtab
grep -q $'name\tfirst_line\tfirst_col\tcount' /tmp/cmmc_basic.symtab

echo "[lex] lexer/mixed.sy"
if ./cmmc --lex tests/lexer/mixed.sy >/tmp/cmmc_mixed.tokens 2>/tmp/cmmc_mixed.err; then
  echo "expected lexer failure for illegal character" >&2
  exit 1
fi
grep -q $'InT\t<KW,1>' /tmp/cmmc_mixed.tokens
grep -q "illegal character" /tmp/cmmc_mixed.err

echo "[parse] valid/basic.sy"
./cmmc --parse tests/valid/basic.sy >/tmp/cmmc_basic.parse
grep -q "accept" /tmp/cmmc_basic.parse

echo "[parse] valid/functions.sy"
./cmmc --parse tests/valid/functions.sy >/tmp/cmmc_functions.parse
grep -q "accept" /tmp/cmmc_functions.parse

echo "[parse] valid/dangling_else.sy"
./cmmc --parse tests/valid/dangling_else.sy >/tmp/cmmc_dangling.parse
grep -q "accept" /tmp/cmmc_dangling.parse

echo "[ast] valid/if_else.sy"
./cmmc --ast tests/valid/if_else.sy >/tmp/cmmc_if.ast
grep -q "funcDef" /tmp/cmmc_if.ast

echo "[ast-dot] valid/if_else.sy"
./cmmc --ast-dot tests/valid/if_else.sy >/tmp/cmmc_if.dot
grep -q "digraph AST" /tmp/cmmc_if.dot

echo "[semantic] invalid/semantic.sy"
if ./cmmc --semantic tests/invalid/semantic.sy >/tmp/cmmc_sem.out 2>/tmp/cmmc_sem.err; then
  echo "expected semantic failure" >&2
  exit 1
fi
grep -q "undefined" /tmp/cmmc_sem.err

echo "[semantic] invalid/const_assign.sy"
if ./cmmc --semantic tests/invalid/const_assign.sy >/tmp/cmmc_const.out 2>/tmp/cmmc_const.err; then
  echo "expected const assignment failure" >&2
  exit 1
fi
grep -q "const" /tmp/cmmc_const.err

echo "[semantic] invalid/call_args.sy"
if ./cmmc --semantic tests/invalid/call_args.sy >/tmp/cmmc_args.out 2>/tmp/cmmc_args.err; then
  echo "expected call argument failure" >&2
  exit 1
fi
grep -q "expects 2" /tmp/cmmc_args.err

echo "[ir] ir/simple.sy"
./cmmc --ir tests/ir/simple.sy -o /tmp/cmmc_simple.ll
grep -q "define i32 @main" /tmp/cmmc_simple.ll
grep -q "ret i32" /tmp/cmmc_simple.ll
grep -q "label_main_ENTRY" /tmp/cmmc_simple.ll

echo "[ir] ir/functions.sy"
./cmmc --ir tests/ir/functions.sy -o /tmp/cmmc_functions.ll
grep -q "define i32 @add(i32 %a.arg, i32 %b.arg)" /tmp/cmmc_functions.ll
grep -q "call i32 @add" /tmp/cmmc_functions.ll
grep -q "declare i32 @getint()" /tmp/cmmc_functions.ll

echo "[ir] ir/if_else.sy"
./cmmc --ir tests/ir/if_else.sy -o /tmp/cmmc_if.ll
grep -q "br i1" /tmp/cmmc_if.ll
grep -q "label_if_then" /tmp/cmmc_if.ll

echo "[ir] ir/consts.sy"
./cmmc --ir tests/ir/consts.sy -o /tmp/cmmc_consts.ll
grep -q "@base = constant i32 7" /tmp/cmmc_consts.ll
grep -q "ret i32" /tmp/cmmc_consts.ll

echo "[ir] valid/float_mix.sy"
./cmmc --ir tests/valid/float_mix.sy -o /tmp/cmmc_float.ll
grep -q "define float @id" /tmp/cmmc_float.ll
grep -q "fadd float" /tmp/cmmc_float.ll

echo "[dump-all]"
rm -rf /tmp/cmmc_out
./cmmc --dump-all tests/valid/functions.sy -o /tmp/cmmc_out >/tmp/cmmc_dump_all.out
test -f /tmp/cmmc_out/functions.tokens
test -f /tmp/cmmc_out/functions.symtab
test -f /tmp/cmmc_out/functions.ast.dot
test -f /tmp/cmmc_out/functions.ll

echo "all tests passed"
