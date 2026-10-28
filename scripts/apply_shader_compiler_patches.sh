#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
shader_compiler="${repo_root}/app/libraries/shader-compiler"
patch_file="${repo_root}/patches/shader-compiler/0001-adreno-gather-rounding.patch"
expected_revision="8717c8f6d7deab58032e042b027ed84838833824"

if [[ ! -d "${shader_compiler}/.git" && ! -f "${shader_compiler}/.git" ]]; then
    echo "shader-compiler submodule is missing; initialize recursive submodules first" >&2
    exit 1
fi

actual_revision="$(git -C "${shader_compiler}" rev-parse HEAD)"
if [[ "${actual_revision}" != "${expected_revision}" ]]; then
    echo "unsupported shader-compiler pin: expected ${expected_revision}, got ${actual_revision}" >&2
    exit 1
fi

# The patch uses zero-context hunks to stay whitespace-clean; the exact pin above protects offsets.
if git -C "${shader_compiler}" apply --unidiff-zero --reverse --check "${patch_file}" >/dev/null 2>&1; then
    echo "shader-compiler Adreno gather patch is already applied"
    exit 0
fi

git -C "${shader_compiler}" apply --unidiff-zero --check "${patch_file}"
git -C "${shader_compiler}" apply --unidiff-zero "${patch_file}"
echo "applied shader-compiler Adreno gather patch to ${actual_revision}"
