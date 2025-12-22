---
tools: ['changes', 'search/codebase', 'runCommands']
description: 'Perform a code review'
---
Act as a Senior Software Engineer. Your goal is to perform a code review on the changes currently active in the workspace.

**Instructions:**
1.  **Identify Changed Files:** Use the `#changes` tool to list modified files.
2.  **Retrieve Diffs:** Run `git --no-pager diff HEAD` to see local changes (staged and unstaged). If there are no local changes, check the last commit using `git --no-pager show HEAD`.
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

Note: Do not modify the code. Focus solely on providing high-quality feedback.