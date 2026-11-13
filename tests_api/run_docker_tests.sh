#!/usr/bin/env bash
# Levanta el compose, espera el healthcheck, lanza peticiones con curl y lo baja.
# MOCK=1 usa el compilador falso en lugar del real.
set -u
cd "$(dirname "$0")/.."
FILES=(-f docker-compose.yml)
[ "${MOCK:-0}" = "1" ] && FILES+=(-f tests_api/docker-compose.mock.yml)
export COMPOSE_PROJECT_NAME=mcpp_test
URL=${URL:-http://127.0.0.1:8080}
fail=0
cleanup() { docker compose "${FILES[@]}" down -v --remove-orphans >/dev/null 2>&1; }
trap cleanup EXIT

post() { curl -sS -m 40 -X POST "$URL/api/compile" -H 'Content-Type: application/json' --data-binary @-; }
check() { # nombre, texto, patron
  if printf '%s' "$2" | grep -Eq "$3"; then echo "ok   - $1"; else echo "FAIL - $1"; echo "       respuesta: $(printf '%s' "$2" | head -c 400)"; fail=1; fi
}
checkno() {
  if printf '%s' "$2" | grep -Eq "$3"; then echo "FAIL - $1"; fail=1; else echo "ok   - $1"; fi
}
jstr() { python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'; }

docker compose "${FILES[@]}" up -d --build || { echo "no se pudo levantar el compose"; exit 1; }
echo "esperando healthcheck..."
for i in $(seq 1 60); do
  st=$(docker inspect --format '{{.State.Health.Status}}' "$(docker compose "${FILES[@]}" ps -q web)" 2>/dev/null)
  [ "$st" = "healthy" ] && break
  sleep 2
done
check "healthcheck healthy" "$st" '^healthy$'

check "GET /api/health" "$(curl -sS $URL/api/health)" '"status": "ok"'
check "GET /api/status" "$(curl -sS $URL/api/status)" '"run_available": true'
check "GET /api/examples" "$(curl -sS $URL/api/examples)" 'hola_mundo'
check "frontend en /" "$(curl -sS $URL/)" '<title>'
checkno "sin CORS por defecto" "$(curl -sSI $URL/api/health)" 'Access-Control'


hola=$(curl -sS $URL/api/examples/hola_mundo | python3 -c 'import json,sys; print(json.dumps({"code": json.load(sys.stdin)["code"], "mode": "all"}))')
r=$(printf '%s' "$hola" | post)
check "compilar y ejecutar valido" "$r" '"success": true'
check "salida del programa" "$r" '"stdout": "[^"]+'
check "codigo de salida 0" "$r" '"exit_code": 0'

err=$(curl -sS $URL/api/examples/error_de_tipo | python3 -c 'import json,sys; print(json.dumps({"code": json.load(sys.stdin)["code"], "mode": "all"}))')
r=$(printf '%s' "$err" | post)
check "error de tipo" "$r" '"success": false'
check "fase typecheck" "$r" '"phase": "typecheck"'

cin=$(curl -sS $URL/api/examples/lectura_cin | python3 -c 'import json,sys; d=json.load(sys.stdin); print(json.dumps({"code": d["code"], "stdin": d["stdin"], "mode": "run"}))')
r=$(printf '%s' "$cin" | post)
[ "${MOCK:-0}" = "1" ] || check "stdin con cin" "$r" 'Hola, Marco'

loop=$(printf 'int main() { while (true) { } return 0; } // RUN_LOOP' | jstr | sed 's/^/{"code": /; s/$/, "mode": "run", "timeout": 1}/')
r=$(printf '%s' "$loop" | post)
check "timeout" "$r" 'tiempo limite excedido'

r=$(printf '{"code": 5}' | post)
check "peticion invalida" "$r" '"phase": "request"'
checkno "sin rutas internas" "$r$(printf '%s' "$loop" | post)" '/app/|/tmp/mc_'


if [ $fail -eq 0 ]; then echo "todas las pruebas pasaron"; else echo "hubo fallos"; fi
exit $fail
