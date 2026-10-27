#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "print_visitor.h"
#include "type_checker.h"

using namespace std;

string sinExtension(const string& ruta) {
    size_t pos = ruta.find_last_of(".");
    size_t barra = ruta.find_last_of("/");
    if (pos == string::npos || (barra != string::npos && pos < barra)) return ruta;
    return ruta.substr(0, pos);
}

void mostrarAyuda(const char* prog) {
    cout << "===============================================================\n";
    cout << "  Mini-Compilador C++ (UTEC CS3402 - Proyecto 2: Avance 30%)   \n";
    cout << "===============================================================\n";
    cout << "Uso: " << prog << " <archivo_fuente.cpp> [opciones]\n\n";
    cout << "Opciones:\n";
    cout << "  --scan    : Ejecuta solo el análisis léxico y muestra/guarda tokens.\n";
    cout << "  --ast     : Muestra el Árbol de Sintaxis Abstracta (AST) formateado.\n";
    cout << "  --check   : Ejecuta el análisis semántico y comprobación de tipos.\n";
    cout << "  --all     : Ejecuta todas las fases del Frontend y genera reportes.\n";
    cout << "  (por defecto: ejecuta parseo y verificación semántica completa)\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        mostrarAyuda(argv[0]);
        return 1;
    }

    string archivoEntrada = argv[1];
    string opcion = (argc >= 3) ? argv[2] : "--default";

    if (archivoEntrada == "-h" || archivoEntrada == "--help") {
        mostrarAyuda(argv[0]);
        return 0;
    }

    ifstream infile(archivoEntrada);
    if (!infile.is_open()) {
        cerr << "Error: No se pudo abrir el archivo de entrada: " << archivoEntrada << endl;
        return 1;
    }

    string inputContent, line;
    while (getline(infile, line)) {
        inputContent += line + '\n';
    }
    infile.close();

    cout << "\n>>> Procesando: " << archivoEntrada << " (" << inputContent.length() << " bytes)\n";

    // 1. Análisis Léxico (Scanner)
    if (opcion == "--scan") {
        Scanner scanner(inputContent.c_str());
        cout << "\n=== [FASE 1: ANÁLISIS LÉXICO] ===\n";
        vector<Token*> tokens = scanner.tokenizeAll();
        for (Token* t : tokens) {
            cout << "  " << *t << "\n";
            delete t;
        }
        Scanner scannerFile(inputContent.c_str());
        ejecutar_scanner(&scannerFile, archivoEntrada);
        cout << "\nTokens guardados en " << sinExtension(archivoEntrada) << "_tokens.txt\n";
        return 0;
    }

    // 2. Análisis Sintáctico (Parser)
    Scanner scanner(inputContent.c_str());
    Parser parser(&scanner);
    Program* ast = nullptr;

    try {
        ast = parser.parseProgram();
    } catch (const exception& e) {
        cerr << "\n[FALLO SINTÁCTICO] " << e.what() << endl;
        return 1;
    }
    cout << "[OK] Análisis sintáctico completado exitosamente.\n";

    // Mostrar AST si se solicita o con --all
    if (opcion == "--ast" || opcion == "--all") {
        cout << "\n=== [FASE 2: ÁRBOL DE SINTAXIS ABSTRACTA (AST)] ===\n";
        ASTPrinter printer(cout);
        printer.print(ast);

        if (opcion == "--ast") {
            delete ast;
            return 0;
        }
    }

    // 3. Análisis Semántico y Comprobación de Tipos (TypeChecker)
    cout << "\n=== [FASE 3: ANÁLISIS SEMÁNTICO Y COMPROBACIÓN DE TIPOS] ===\n";
    TypeChecker checker;
    checker.check(ast);
    checker.printReport(cout);

    if (checker.hasErrors()) {
        cerr << "\n[FALLO SEMÁNTICO] Se encontraron " << checker.errors.size() << " error(es) semántico(s).\n";
        delete ast;
        return 1;
    }

    cout << "\n[ÉXITO] El programa es sintáctica y semánticamente correcto según la gramática EBNF.\n";

    // Si es --all, guardar el AST en archivo de texto
    if (opcion == "--all") {
        string astFile = sinExtension(archivoEntrada) + "_ast.txt";
        ofstream outAst(astFile);
        if (outAst.is_open()) {
            ASTPrinter printerFile(outAst);
            printerFile.print(ast);
            outAst.close();
            cout << "AST guardado en " << astFile << "\n";
        }
        Scanner scannerFile(inputContent.c_str());
        ejecutar_scanner(&scannerFile, archivoEntrada);
        cout << "Tokens guardados en " << sinExtension(archivoEntrada) << "_tokens.txt\n";
    }

    delete ast;
    return 0;
}
