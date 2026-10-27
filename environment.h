#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename T>
class Environment {
private:
    // Un nivel (scope): asocia cada nombre de identificador con su información
    using Rib = std::unordered_map<std::string, T>;

    // Pila de niveles; ribs.back() es el scope más interno
    std::vector<Rib> ribs;

    // Índice del nivel más interno que contiene la variable, o -1 si no existe
    [[nodiscard]] int search_rib(const std::string& var) const {
        for (int idx = static_cast<int>(ribs.size()) - 1; idx >= 0; --idx) {
            if (ribs[idx].count(var)) return idx;
        }
        return -1;
    }

    void ensure_level() {
        if (ribs.empty()) {
            ribs.emplace_back();
        }
    }

public:
    Environment() = default;

    void clear() noexcept { ribs.clear(); }

    // Abre un nuevo nivel (scope)
    void add_level() { ribs.emplace_back(); }

    // Cierra el nivel más interno
    bool remove_level() noexcept {
        if (ribs.empty()) return false;
        ribs.pop_back();
        return true;
    }

    // Declara una variable en el nivel actual
    void add_var(const std::string& var, T value = T{}) {
        ensure_level();
        ribs.back()[var] = std::move(value);
    }

    // Actualiza una variable existente en el ámbito más cercano donde se definió
    bool update(const std::string& x, T v) {
        const int idx = search_rib(x);
        if (idx < 0) return false;
        ribs[idx][x] = std::move(v);
        return true;
    }

    // Indica si la variable existe en algún ámbito abierto
    [[nodiscard]] bool check(const std::string& x) const {
        return search_rib(x) >= 0;
    }

    // Indica si la variable ya existe en el ámbito actual (para detectar redeclaraciones)
    [[nodiscard]] bool check_current_level(const std::string& x) const {
        if (ribs.empty()) return false;
        return ribs.back().count(x) > 0;
    }

    // Devuelve el valor asociado a la variable, o T{} si no existe
    [[nodiscard]] T lookup(const std::string& x) const {
        const int idx = search_rib(x);
        if (idx >= 0) return ribs[idx].at(x);
        return T{};
    }

    bool lookup(const std::string& x, T& v) const {
        const int idx = search_rib(x);
        if (idx < 0) return false;
        v = ribs[idx].at(x);
        return true;
    }

    [[nodiscard]] std::size_t levels() const noexcept {
        return ribs.size();
    }
};

#endif // ENVIRONMENT_H
