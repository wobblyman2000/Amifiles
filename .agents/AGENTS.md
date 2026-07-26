# Workspace Customization Rules

## Automatic Version Bumping
- **Rule**: Whenever you modify the codebase (add, edit, or delete any source files), you MUST automatically increment the patch version in `CMakeLists.txt`.
- **Method**: You can run `python3 scripts/bump_version.py` from the project root or edit `CMakeLists.txt` manually to increment the patch version.
- **Verification**: Ensure the version is bumped and mention the new version number in the final summary.
