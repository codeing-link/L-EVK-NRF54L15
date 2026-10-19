#!/usr/bin/env python3
"""Update Lierda modules from the upstream application's west manifest."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import yaml


MAIN_NAME = "l-evk-t2-gh3220"
MAIN_URL = "https://gitee.com/XiaoPb/l-evk-t2-gh3220.git"
SKIPPED_PROJECTS = {"l-evk-t2-nrf54l15"}


def run(*args: str, cwd: Path | None = None, capture: bool = False) -> str:
    command = [str(arg) for arg in args]
    print("+", " ".join(command))
    result = subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    return result.stdout.strip() if capture else ""


def ensure_clean(repo: Path) -> None:
    dirty = run(
        "git", "status", "--porcelain", "--untracked-files=normal",
        cwd=repo,
        capture=True,
    )
    if dirty:
        raise RuntimeError(
            f"Refusing to update dirty repository: {repo}\n{dirty}"
        )


def ensure_repository(repo: Path, url: str) -> None:
    if (repo / ".git").is_dir():
        ensure_clean(repo)
        current_url = run(
            "git", "remote", "get-url", "origin", cwd=repo, capture=True
        )
        if current_url != url:
            print(f"Updating origin URL: {current_url} -> {url}")
            run("git", "remote", "set-url", "origin", url, cwd=repo)
        return

    if repo.exists() and any(repo.iterdir()):
        raise RuntimeError(f"Target exists and is not a Git repository: {repo}")
    repo.parent.mkdir(parents=True, exist_ok=True)
    run("git", "clone", url, str(repo))


def remote_default_branch(url: str) -> str:
    output = run("git", "ls-remote", "--symref", url, "HEAD", capture=True)
    for line in output.splitlines():
        if line.startswith("ref: refs/heads/") and line.endswith("\tHEAD"):
            return line.removeprefix("ref: refs/heads/").removesuffix("\tHEAD")
    raise RuntimeError(f"Cannot determine the default branch of {url}")


def update_revision(repo: Path, revision: str) -> str:
    run("git", "fetch", "--prune", "--tags", "origin", cwd=repo)
    branch_ref = f"refs/heads/{revision}"
    branch_exists = bool(
        run("git", "ls-remote", "--heads", "origin", branch_ref,
            cwd=repo, capture=True)
    )

    if branch_exists:
        local_exists = subprocess.run(
            ["git", "show-ref", "--verify", "--quiet", branch_ref], cwd=repo
        ).returncode == 0
        if local_exists:
            run("git", "switch", revision, cwd=repo)
        else:
            run("git", "switch", "--track", "-c", revision,
                f"origin/{revision}", cwd=repo)
        run("git", "merge", "--ff-only", f"origin/{revision}", cwd=repo)
    else:
        run("git", "checkout", "--detach", revision, cwd=repo)

    return run("git", "rev-parse", "HEAD", cwd=repo, capture=True)


def resolve_project_url(
    project: dict, remotes: dict[str, str], default_remote: str
) -> str:
    if "url" in project:
        return str(project["url"])
    remote = str(project.get("remote", default_remote))
    if remote not in remotes:
        raise RuntimeError(f"Unknown west remote '{remote}' for {project['name']}")
    repo_path = str(project.get("repo-path", project["name"]))
    return f"{remotes[remote].rstrip('/')}/{repo_path}.git"


def load_projects(manifest_path: Path, modules_root: Path) -> list[dict[str, str]]:
    data = yaml.safe_load(manifest_path.read_text(encoding="utf-8"))
    manifest = data.get("manifest", {})
    remotes = {
        str(item["name"]): str(item["url-base"])
        for item in manifest.get("remotes", [])
    }
    default_remote = str(manifest.get("defaults", {}).get("remote", ""))
    selected: list[dict[str, str]] = []

    for project in manifest.get("projects", []):
        name = str(project["name"])
        path = Path(str(project.get("path", name)))
        if name in SKIPPED_PROJECTS or path.name in SKIPPED_PROJECTS:
            print(f"Skipping board module managed by this application: {name}")
            continue
        if len(path.parts) < 3 or path.parts[:2] != ("modules", "lierda"):
            continue
        destination = (modules_root / path.name).resolve()
        if destination.parent != modules_root.resolve():
            raise RuntimeError(f"Unsafe module destination from west.yml: {path}")
        selected.append(
            {
                "name": name,
                "path": str(destination),
                "url": resolve_project_url(project, remotes, default_remote),
                "revision": str(project.get("revision", "master")),
            }
        )
    return selected


def main() -> int:
    app_root = Path(__file__).resolve().parents[1]
    ncs_root = app_root.parents[1]
    modules_root = ncs_root / "modules" / "lierda"
    main_repo = modules_root / MAIN_NAME
    modules_root.mkdir(parents=True, exist_ok=True)

    ensure_repository(main_repo, MAIN_URL)
    main_branch = remote_default_branch(MAIN_URL)
    main_commit = update_revision(main_repo, main_branch)

    lock = {
        "source_manifest": "modules/lierda/l-evk-t2-gh3220/west.yml",
        "projects": [
            {
                "name": MAIN_NAME,
                "url": MAIN_URL,
                "revision": main_branch,
                "commit": main_commit,
            }
        ],
    }

    for project in load_projects(main_repo / "west.yml", modules_root):
        destination = Path(project["path"])
        ensure_repository(destination, project["url"])
        commit = update_revision(destination, project["revision"])
        lock["projects"].append(
            {
                "name": project["name"],
                "url": project["url"],
                "revision": project["revision"],
                "commit": commit,
            }
        )

    lock_path = app_root / "modules.lock.yml"
    lock_path.write_text(
        "# Generated by scripts/update_lierda_modules.py; commit this file.\n"
        + json.dumps(lock, indent=2, ensure_ascii=False)
        + "\n",
        encoding="utf-8",
    )
    print(f"Updated module lock: {lock_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
