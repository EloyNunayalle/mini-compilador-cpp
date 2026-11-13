#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "print_visitor.h"
#include "type_checker.h"
#include "gencode.h"
#include "optimizer.h"
#include "api.h"
#include <iterator>
#include <sstream>

using namespace std;

string sinExtension(const string& ruta) {
    size_t pos = ruta.find_last_of(".");
    size_t barra = ruta.find_last_of("/");
    if (pos == string::npos || (barra != string::npos && pos < barra)) return ruta;
    return ruta.substr(0, pos);
}

void mostrarAyuda(const char* prog) {
    cout << "Mini-Compilador C++\n\n";
    cout << "Uso: " << prog << " <archivo_fuente.cpp> [opciones]\n\n";
    cout << "Opciones:\n";
    cout << "  --scan    : Ejecuta solo el análisis léxico y muestra/guarda tokens.\n";
    cout << "  --ast     : Muestra el Árbol de Sintaxis Abstracta (AST) formateado.\n";
    cout << "  --check   : Ejecuta el análisis semántico y comprobación de tipos.\n";
    cout << "  --all     : Ejecuta todas las fases del Frontend y genera reportes.\n";
    cout << "  --asm [salida.s] : Genera ensamblador GNU AS (AT&T, Linux x86-64). Por defecto <archivo>.s\n";
    cout << "  -O0       : Sin optimizaciones (por defecto).\n";
    cout << "  -O1       : Activa los pases de optimización sobre el AST (inline, fold, sethi).\n";
    cout << "  --passes a,b : Con -O1, elige los pases (por defecto inline,fold,sethi).\n";
    cout << "  --report-opt : Con -O1, imprime las transformaciones aplicadas.\n";
    cout << "  --api --mode <scan|ast|check|asm|all> [-O1] [--passes ...] : lee el código por stdin y emite JSON.\n";
    cout << "  --version : Muestra la versión.\n";
    cout << "  (por defecto: ejecuta parseo y verificación semántica completa)\n";
}

static vector<string> splitList(const string& s) {
    vector<string> out;
    string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int main(int argc, const char* argv[]) {
    // opciones comunes: -O0/-O1, --passes, --report-opt, --api, --mode, --version
    vector<string> pos;
    bool optimize = false, reportOpt = false, apiMode = false;
    string apiModeName = "all";
    vector<string> passes;
    bool passesGiven = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-O1") optimize = true;
        else if (a == "-O0") optimize = false;
        else if (a == "--report-opt") reportOpt = true;
        else if (a == "--api") apiMode = true;
        else if (a == "--version") {
            cout << "mini-compilador-cpp 1.0\n";
            return 0;
        } else if (a == "--mode" && i + 1 < argc) apiModeName = argv[++i];
        else if (a == "--passes" && i + 1 < argc) {
            passes = splitList(argv[++i]);
            passesGiven = true;
        } else pos.push_back(a);
    }
    if (!passesGiven) passes = {"inline", "fold", "sethi"};

    if (apiMode) {
        ApiOptions ao;
        ao.mode = apiModeName;
        ao.optimize = optimize;
        ao.passes = passes;
        string source((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
        cout << runApi(source, ao) << endl;
        return 0;
    }

    if (pos.empty()) {
        mostrarAyuda(argv[0]);
        return 1;
    }

    string archivoEntrada = pos[0];
    string opcion = (pos.size() >= 2) ? pos[1] : "--default";
    string archivoSalida = (pos.size() >= 3) ? pos[2] : "";

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

    // Optimización opcional (-O1): pases sobre el AST y nueva verificación
    if (optimize) {
        OptReport rep;
        bool ok = runOptimizations(ast, checker, passes, rep);
        if (!ok) {
            cerr << "[AVISO] optimización descartada: " << (rep.log.empty() ? "" : rep.log.back()) << "\n";
            delete ast;
            Scanner sc2(inputContent.c_str());
            Parser p2(&sc2);
            ast = p2.parseProgram();
            checker.check(ast);
        } else if (reportOpt) {
            cout << "\n=== [OPTIMIZACIONES -O1] ===\n";
            for (const PassResult& r : rep.results) {
                cout << "  " << r.name << ": " << r.count << " transformaciones\n";
                for (const string& d : r.details) cout << "    " << d << "\n";
            }
        }
    }

    // 4. Generación de código (ensamblador x86-64 AT&T)
    if (opcion == "--asm") {
        string asmFile = archivoSalida.empty() ? sinExtension(archivoEntrada) + ".s" : archivoSalida;
        ofstream outAsm(asmFile);
        if (!outAsm.is_open()) {
            cerr << "Error: no se pudo escribir " << asmFile << endl;
            delete ast;
            return 1;
        }
        try {
            Gencode gen(checker.records);
            gen.generate(ast, outAsm);
        } catch (const exception& e) {
            cerr << "\n[FALLO GENCODE] " << e.what() << endl;
            delete ast;
            return 1;
        }
        outAsm.close();
        cout << "Ensamblador generado en " << asmFile << "\n";
    }

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
