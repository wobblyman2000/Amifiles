# Amifiles Project Rules

## Automated Documentation Updates
Whenever you implement a new user-facing feature, bugfix, or change existing behavior in the file manager, you MUST automatically update the corresponding section(s) in the user manual inside [helpdialog.cpp](file:///home/dave/cpp_projects/Amifiles/src/helpdialog.cpp). Always maintain up-to-date documentation without needing the user to request it.

## AgeColorRule Reference
- The customizable file age color and emoji badge configuration structure is defined as `AgeColorRule` inside [filepanel.h](file:///home/dave/cpp_projects/Amifiles/src/filepanel.h#L43-L49).
- Dialog configuration and serialization are handled in [agestylingdialog.h](file:///home/dave/cpp_projects/Amifiles/src/agestylingdialog.h) and [agestylingdialog.cpp](file:///home/dave/cpp_projects/Amifiles/src/agestylingdialog.cpp).

## Automated GitHub Commits & Pushes
- Whenever you finish implementing a feature, fix a bug, or make any code changes that compile successfully, you MUST automatically stage, commit, and push all modifications to the remote repository (`git add . && git commit -m "..." && git push origin main`). Do not wait for the user to request a git push.

## Automated Version Bumps
- Every time you implement any code changes, feature additions, or bug fixes, you MUST run the helper script `python3 scripts/bump_version.py` to automatically increment the patch version number in `CMakeLists.txt` prior to committing and pushing the changes. Incrementing the version number is the absolute first step of the commit/push workflow.
