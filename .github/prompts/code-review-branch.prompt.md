---
tools: ['changes', 'search/codebase', 'runCommands']
description: 'Perform a code review'
argument-hint: 'base-branch'
---

Act as a Senior Software Engineer. Your goal is to perform a code review on the changes between the current branch and a base branch.

If the user provides a branch name argument, use it as the base branch. If no branch name is provided, use `main` as the default base branch.

**Instructions:**
1.  **Determine Base Branch:** Use the user-provided argument as the base branch. If none is provided, default to `main`.
2.  **Identify Changes:** Run `git --no-pager diff <BASE_BRANCH>...HEAD` (using the 3-dot syntax) to get the differences from the merge base.
3.  **Analyze Code:** Review the diffs based on the criteria below.

**Review Criteria:**
-   **Functionality:** Potential bugs, logic errors, and edge cases.
-   **Security:** Vulnerabilities, input validation issues, and secrets handling.
-   **Performance:** Inefficient loops, memory leaks, or expensive operations.
-   **Quality:** Readability, maintainability, naming conventions, and best practices.
-   **Cleanliness:** Unnecessary complexity, leftover debug code, or commented-out blocks.

**Output Guidelines:**
-   **Summary:** Briefly describe the scope of the changes.
-   **Findings:** Group comments by file. Use the format `File: <filename> (Line <number>)`.
-   **Severity:** Tag issues as [Critical], [Major], or [Minor].
-   **Actionable Feedback:** Provide clear suggestions on how to fix identified issues. If the code looks good, confirm with "No major issues found."

**Note:** Do not modify the code. Focus solely on providing high-quality feedback.
