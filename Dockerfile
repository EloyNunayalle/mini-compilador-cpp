# Imagen linux/amd64 (platform en compose): el ensamblador generado es x86-64.
FROM gcc:13 AS build
WORKDIR /src
COPY Makefile ./
COPY *.cpp *.h ./
RUN make -j2 compiler

FROM gcc:13
RUN useradd --system --uid 10001 --home-dir /nonexistent --shell /usr/sbin/nologin app
WORKDIR /app
COPY --from=build /src/compiler /app/compiler
COPY runtime /app/runtime
COPY web /app/web
ENV COMPILER_BIN=/app/compiler \
    RUNTIME_C=/app/runtime/runtime.c \
    PORT=8080 \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1
USER app
EXPOSE 8080
HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
  CMD ["python3", "-c", "import urllib.request,sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:8080/api/health', timeout=2).status == 200 else 1)"]
CMD ["python3", "-I", "/app/web/backend/app.py"]
