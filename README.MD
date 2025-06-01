# PrimePresicionEls

## Overview

The PrimePrecision Electronic Leadscrew (ELS) project aims to develop a robust, precise, and user-friendly control system for a lathe, enhancing its capabilities for threading, turning, and other machining operations.

**Overall Goals:**

- Replace manual gear changes with an electronic system for higher precision and flexibility.
- Provide intuitive user interface for selecting modes, feed rates, and thread pitches.
- Ensure reliable and safe operation.
- Support a range of common metric and imperial threads and feeds.
- Allow for future expansion with advanced features.

## Setup

_(To be filled in with setup instructions as the project develops)_

## Usage

_(To be filled in with usage instructions as the project develops)_

## ⚡️ Dynamic Command System & Workflow Modifiers

This project workflow supports dynamic commands to adjust behavior and access information quickly. Commands start with `/`.

**Core Commands:**

- `/help`
  - **Action:** Displays this list of available commands, their descriptions, the current operational mode, and the AI's current persona.
- `/mode [step-by-step | task-confirmation | full-autopilot]`
  - **Action:** Switches the AI's operational mode.
  - **Example:** `/mode step-by-step`
  - **Effect:** The AI will confirm the change and adjust its interaction frequency and autonomy. `.clinerules/els_rules.md` will be internally updated by the AI.
- `/persona [new persona description or reference to a predefined persona]`
  - **Action:** Updates the AI's active persona. This influences its tone, suggestions, and rule interpretation.
  - **Example:** `/persona Senior Python Developer specializing in APIs`
  - **Effect:** The AI will confirm. The `AI Persona Directive` in `.clinerules/els_rules.md` will be updated by the AI.
- `/tone [e.g., formal, informal, concise, detailed, teaching, expert]`
  - **Action:** Adjusts the AI's communication tone (a sub-aspect of persona).
  - **Example:** `/tone concise`
- `/setvar project.[variable_path] = [value]` (Consult `ai_docs/PLANNING.MD` for defined variables)
  - **Action:** Signals a change to a project-specific variable.
  - **Effect:** The AI will acknowledge and suggest an update to `ai_docs/PLANNING.MD`.
- `/getvar project.[variable_path]` (Consult `ai_docs/PLANNING.MD` for defined variables)
  - **Action:** Retrieves a value from `ai_docs/PLANNING.MD`.
- `/summarize`
  - **Action:** Provides a brief summary of current task status, recent actions, and next planned steps from `ai_docs/TASK.MD`.
- `/reorient`
  - **Action:** Prompts the AI to re-read key documents (`.clinerules/els_rules.md`, `ai_docs/PLANNING.MD`, current `ai_docs/TASK.MD`) to refresh its context.
  - **Effect:** Useful if the AI seems to be losing track or if major changes have occurred.
