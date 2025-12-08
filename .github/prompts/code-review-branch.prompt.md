---
mode: 'agent'
tools: ['changes', 'codebase', 'runCommands']
description: 'Perform a code review'
---
Your goal is to performed a code review on the changed files in the project.  The changes refer to the differences between the current branch and its merge base with the `main` branch. 
The current branch is the one you are reviewing, and the merge base is the point where this branch diverged from `main`.

As part of the review perform the following tasks:  
- List the files that have been changed.
- Show the differences (changes) in the files.
- If you execute any `git` commands make sure they are done in non-interactive mode, use the `--no-pager` flag.
- Use the command `git --no-pager diff main...HEAD` to get the code differences between this branch and the branch point.
- Perform the code review, only comment on the changes.

For the code review focus on:  
- Potential bugs  
- Security vulnerabilities  
- Performance issues  
- Adherence to best practices  
- Readability and maintainability  
- Unnecessary complexity  
  
Provide concise, actionable feedback. If there are no significant issues state "No major issues found." 
Do not modify any code! 
Structure your feedback clearly.
If you need to refer to specific lines in the code, use the format `file:line_number` (e.g., `src/main.py:42`).