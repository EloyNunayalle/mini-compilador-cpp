#!/usr/bin/env python3
"""Pruebas de optimizacion: compara -O0 contra -O1.

Para cada programa ejecutable de tests_opt/ y de tests_gencode/ se genera el ensamblador
con -O0 y con -O1, se ejecutan ambos y se exige la misma salida estandar y el mismo
codigo de retorno. En tests_gencode ademas se compara contra el .expected del oraculo.
Marcas dentro del .cpp (solo se evaluan sobre el ensamblador de -O1):
  // OPT-ASM-CONTAINS: texto
  // OPT-ASM-ABSENT: texto
En Linux x86-64 usa gcc local; en otras plataformas usa Docker (gcc para linux/amd64).
"""
import glob
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_gencode_tests import ejecutar_script, escribir_script, marcas, entrada_de, RUNTIME  # noqa: E402

COMPILER = "./compiler"
OUT = os.path.join("outputs", "opt")


def main():
    if subprocess.run(["make", "-s"], capture_output=True).returncode != 0:
        print("Error al compilar el compilador con make")
        return 1
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(OUT, exist_ok=True)
    filtro = [a for a in sys.argv[1:] if not a.startswith("--")]

    casos = []
    for d in ("tests_opt", "tests_gencode"):
        for f in sorted(glob.glob(os.path.join(d, "*.cpp"))):
            n = os.path.basename(f)[:-4]
            if marcas(f, "EXPECT-ERROR"):
                continue
            if filtro and not any(x in n for x in filtro):
                continue
            casos.append((d, n, f))

    resultados = {}
    lineas = []
    listos = []
    for d, n, f in casos:
        ok = True
        for nivel in ("0", "1"):
            asm = os.path.join(OUT, f"{n}.O{nivel}.s")
            p = subprocess.run([COMPILER, f, "--asm", asm, "-O" + nivel], capture_output=True, text=True)
            if p.returncode != 0:
                resultados[n] = f"el compilador fallo con -O{nivel}: " + (p.stdout + p.stderr)[-200:]
                ok = False
                break
        if not ok:
            continue
        asm1 = open(os.path.join(OUT, f"{n}.O1.s")).read()
        falla = None
        for m in marcas(f, "OPT-ASM-CONTAINS"):
            if m not in asm1:
                falla = "el .s de -O1 no contiene: " + m
        for m in marcas(f, "OPT-ASM-ABSENT"):
            if m in asm1:
                falla = "el .s de -O1 contiene: " + m
        if falla:
            resultados[n] = falla
            continue
        for nivel in ("0", "1"):
            asm = os.path.join(OUT, f"{n}.O{nivel}.s")
            exe = os.path.join(OUT, f"{n}.O{nivel}.bin")
            out = os.path.join(OUT, f"{n}.O{nivel}.out")
            lineas.append(f"gcc {asm} {RUNTIME} -o {exe} 2> {out}.err || {{ echo 'FALLO ensamblado' > {out}; continue; }}")
            lineas.append(f"{exe} < {entrada_de(n) if d == 'tests_gencode' else '/dev/null'} > {out} 2>/dev/null; echo \"[exit $?]\" >> {out}")
        listos.append((d, n))

    if lineas:
        script = os.path.join(OUT, "ejecutar.sh")
        escribir_script(script, lineas)
        r = ejecutar_script(script)
        if r.returncode != 0:
            print(r.stdout + r.stderr)

    for d, n in listos:
        o0 = os.path.join(OUT, f"{n}.O0.out")
        o1 = os.path.join(OUT, f"{n}.O1.out")
        if not (os.path.exists(o0) and os.path.exists(o1)):
            resultados[n] = "no se produjo salida"
            continue
        a, b = open(o0, errors="replace").read(), open(o1, errors="replace").read()
        if a != b:
            resultados[n] = "-O0 y -O1 difieren"
            continue
        if d == "tests_gencode":
            exp = os.path.join(d, n + ".expected")
            if os.path.exists(exp) and open(exp, errors="replace").read() != b:
                resultados[n] = "la salida difiere del oraculo"
                continue
        resultados[n] = None

    print("=" * 78)
    pasaron = 0
    for _, n, _f in casos:
        r = resultados.get(n, "sin resultado")
        print(f"{n:<34} {'PASO' if r is None else 'FALLO: ' + r}")
        pasaron += r is None
    print("=" * 78)
    print(f"Pruebas de optimizacion: {pasaron}/{len(casos)} superadas")
    return 0 if pasaron == len(casos) else 1


if __name__ == "__main__":
    sys.exit(main())
