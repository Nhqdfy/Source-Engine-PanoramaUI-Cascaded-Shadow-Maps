#!/usr/bin/env python3
#==============================================================================
# Rebuilds a stdshaders/fxctmp9/<shader>.inc combo index class from the
# // STATIC: / // DYNAMIC: declarations of the matching .fxc.
#
# This tree compiles stdshaders at runtime (DYNAMIC_SHADER_COMPILE): the engine
# parses the .fxc comments in materialsystem/shaderapidx9/vertexshaderdx8.cpp
# (CShaderManager::FindOrCreateShaderCombos) and turns the shader index into the
# pixel/vertex shader macros with a mixed radix decode in DECLARATION ORDER:
#
#     nCombo = nStaticIndex + nDynamicIndex
#     for each DYNAMIC combo (declaration order): val = nCombo % count + min; nCombo /= count
#     for each STATIC  combo (declaration order): val = nCombo % count + min; nCombo /= count
#
# The shader DLL builds nStaticIndex/nDynamicIndex from the .inc that the
# stdshader sources include, so the GetIndex() weights in the .inc have to match
# that decode:
#
#     first DYNAMIC combo  : weight = 1
#     later DYNAMIC combos : weight *= count of every earlier DYNAMIC combo
#     first STATIC combo   : weight = product of all DYNAMIC counts
#     later STATIC combos  : weight *= count of every earlier STATIC combo
#
# i.e. the DYNAMIC combos are the low digits of the index and the STATIC combos
# continue above them.  Getting this wrong silently compiles the wrong
# permutation (for example CASCADED_SHADOW_MAPPING decodes as 0 no matter what
# the shader sets), which is what happened to a hand written
# lightmappedgeneric_ps30.inc.
#
# Valve ships utils/fxc_prep.pl for this; perl is not always available, so this
# script re-implements just the combo/index part.  Usage:
#
#     python utils/fxc_prep_combo_inc.py materialsystem/stdshaders/lightmappedgeneric_ps30.fxc
#
#==============================================================================

import os
import re
import sys

COMBO_RE = re.compile(r'\s*(DYNAMIC|STATIC)\s*:\s*"([^"]*)"\s*"(\d+)\.\.(\d+)"')


def parse_combos(path, target):
    """Mirror the PC/non-X360 branch of CShaderManager::FindOrCreateShaderCombos."""
    static = []
    dynamic = []
    tag = target.lower()

    with open(path, 'r', encoding='latin-1') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if len(line) < 2 or line[0] != '/' or line[1] != '/':
                continue

            low = line.lower()

            # platform filter: the PC parser drops anything tagged for the consoles
            if '[360]' in low or '[xbox]' in low:
                continue

            # shader model filter: a line that names another model is dropped
            if '[ps' in low and '_ps20' in tag and '_ps20b' not in tag and '[ps20]' not in low:
                continue
            if '[ps' in low and '_ps20b' in tag and '[ps20b]' not in low:
                continue
            if '[ps' in low and '_ps30' in tag and '[ps30]' not in low:
                continue
            if '[vs' in low and '_vs20' in tag and '[vs20]' not in low:
                continue
            if '[vs' in low and '_vs30' in tag and '[vs30]' not in low:
                continue

            m = COMBO_RE.match(line[2:])
            if not m:
                continue

            kind, name, lo, hi = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
            (dynamic if kind == 'DYNAMIC' else static).append((name, lo, hi))

    return static, dynamic


def combo_weights(combos, base):
    """weight[i] = base * product(count of combos before i)."""
    out = []
    weight = base
    for (name, lo, hi) in combos:
        out.append(weight)
        weight *= (hi - lo + 1)
    return out


