# PrimePrecision ELS - AI Coding Assistant Rules (Embedded C++)

These rules are adapted from the "Full Process for Coding with AI Coding Assistants" guide, tailored for the PrimePrecision ELS embedded C++ project. They are intended to guide the AI coding assistant's behavior and ensure consistency.

### 1. Project Awareness & Context

- **Always read `PLANNING.MD`** at the start of a new conversation or significant task switch to understand the project's architecture, goals, style, and constraints.
- **Check `TASK.MD`** before starting a new task. If the task isn’t listed, discuss adding it with a brief description.
- **Use consistent naming conventions, file structure, and architectural patterns** as described in `PLANNING.MD` or observed in the existing codebase.

### 2. Code Structure & Modularity

- **Keep source files focused** on a single class, a coherent set of functions, or a specific module's implementation. Header files should primarily contain declarations and necessary inline definitions.
- **Organize code into clearly separated modules**, grouped by feature or responsibility (e.g., Hardware Abstraction, Motion Control, UI). Follow the existing project structure (`src/`, `include/`).
- **Use clear, consistent imports/includes.** Prefer relative paths for project-internal includes where appropriate (e.g., `#include "Motion/MotionControl.h"`).
- **Employ header guards** (e.g., `#ifndef MY_HEADER_H_ ... #define MY_HEADER_H_ ... #endif`) in all header files to prevent multiple inclusion errors.
- **Use forward declarations** where possible to reduce compilation dependencies between headers.

### 3. Task Completion

- **Mark completed tasks in `TASK.MD`** immediately after finishing and verifying them.
- Add new sub-tasks or TODOs discovered during development to `TASK.MD` under a “Discovered During Work” or similar section, or discuss their addition.

### 4. Style & Conventions (C++)

- **Use C++** as the primary language, adhering to the version and standards compatible with the STM32 toolchain (as noted in `PLANNING.MD`).
- **Follow established C++ coding standards.** This includes:
  - Consistent naming conventions for classes (e.g., `PascalCase`), functions (e.g., `camelCase` or `snake_case` as per project style), variables (e.g., `camelCase` or `snake_case`).
  - Consistent brace style (e.g., Allman or K&R, as per project style).
  - Use `nullptr` instead of `NULL` or `0` for pointers.
  - Prefer `enum class` over plain `enum` for type safety and scope.
  - Initialize variables upon declaration.
- **Write Doxygen-style docstrings for public functions, classes, and significant members** in header files.
  ```cpp
  /**
   * @brief Brief summary of the function/class/method.
   *
   * More detailed description if needed, explaining its purpose,
   * any non-obvious behavior, or important considerations.
   *
   * @param paramName Description of the parameter.
   * @return return_type Description of the return value (if any).
   * @note Any special notes or warnings.
   * @see RelatedFunctionOrClass
   */
  // Example:
  // void processData(const DataType& data, bool enableFeature);
  ```

### 5. Documentation & Explainability

- **Update `PLANNING.MD`** if architectural decisions change or significant new principles are adopted.
- **Update `TASK.MD`** regularly to reflect progress and new findings.
- **Update the main project `README.MD`** (if one exists or is created) when new high-level features are added, dependencies change, or setup/build steps are modified.
- **Comment non-obvious code sections** and ensure logic is understandable to a developer familiar with embedded C++.
- When writing complex logic, **add an inline `// Reason:` comment** explaining the 'why' behind a particular implementation choice, not just the 'what'.

### 6. AI Behavior Rules

- **Never assume missing context. Ask clarifying questions if uncertain** about requirements, existing code behavior, or implementation details.
- **Never hallucinate libraries, functions, or hardware registers** – only use known, verified STM32 HAL/LL functions, standard C++ features, or libraries already part of the project (e.g., STM32Step, Lumen Protocol).
- **Always confirm file paths and module names** exist or are planned before referencing them in code or include statements.
- **Never delete or overwrite existing code** unless explicitly instructed to as part of a task from `TASK.MD`, or after confirming it's safe and necessary.
- **Propose changes incrementally.** For significant refactoring or new features, outline the approach first if the path isn't straightforward.
- **When providing code, ensure it is complete and syntactically correct C++** for the relevant context.
