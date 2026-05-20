#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

check_ir() {
  if command -v clang >/dev/null 2>&1; then
    clang -Wno-override-module -c -x ir "$1" -o /tmp/cmmc_ir_check.o
  fi
}

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

echo "[lex] lexer/sorted_test.sy"
if ./cmmc --lex tests/lexer/sorted_test.sy >/tmp/cmmc_sorted.tokens 2>/tmp/cmmc_sorted.err; then
  echo "expected lexer failure for illegal character" >&2
  exit 1
fi
grep -q $'VoId\t<KW,2>' /tmp/cmmc_sorted.tokens
grep -q $'_4_\t<IDN,_4_>' /tmp/cmmc_sorted.tokens
grep -q $'<=\t<OP,15>' /tmp/cmmc_sorted.tokens
grep -q $'31415926.535897\t<FLOAT,31415926.535897>' /tmp/cmmc_sorted.tokens
grep -q "illegal character" /tmp/cmmc_sorted.err

echo "[parse] valid/basic.sy"
./cmmc --parse tests/valid/basic.sy >/tmp/cmmc_basic.parse
grep -q "accept" /tmp/cmmc_basic.parse

echo "[parse] valid/functions.sy"
./cmmc --parse tests/valid/functions.sy >/tmp/cmmc_functions.parse
grep -q "accept" /tmp/cmmc_functions.parse

echo "[parse] valid/dangling_else.sy"
./cmmc --parse tests/valid/dangling_else.sy >/tmp/cmmc_dangling.parse
grep -q "accept" /tmp/cmmc_dangling.parse
grep -q "reduction(" /tmp/cmmc_dangling.parse

echo "[semantic] valid/nested_calls.sy"
./cmmc --semantic tests/valid/nested_calls.sy >/tmp/cmmc_nested_sem.out

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
check_ir /tmp/cmmc_simple.ll

echo "[ir] ir/functions.sy"
./cmmc --ir tests/ir/functions.sy -o /tmp/cmmc_functions.ll
grep -q "define i32 @add(i32 %a.arg, i32 %b.arg)" /tmp/cmmc_functions.ll
grep -q "call i32 @add" /tmp/cmmc_functions.ll
grep -q "declare i32 @getint()" /tmp/cmmc_functions.ll
check_ir /tmp/cmmc_functions.ll

echo "[ir] ir/if_else.sy"
./cmmc --ir tests/ir/if_else.sy -o /tmp/cmmc_if.ll
grep -q "br i1" /tmp/cmmc_if.ll
grep -q "label_if_then" /tmp/cmmc_if.ll
check_ir /tmp/cmmc_if.ll

echo "[ir] ir/consts.sy"
./cmmc --ir tests/ir/consts.sy -o /tmp/cmmc_consts.ll
grep -q "@base = constant i32 7" /tmp/cmmc_consts.ll
grep -q "ret i32" /tmp/cmmc_consts.ll
check_ir /tmp/cmmc_consts.ll

echo "[ir] ir/const_expr.sy"
./cmmc --ir tests/ir/const_expr.sy -o /tmp/cmmc_const_expr.ll
grep -q "@b = constant i32 4" /tmp/cmmc_const_expr.ll
check_ir /tmp/cmmc_const_expr.ll

echo "[ir] valid/float_mix.sy"
./cmmc --ir tests/valid/float_mix.sy -o /tmp/cmmc_float.ll
grep -q "define float @id" /tmp/cmmc_float.ll
grep -q "fadd float" /tmp/cmmc_float.ll
check_ir /tmp/cmmc_float.ll

echo "[ir] ir/float_global.sy"
./cmmc --ir tests/ir/float_global.sy -o /tmp/cmmc_float_global.ll
grep -q "load float, float\\* @g" /tmp/cmmc_float_global.ll
check_ir /tmp/cmmc_float_global.ll

echo "[ir] ir/float_const_expr.sy"
./cmmc --ir tests/ir/float_const_expr.sy -o /tmp/cmmc_float_const_expr.ll
grep -q "@b = constant i32 4" /tmp/cmmc_float_const_expr.ll
check_ir /tmp/cmmc_float_const_expr.ll

echo "[ir] ir/float_init_expr.sy"
./cmmc --ir tests/ir/float_init_expr.sy -o /tmp/cmmc_float_init_expr.ll
grep -q "@h = constant float 3.500000e+00" /tmp/cmmc_float_init_expr.ll
grep -q "@g = global float 1.000000e+00" /tmp/cmmc_float_init_expr.ll
check_ir /tmp/cmmc_float_init_expr.ll

echo "[ir] ir/float_call_cast.sy"
./cmmc --ir tests/ir/float_call_cast.sy -o /tmp/cmmc_float_call_cast.ll
grep -q "sitofp i32 1 to float" /tmp/cmmc_float_call_cast.ll
grep -q "call float @f(float" /tmp/cmmc_float_call_cast.ll
check_ir /tmp/cmmc_float_call_cast.ll

echo "[ir] ir/float_local_const.sy"
./cmmc --ir tests/ir/float_local_const.sy -o /tmp/cmmc_float_local_const.ll
grep -q "alloca i32" /tmp/cmmc_float_local_const.ll
grep -q "fadd float" /tmp/cmmc_float_local_const.ll
check_ir /tmp/cmmc_float_local_const.ll

echo "[dump-all]"
rm -rf /tmp/cmmc_out
./cmmc --dump-all tests/valid/functions.sy -o /tmp/cmmc_out >/tmp/cmmc_dump_all.out
test -f /tmp/cmmc_out/functions.tokens
test -f /tmp/cmmc_out/functions.symtab
test -f /tmp/cmmc_out/functions.ast.dot
test -f /tmp/cmmc_out/functions.ll

echo "all tests passed"
