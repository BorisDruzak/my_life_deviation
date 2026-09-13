#!/usr/bin/env python3
"""Build and verify a traceable NORM-MEMORY source delivery."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


PRIVATE_MARKERS = (
    b"-----BEGIN " b"PRIVATE KEY-----",
    b"-----BEGIN RSA " b"PRIVATE KEY-----",
    b"-----BEGIN OPENSSH " b"PRIVATE KEY-----",
    b"-----BEGIN EC " b"PRIVATE KEY-----",
)
SECRET_NAMES = {".env", "credentials", "credentials.json", "secrets", "secrets.json", "token", "tokens"}
SECRET_SUFFIXES = {".key", ".pem", ".p12", ".pfx"}
CANONICAL_GIT_CONFIG = ["-c", "core.autocrlf=false", "-c", "core.eol=lf", "-c", "core.longpaths=true"]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git(repo: Path, args: list[str], *, env: dict[str, str] | None = None,
        binary: bool = False) -> str | bytes:
    completed = subprocess.run(
        ["git", *args], cwd=repo, env=env, capture_output=True,
        text=not binary, encoding=None if binary else "utf-8", check=False,
    )
    if completed.returncode:
        stderr = completed.stderr.decode("utf-8", "replace") if binary else completed.stderr
        raise RuntimeError(f"git {' '.join(args[:3])} failed ({completed.returncode}): {stderr.strip()}")
    return completed.stdout


def safe_parts(name: str) -> tuple[str, ...]:
    normalized = name.replace("\\", "/")
    path = PurePosixPath(normalized)
    if not normalized or path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise ValueError(f"unsafe archive path: {name!r}")
    if any(":" in part or not part for part in path.parts):
        raise ValueError(f"unsafe archive path: {name!r}")
    return path.parts


def sensitive_path(name: str) -> bool:
    parts = safe_parts(name)
    lowered = [part.lower() for part in parts]
    first = lowered[0]
    if first in {"out", ".git", ".worktrees", "temp", "tmp"}:
        return True
    if len(parts) > 1 and first.startswith("build"):
        return True
    if any(part in {".git", "__pycache__", "temp", "tmp"} for part in lowered[:-1]):
        return True
    filename = lowered[-1]
    stem = Path(filename).stem
    return filename in SECRET_NAMES or stem in SECRET_NAMES or Path(filename).suffix in SECRET_SUFFIXES or filename.endswith(".tmp")


def validate_content(name: str, data: bytes) -> None:
    if sensitive_path(name):
        raise ValueError(f"forbidden delivery path: {name}")
    if any(marker in data for marker in PRIVATE_MARKERS):
        raise ValueError(f"private-key material found in: {name}")


def primary_index(repo: Path) -> Path:
    value = str(git(repo, ["rev-parse", "--git-path", "index"])).strip()
    path = Path(value)
    return path if path.is_absolute() else (repo / path).resolve()


def optional_hash(path: Path) -> str | None:
    return sha256_file(path) if path.is_file() else None


def isolated_environment(index: Path) -> dict[str, str]:
    result = os.environ.copy()
    result["GIT_INDEX_FILE"] = str(index)
    return result


def index_entries(repo: Path, env: dict[str, str]) -> list[tuple[str, str, str]]:
    raw = bytes(git(repo, ["ls-files", "-s", "-z"], env=env, binary=True))
    result: list[tuple[str, str, str]] = []
    for row in raw.split(b"\0"):
        if not row:
            continue
        metadata, encoded = row.split(b"\t", 1)
        mode, object_id, stage = metadata.decode("ascii").split()
        if stage != "0" or mode == "160000":
            raise ValueError("delivery does not support unresolved entries or submodules")
        name = encoded.decode("utf-8")
        validate_content(name, b"")
        result.append((name, mode, object_id))
    return result


def read_exact(stream: Any, size: int) -> bytes:
    chunks: list[bytes] = []
    remaining = size
    while remaining:
        chunk = stream.read(min(1024 * 1024, remaining))
        if not chunk:
            raise EOFError("truncated git object stream")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def materialize_index(repo: Path, env: dict[str, str], entries: list[tuple[str, str, str]],
                      destination: Path) -> dict[str, dict[str, Any]]:
    destination.mkdir(parents=True, exist_ok=False)
    process = subprocess.Popen(
        ["git", "cat-file", "--batch"], cwd=repo, env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    assert process.stdin is not None and process.stdout is not None
    manifest: dict[str, dict[str, Any]] = {}
    try:
        for name, mode, object_id in entries:
            process.stdin.write(object_id.encode("ascii") + b"\n")
            process.stdin.flush()
            header = process.stdout.readline().decode("ascii").strip().split()
            if len(header) != 3 or header[1] != "blob":
                raise RuntimeError(f"cannot read staged blob for {name}")
            size = int(header[2])
            data = read_exact(process.stdout, size)
            if process.stdout.read(1) != b"\n":
                raise RuntimeError("invalid git batch delimiter")
            validate_content(name, data)
            target = destination.joinpath(*safe_parts(name))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            manifest[name] = {"sha256": sha256_bytes(data), "bytes": size, "git_mode": mode}
    finally:
        if process.stdin:
            process.stdin.close()
        stderr = process.stderr.read().decode("utf-8", "replace") if process.stderr else ""
        code = process.wait()
        if code:
            raise RuntimeError(f"git cat-file failed ({code}): {stderr.strip()}")
    return manifest


def safe_extract_archive(repo: Path, base: str, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=False)
    process = subprocess.Popen(
        ["git", *CANONICAL_GIT_CONFIG, "archive", "--format=tar", base], cwd=repo,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    assert process.stdout is not None
    try:
        with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
            for member in archive:
                parts = safe_parts(member.name.rstrip("/"))
                target = destination.joinpath(*parts)
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    source = archive.extractfile(member)
                    if source is None:
                        raise RuntimeError(f"cannot extract {member.name}")
                    with target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                else:
                    raise ValueError(f"links and special archive entries are forbidden: {member.name}")
    finally:
        stderr = process.stderr.read().decode("utf-8", "replace") if process.stderr else ""
        code = process.wait()
        if code:
            raise RuntimeError(f"git archive failed ({code}): {stderr.strip()}")


def file_manifest(root: Path, *, exclude_git: bool = False) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file()):
        relative = path.relative_to(root).as_posix()
        if exclude_git and relative.startswith(".git/"):
            continue
        result[relative] = {"sha256": sha256_file(path), "bytes": path.stat().st_size}
    return result


def compare_source(expected: dict[str, dict[str, Any]], actual_root: Path) -> None:
    actual = file_manifest(actual_root, exclude_git=True)
    expected_simple = {name: {"sha256": row["sha256"], "bytes": row["bytes"]} for name, row in expected.items()}
    if expected_simple != actual:
        missing = sorted(set(expected_simple) - set(actual))[:10]
        extra = sorted(set(actual) - set(expected_simple))[:10]
        changed = sorted(name for name in set(expected_simple) & set(actual) if expected_simple[name] != actual[name])[:10]
        raise RuntimeError(f"applied tree differs from staged source; missing={missing}, extra={extra}, changed={changed}")


def evidence_files(repo: Path, requested: Iterable[Path], output_root: Path) -> list[tuple[Path, str, str]]:
    result: list[tuple[Path, str, str]] = []
    used: set[str] = set()
    for number, supplied in enumerate(requested, 1):
        source = (repo / supplied).resolve() if not supplied.is_absolute() else supplied.resolve()
        if not source.exists():
            raise FileNotFoundError(f"evidence does not exist: {supplied}")
        if source == output_root or source in output_root.parents or output_root in source.parents:
            raise ValueError("evidence and delivery output must not contain one another")
        label = re.sub(r"[^A-Za-z0-9._-]+", "-", source.name).strip("-.") or "evidence"
        prefix = f"evidence/{number:02d}-{label}"
        candidates = [source] if source.is_file() else sorted(path for path in source.rglob("*") if path.is_file())
        for path in candidates:
            if path.is_symlink():
                raise ValueError(f"evidence symlink is forbidden: {path.name}")
            relative = path.name if source.is_file() else path.relative_to(source).as_posix()
            archive_name = f"{prefix}/{relative}"
            validate_content(archive_name, b"")
            if archive_name in used:
                raise ValueError(f"duplicate evidence archive path: {archive_name}")
            used.add(archive_name)
            try:
                display = source.relative_to(repo).as_posix()
            except ValueError:
                display = source.name
            result.append((path, archive_name, display))
    return result


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")


def zip_info(name: str, executable: bool = False) -> zipfile.ZipInfo:
    safe_parts(name)
    info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = ((0o100755 if executable else 0o100644) << 16)
    return info


def write_verified_zip(path: Path, staged_tree: Path, source_manifest: dict[str, dict[str, Any]],
                       evidence: list[tuple[Path, str, str]], metadata: dict[str, bytes],
                       verify_root: Path) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    expected: dict[str, dict[str, Any]] = {}
    evidence_rows: list[dict[str, Any]] = []
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9, allowZip64=True) as archive:
        for name, row in sorted(source_manifest.items()):
            data = staged_tree.joinpath(*safe_parts(name)).read_bytes()
            archive_name = f"source-tree/{name}"
            archive.writestr(zip_info(archive_name, row["git_mode"] == "100755"), data)
            expected[archive_name] = {"sha256": sha256_bytes(data), "bytes": len(data)}
        for source, archive_name, display in evidence:
            data = source.read_bytes()
            validate_content(archive_name, data)
            digest = sha256_bytes(data)
            archive.writestr(zip_info(archive_name), data)
            expected[archive_name] = {"sha256": digest, "bytes": len(data)}
            evidence_rows.append({"source": display, "archive_path": archive_name, "sha256": digest, "bytes": len(data)})
        evidence_manifest = json_bytes({"schema": "norm-evidence-manifest-0.1", "files": evidence_rows})
        all_metadata = {**metadata, "delivery/EVIDENCE_MANIFEST.json": evidence_manifest}
        for archive_name, data in sorted(all_metadata.items()):
            archive.writestr(zip_info(archive_name), data)
            expected[archive_name] = {"sha256": sha256_bytes(data), "bytes": len(data)}

    with zipfile.ZipFile(path, "r") as archive:
        if archive.testzip() is not None:
            raise RuntimeError("ZIP CRC verification failed")
        names = archive.namelist()
        if len(names) != len(set(names)) or set(names) != set(expected):
            raise RuntimeError("ZIP inventory differs from expected inventory")
        verify_root.mkdir(parents=True, exist_ok=False)
        for member in archive.infolist():
            target = verify_root.joinpath(*safe_parts(member.filename))
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(member, "r") as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
    extracted = file_manifest(verify_root)
    if extracted != expected:
        raise RuntimeError("extracted ZIP hashes differ from packaged hashes")
    for name, row in source_manifest.items():
        if extracted[f"source-tree/{name}"]["sha256"] != row["sha256"]:
            raise RuntimeError(f"source hash mismatch after ZIP extraction: {name}")
    return expected, {"files": evidence_rows}


def fresh_output(root: Path, base: str) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    stem = f"norm-memory-0.1-{base[:8]}-{stamp}"
    for number in range(1000):
        candidate = root / (stem if number == 0 else f"{stem}-{number:02d}")
        try:
            candidate.mkdir()
            return candidate
        except FileExistsError:
            continue
    raise RuntimeError("cannot allocate fresh delivery directory")


def package(repo: Path, base_arg: str, output_root: Path, requested_evidence: list[Path]) -> Path:
    repo = Path(str(git(repo, ["rev-parse", "--show-toplevel"])).strip()).resolve()
    base = str(git(repo, ["rev-parse", f"{base_arg}^{{commit}}"])).strip()
    head = str(git(repo, ["rev-parse", "HEAD"])).strip()
    git(repo, ["merge-base", "--is-ancestor", base, head])
    index_path = primary_index(repo)
    index_before = optional_hash(index_path)
    output_root = (repo / output_root).resolve() if not output_root.is_absolute() else output_root.resolve()

    try:
        with tempfile.TemporaryDirectory(prefix="norm-release-") as temporary:
            work = Path(temporary)
            isolated_index = work / "release.index"
            env = isolated_environment(isolated_index)
            git(repo, ["read-tree", base], env=env)
            git(repo, ["add", "-A", "--", "."], env=env)
            entries = index_entries(repo, env)
            staged_tree = work / "staged-tree"
            source_manifest = materialize_index(repo, env, entries, staged_tree)
            patch_data = bytes(git(repo, ["diff", "--binary", "--full-index", "--cached", base, "--"], env=env, binary=True))
            if not patch_data:
                raise RuntimeError("release patch is empty")
            changes = str(git(repo, ["diff", "--name-status", "--cached", base, "--"], env=env)).splitlines()
            patch_path = work / "NORM_MEMORY_0.1.patch"
            patch_path.write_bytes(patch_data)

            applied = work / "applied-base"
            safe_extract_archive(repo, base, applied)
            git(applied, ["init", "-q"])
            check_output = str(git(applied, [*CANONICAL_GIT_CONFIG, "apply", "--check", "--binary", str(patch_path)])).strip()
            apply_output = str(git(applied, [*CANONICAL_GIT_CONFIG, "apply", "--binary", str(patch_path)])).strip()
            compare_source(source_manifest, applied)

            evidence = evidence_files(repo, requested_evidence, output_root)
            source_document = {
                "schema": "norm-source-manifest-0.1",
                "base": base,
                "head_at_packaging": head,
                "files": source_manifest,
            }
            source_bytes = json_bytes(source_document)
            metadata = {
                "delivery/SOURCE_MANIFEST.json": source_bytes,
                "delivery/PATCH_SHA256.txt": (sha256_bytes(patch_data) + "  NORM_MEMORY_0.1.patch\n").encode("ascii"),
            }
            archive_path = work / "NORM_MEMORY_0.1_SOURCE_AND_EVIDENCE.zip"
            archive_inventory, evidence_document = write_verified_zip(
                archive_path, staged_tree, source_manifest, evidence, metadata, work / "zip-verify"
            )

            output = fresh_output(output_root, base)
            shutil.copy2(patch_path, output / patch_path.name)
            shutil.copy2(archive_path, output / archive_path.name)
            (output / "SOURCE_MANIFEST.json").write_bytes(source_bytes)
            (output / "EVIDENCE_MANIFEST.json").write_bytes(json_bytes({"schema": "norm-evidence-manifest-0.1", **evidence_document}))
            verification = {
                "git_apply_check": {"returncode": 0, "stdout": check_output},
                "git_apply": {"returncode": 0, "stdout": apply_output},
                "applied_source_hashes_match": True,
                "zip_crc_ok": True,
                "zip_safe_paths": True,
                "zip_extracted_hashes_match": True,
                "source_hashes_after_extract_match": True,
            }
            (output / "VERIFICATION.json").write_bytes(json_bytes(verification))
            artifact_names = [patch_path.name, archive_path.name, "SOURCE_MANIFEST.json", "EVIDENCE_MANIFEST.json", "VERIFICATION.json"]
            artifacts = {name: {"sha256": sha256_file(output / name), "bytes": (output / name).stat().st_size} for name in artifact_names}
            sha_lines = "".join(f"{row['sha256']}  {name}\n" for name, row in sorted(artifacts.items()))
            (output / "SHA256SUMS").write_text(sha_lines, encoding="ascii", newline="\n")
            artifacts["SHA256SUMS"] = {"sha256": sha256_file(output / "SHA256SUMS"), "bytes": (output / "SHA256SUMS").stat().st_size}
            manifest = {
                "schema": "norm-delivery-0.1",
                "created_utc": datetime.now(timezone.utc).isoformat(),
                "base": base,
                "head_at_packaging": head,
                "primary_git_index_unchanged": optional_hash(index_path) == index_before,
                "patch": {"binary": True, "changes": changes},
                "source_file_count": len(source_manifest),
                "evidence_file_count": len(evidence),
                "zip_file_count": len(archive_inventory),
                "verification": verification,
                "artifacts": artifacts,
            }
            (output / "DELIVERY.json").write_bytes(json_bytes(manifest))
    finally:
        if optional_hash(index_path) != index_before:
            raise RuntimeError("primary Git index changed during isolated packaging")
    return output


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="approved baseline commit")
    parser.add_argument("--out", type=Path, required=True, help="parent directory for a fresh versioned delivery")
    parser.add_argument("--evidence", type=Path, action="append", default=[], help="evidence file or directory; repeatable")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        output = package(Path.cwd(), args.base, args.out, args.evidence)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}", file=os.sys.stderr)
        return 1
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
