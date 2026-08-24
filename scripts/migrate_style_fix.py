
def should_skip(path: Path) -> bool:
    parts = {p.lower() for p in path.parts}
    return bool(parts & SKIP_PATH_PARTS) or any(
        p.startswith("cmake-build") for p in path.parts
    )


TEXT_UPDATE_SUFFIXES = SOURCE_EXTS | {".cmake", ".md", ".mdc", ".txt", ".py"}

