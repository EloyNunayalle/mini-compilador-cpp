#!/usr/bin/env python3
"""Pruebas del Gencode: genera .s, ensambla con gcc junto al runtime, ejecuta y compara.

Cada prueba es tests_gencode/<nombre>.cpp con archivos opcionales:
  <nombre>.in        entrada estandar
  <nombre>.expected  salida esperada seguida de una linea "[exit N]"
Marcas dentro del .cpp:
  // ASM-CONTAINS: texto     el .s generado debe contener el texto
  // ASM-ABSENT: texto       el .s generado no debe contener el texto
  // EXPECT-ERROR: texto     el compilador debe rechazar el programa con ese texto
                             (en estos casos no hay ejecucion)

En Linux x86-64 usa gcc local; en otras plataformas usa Docker (gcc para linux/amd64).
Con --update-expected genera los .expected compilando cada .cpp con g++ real (oraculo).
Con --asan enlaza con AddressSanitizer y LeakSanitizer y falla si reportan algo.
"""
import glob
import os
import platform
import re
import shutil
import subprocess
import sys

COMPILER = "./compiler"
TEST_DIR = "tests_gencode"
OUT_DIR = os.path.join("outputs", "gencode")
RUNTIME = os.path.join("runtime", "runtime.c")
DOCKER_IMAGE = "gcc:13"


def es_linux_x86():
    return platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")


def marcas(path, nombre):
    res = []
    with open(path, encoding="utf-8") as f:
        for linea in f:
            m = re.search(r"//\s*" + nombre + r":\s*(.*)$", linea)
            if m:
                res.append(m.group(1).rstrip())
    return res


def ejecutar_script(script_path):
    if es_linux_x86():
        return subprocess.run(["sh", script_path], capture_output=True, text=True)
    cmd = ["docker", "run", "--rm", "--platform", "linux/amd64",
           "-v", os.getcwd() + ":/w", "-w", "/w", DOCKER_IMAGE, "sh", "/w/" + script_path]
    return subprocess.run(cmd, capture_output=True, text=True)


def escribir_script(path, lineas):
    with open(path, "w") as f:
        f.write("#!/bin/sh\n")
        f.write("\n".join(lineas) + "\n")


def entrada_de(nombre):
    p = os.path.join(TEST_DIR, nombre + ".in")
    return p if os.path.exists(p) else "/dev/null"


def actualizar_esperados(nombres):
    lineas = []
    for n in nombres:
        src = os.path.join(TEST_DIR, n + ".cpp")
        exe = os.path.join(OUT_DIR, n + ".ref")
        out = os.path.join(TEST_DIR, n + ".expected")
        lineas.append(f"g++ -std=c++17 -w {src} -o {exe} || {{ echo 'FALLO g++ {n}'; continue; }}")
        lineas.append(f"{exe} < {entrada_de(n)} > {out}; echo \"[exit $?]\" >> {out}")
    script = os.path.join(OUT_DIR, "oraculo.sh")
    escribir_script(script, lineas)
    res = ejecutar_script(script)
    print(res.stdout + res.stderr)


def main():
    actualizar = "--update-expected" in sys.argv
    asan = "--asan" in sys.argv
    flags = "-g -fsanitize=address" if asan else ""
    filtro = [a for a in sys.argv[1:] if not a.startswith("--")]

    if subprocess.run(["make", "-s"], capture_output=True).returncode != 0:
        print("Error al compilar el compilador con make")
        return 1

    shutil.rmtree(OUT_DIR, ignore_errors=True)
    os.makedirs(OUT_DIR, exist_ok=True)

    archivos = sorted(glob.glob(os.path.join(TEST_DIR, "*.cpp")))
    nombres = [os.path.basename(a)[:-4] for a in archivos]
    if filtro:
        nombres = [n for n in nombres if any(f in n for f in filtro)]

    if actualizar:
        actualizar_esperados([n for n in nombres if not marcas(os.path.join(TEST_DIR, n + ".cpp"), "EXPECT-ERROR")])
        return 0

    resultados = {}
    por_ejecutar = []
    for n in nombres:
        src = os.path.join(TEST_DIR, n + ".cpp")
        asm = os.path.join(OUT_DIR, n + ".s")
        esperado_error = marcas(src, "EXPECT-ERROR")
        proc = subprocess.run([COMPILER, src, "--asm", asm], capture_output=True, text=True)
        salida = proc.stdout + proc.stderr

        if esperado_error:
            if proc.returncode == 0:
                resultados[n] = "el compilador no detecto el error"
            elif not all(e in salida for e in esperado_error):
                resultados[n] = "mensaje de error distinto: " + salida.strip().splitlines()[-2][:160]
            else:
                resultados[n] = None
            continue

        if proc.returncode != 0:
            resultados[n] = "el compilador fallo: " + salida.strip()[-300:]
            continue

        texto_asm = open(asm).read()
        falla = None
        for m in marcas(src, "ASM-CONTAINS"):
            if m not in texto_asm:
                falla = "el .s no contiene: " + m
        for m in marcas(src, "ASM-ABSENT"):
            if m in texto_asm:
                falla = "el .s contiene: " + m
        if falla:
            resultados[n] = falla
            continue
        por_ejecutar.append(n)

    lineas = []
    for n in por_ejecutar:
        asm = os.path.join(OUT_DIR, n + ".s")
        exe = os.path.join(OUT_DIR, n + ".bin")
        out = os.path.join(OUT_DIR, n + ".out")
        lineas.append(f"gcc {flags} {asm} {RUNTIME} -o {exe} 2> {out}.err || {{ echo 'FALLO ensamblado' > {out}; cat {out}.err >> {out}; continue; }}")
        lineas.append(f"{exe} < {entrada_de(n)} > {out} 2>{out}.rt; echo \"[exit $?]\" >> {out}")
    if lineas:
        script = os.path.join(OUT_DIR, "ejecutar.sh")
        escribir_script(script, lineas)
        res = ejecutar_script(script)
        if res.returncode != 0:
            print(res.stdout + res.stderr)

    for n in por_ejecutar:
        out = os.path.join(OUT_DIR, n + ".out")
        exp = os.path.join(TEST_DIR, n + ".expected")
        if not os.path.exists(out):
            resultados[n] = "no se produjo salida"
            continue
        if not os.path.exists(exp):
            resultados[n] = "falta " + exp + " (use --update-expected)"
            continue
        real = open(out, errors="replace").read()
        esperado = open(exp, errors="replace").read()
        resultados[n] = None if real == esperado else "salida distinta"
        rt = out + ".rt"
        if asan and os.path.exists(rt) and "Sanitizer" in open(rt, errors="replace").read():
            resultados[n] = "sanitizer reporto errores (ver " + rt + ")"

    print("=" * 78)
    pasaron = 0
    for n in nombres:
        r = resultados.get(n, "sin resultado")
        print(f"{n:<34} {'PASO' if r is None else 'FALLO: ' + r}")
        if r is None:
            pasaron += 1
    print("=" * 78)
    print(f"Pruebas de Gencode: {pasaron}/{len(nombres)} superadas")
    return 0 if pasaron == len(nombres) else 1


if __name__ == "__main__":
    sys.exit(main())
