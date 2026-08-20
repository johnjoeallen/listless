## C++ Naming Convention

For all C++ code, use the following naming conventions consistently:

* **PascalCase** for classes, structs, enums, type aliases, and filenames that primarily represent a named type.

  * Examples: `FileViewer`, `DirectoryViewer`, `FileViewer.cpp`, `FileViewer.hpp`
* **camelCase** for functions, methods, variables, parameters, and data members.

  * Examples: `openFile()`, `currentFile`, `lineNumber`
* **UPPER_CASE_WITH_UNDERSCORES** for constants and macros.

  * Examples: `MAX_LINE_LENGTH`, `DEFAULT_TAB_WIDTH`
* **lowercase** for namespaces.
* Keep conventional infrastructure filenames lowercase where appropriate, especially `main.cpp`.
* Do **not** automatically convert C++ filenames to `snake_case`.
* When renaming a C++ type, keep its corresponding source/header filenames aligned with the type name.

## Issue-Driven Workflow

* Every code change must be tied to a GitHub issue. Before making changes, confirm there's an open issue that covers the work; if none exists, create one first (or ask the user to point at one) rather than making changes with no issue reference.
* If the user requests a change that doesn't appear to belong to the issue currently being worked, say so explicitly and check whether they want to: (a) file a new issue for it, (b) confirm it's actually in scope of the current issue, or (c) proceed anyway. Don't silently fold unrelated work into the current issue's commits/PR.
* Before pushing an update intended for an existing pull request, query that pull request and confirm that it is still open. If it is merged or closed, create a follow-up issue, branch, and pull request instead of pushing to its old branch.
* Keep the user-facing README and GitHub Pages home page synchronized. Changes to the project summary, current-platform status, or screenshot gallery must update both, using the same screenshot assets.
* When Codex makes code changes, credit Codex in the associated commit or pull request.