def emit_class(lines, shader, combos, weights, is_static):
    kind = 'Static' if is_static else 'Dynamic'
    missing = 'static' if is_static else 'dynamic'

    lines.append('class %s_%s_Index' % (shader, kind))
    lines.append('{')
    for (name, lo, hi) in combos:
        lines.append('private:')
        lines.append('\tint m_n%s;' % name)
        lines.append('#ifdef _DEBUG')
        lines.append('\tbool m_b%s;' % name)
        lines.append('#endif')
        lines.append('public:')
        lines.append('\tvoid Set%s( int i )' % name)
        lines.append('\t{')
        lines.append('\t\tAssert( i >= %d && i <= %d );' % (lo, hi))
        lines.append('\t\tm_n%s = i;' % name)
        lines.append('#ifdef _DEBUG')
        lines.append('\t\tm_b%s = true;' % name)
        lines.append('#endif')
        lines.append('\t}')
        lines.append('\tvoid Set%s( bool i )' % name)
        lines.append('\t{')
        lines.append('\t\tm_n%s = i ? 1 : 0;' % name)
        lines.append('#ifdef _DEBUG')
        lines.append('\t\tm_b%s = true;' % name)
        lines.append('#endif')
        lines.append('\t}')

    lines.append('public:')
    lines.append('\t%s_%s_Index()' % (shader, kind))
    lines.append('\t{')
    for (name, lo, hi) in combos:
        lines.append('#ifdef _DEBUG')
        lines.append('\t\tm_b%s = false;' % name)
        lines.append('#endif // _DEBUG')
        lines.append('\t\tm_n%s = 0;' % name)
    lines.append('\t}')
    # SE port: the combo index is 64 bit - CS:GO's map/shader combo tables exceed INT_MAX
    lines.append('\tint64 GetIndex()')
    lines.append('\t{')
    lines.append('\t\t// Asserts to make sure that we aren\'t using any skipped combinations.')
    lines.append('\t\t// Asserts to make sure that we are setting all of the combination vars.')
    lines.append('#ifdef _DEBUG')
    lines.append('\t\tbool bAll%sVarsDefined = %s;'
                 % (kind, ' && '.join('m_b%s' % name for (name, lo, hi) in combos)))
    lines.append('\t\tAssert( bAll%sVarsDefined );' % kind)
    lines.append('#endif // _DEBUG')
    if combos:
        terms = ['( %d * m_n%s )' % (weight, name)
                 for (name, lo, hi), weight in zip(combos, weights)]
        lines.append('\t\treturn %s + 0;' % ' + '.join(terms))
    else:
        lines.append('\t\treturn 0;')
    lines.append('\t}')
    lines.append('};')
    lines.append('#define shader%sTest_%s %s + 0'
                 % (kind, shader, ' + '.join('psh_forgot_to_set_%s_%s' % (missing, name)
                                             for (name, lo, hi) in combos)))


def main(argv):
    if len(argv) < 2:
        sys.stderr.write('usage: %s <shader.fxc> [out.inc]\n' % argv[0])
        return 1

    # Model shaders share ONE _ps2x.fxc for ps20/ps20b/ps30, so the shader name the engine asks for
    # (which drives the [ps20b]/[ps30] tag filtering) cannot be derived from the file name.
    # Pass it as --target=<name>; fall back to the file name otherwise.
    positional = [a for a in argv[1:] if not a.startswith('--')]
    target = next((a.split('=', 1)[1] for a in argv[1:] if a.startswith('--target=')), None)
    fxc = positional[0]
    shader = target or os.path.splitext(os.path.basename(fxc))[0]
    out = positional[1] if len(positional) > 1 else os.path.join(os.path.dirname(fxc), 'fxctmp9', shader + '.inc')

    static, dynamic = parse_combos(fxc, shader)
    if not static and not dynamic:
        sys.stderr.write('%s: no // STATIC: or // DYNAMIC: declarations found for %s\n' % (fxc, shader))
        return 1

    dyn_weights = combo_weights(dynamic, 1)
    dyn_total = dyn_weights[-1] * (dynamic[-1][2] - dynamic[-1][1] + 1) if dynamic else 1
    sta_weights = combo_weights(static, dyn_total)

    lines = []
    lines.append('// AUTOGENERATED by utils/fxc_prep_combo_inc.py from %s - do not edit.'
                 % os.path.basename(fxc))
    lines.append('// The weights below mirror the runtime combo decode in')
    lines.append('// materialsystem/shaderapidx9/vertexshaderdx8.cpp and must match the')
    lines.append('// declaration order of the .fxc.')
    lines.append('#include "shaderlib/cshader.h"')
    emit_class(lines, shader, static, sta_weights, True)
    emit_class(lines, shader, dynamic, dyn_weights, False)

    with open(out, 'w', encoding='latin-1', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')

    sys.stdout.write('%s: %d static x %d dynamic combos -> %s (space %d)\n'
                     % (shader, len(static), len(dynamic), out,
                        dyn_total * (sta_weights[-1] * (static[-1][2] - static[-1][1] + 1)
                                     if static else 1)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))