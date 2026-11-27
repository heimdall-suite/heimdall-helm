# PlatformIO pre-script: bakes the firmware revision into the build as
# HELM_FW_REV (a string literal), so `cfg info` (lib/cli, issue #69) can
# tell a client exactly which build is running.
#
# `git describe --always --dirty` -> e.g. "3a61cd7" or "3a61cd7-dirty".
# Falls back to "unknown" outside a git checkout (source tarball, CI
# without history) instead of failing the build.
import subprocess

Import("env")  # noqa: F821 -- injected by PlatformIO/SCons


def _git_rev(project_dir):
    try:
        out = subprocess.check_output(
            ["git", "describe", "--always", "--dirty", "--abbrev=7"],
            cwd=project_dir,
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip() or "unknown"
    except Exception:
        return "unknown"


rev = _git_rev(env.subst("$PROJECT_DIR"))  # noqa: F821
# Global CPPDEFINES so lib/ sources (lib/cli) see it too, not just src/.
env.Append(CPPDEFINES=[("HELM_FW_REV", '\\"%s\\"' % rev)])  # noqa: F821
