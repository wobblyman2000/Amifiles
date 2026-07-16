# Amifiles Project Rules

## Automated Documentation Updates
Whenever you implement a new user-facing feature, bugfix, or change existing behavior in the file manager, you MUST automatically update the corresponding section(s) in the user manual inside [helpdialog.cpp](file:///home/dave/cpp_projects/Amifiles/src/helpdialog.cpp). Always maintain up-to-date documentation without needing the user to request it.

## AgeColorRule Reference
- The customizable file age color and emoji badge configuration structure is defined as `AgeColorRule` inside [filepanel.h](file:///home/dave/cpp_projects/Amifiles/src/filepanel.h#L43-L49).
- Dialog configuration and serialization are handled in [agestylingdialog.h](file:///home/dave/cpp_projects/Amifiles/src/agestylingdialog.h) and [agestylingdialog.cpp](file:///home/dave/cpp_projects/Amifiles/src/agestylingdialog.cpp).
