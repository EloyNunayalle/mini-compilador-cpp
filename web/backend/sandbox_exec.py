"""Aplica limites de recursos y reemplaza el proceso por el programa de usuario.

Uso: sandbox_exec.py MEM_BYTES CPU_SECS FSIZE_BYTES NPROC NOFILE PROGRAMA
"""
import os
import sys

try:
    import resource
except ImportError:  # plataforma sin resource
    resource = None


def limit(kind, value):
    try:
        resource.setrlimit(kind, (value, value))
    except (ValueError, OSError):
        pass


def main():
    mem, cpu, fsize, nproc, nofile, prog = sys.argv[1:7]
    if resource is not None:
        limit(resource.RLIMIT_CPU, int(cpu))
        limit(resource.RLIMIT_FSIZE, int(fsize))
        limit(resource.RLIMIT_CORE, 0)
        limit(resource.RLIMIT_NOFILE, int(nofile))
        if hasattr(resource, "RLIMIT_AS"):
            limit(resource.RLIMIT_AS, int(mem))
        if hasattr(resource, "RLIMIT_NPROC"):
            limit(resource.RLIMIT_NPROC, int(nproc))
    os.execv(prog, [prog])


if __name__ == "__main__":
    main()
