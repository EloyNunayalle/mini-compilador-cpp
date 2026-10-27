#!/usr/bin/env python3
import os
import glob
import subprocess
import shutil

COMPILER = "./compiler"
INPUT_DIR = "inputs"
OUTPUT_DIR = "outputs"

def compilar_compilador():
    print("Compilando el compilador con 'make'...")
    res = subprocess.run(["make"], capture_output=True, text=True)
    if res.returncode != 0:
        print("Error de compilación:\n", res.stderr)
        exit(1)
    print("Compilador listo: ./compiler\n")

def run_tests():
    shutil.rmtree(OUTPUT_DIR, ignore_errors=True)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    archivos = sorted(glob.glob(os.path.join(INPUT_DIR, "*.cpp")))
    if not archivos:
        print("No se encontraron archivos .cpp en inputs/")
        return

    print("=" * 80)
    print(f"{'CASO DE PRUEBA':<35} | {'EXPECTATIVA':<15} | {'RESULTADO':<12} | {'ESTADO':<10}")
    print("=" * 80)

    reporte_lineas = []
    total = len(archivos)
    pasaron = 0

    for filepath in archivos:
        nombre = os.path.basename(filepath)
        es_caso_error = "error" in nombre.lower()
        expectativa = "Detectar Error" if es_caso_error else "Compilar OK"

        # Ejecutar compilador con --all
        proc = subprocess.run([COMPILER, filepath, "--all"], capture_output=True, text=True)
        ret = proc.returncode

        if es_caso_error:
            # Debe fallar con código != 0 y reportar el error
            if ret != 0:
                resultado = "Error Detectado"
                estado = "PASÓ"
                pasaron += 1
            else:
                resultado = "No Detectado"
                estado = "FALLÓ"
        else:
            # Debe pasar con código == 0
            if ret == 0:
                resultado = "Frontend OK"
                estado = "PASÓ"
                pasaron += 1
            else:
                resultado = "Fallo Frontend"
                estado = "FALLÓ"

        simbolo = "✓" if estado == "PASÓ" else "✗"
        print(f"{nombre:<35} | {expectativa:<15} | {resultado:<12} | {simbolo} {estado:<8}")

        linea_reporte = f"[{estado}] {nombre} ({expectativa}): {resultado}\n"
        if ret != 0 and es_caso_error:
            linea_reporte += f"  Salida de error:\n  {proc.stderr.strip() or proc.stdout.strip()}\n"
        reporte_lineas.append(linea_reporte)

    print("=" * 80)
    print(f"Resumen de Pruebas: {pasaron}/{total} pruebas superadas exitosamente ({(pasaron/total)*100:.1f}%)")
    print("=" * 80)

    # Mover archivos generados a outputs/
    for tok_file in glob.glob(os.path.join(INPUT_DIR, "*_tokens.txt")):
        shutil.move(tok_file, os.path.join(OUTPUT_DIR, os.path.basename(tok_file)))
    for ast_file in glob.glob(os.path.join(INPUT_DIR, "*_ast.txt")):
        shutil.move(ast_file, os.path.join(OUTPUT_DIR, os.path.basename(ast_file)))

    reporte_file = os.path.join(OUTPUT_DIR, "reporte_avance_30.txt")
    with open(reporte_file, "w") as f:
        f.write("=== REPORTE DE EJECUCIÓN - AVANCE 30% PROYECTO 2 CS3402 ===\n")
        f.write(f"Total pruebas: {total} | Pasaron: {pasaron} | Fallaron: {total - pasaron}\n\n")
        for l in reporte_lineas:
            f.write(l + "\n")

    print(f"\nReporte guardado en {reporte_file}")
    print(f"Tokens y ASTs generados en carpeta '{OUTPUT_DIR}/'")

if __name__ == "__main__":
    compilar_compilador()
    run_tests()
