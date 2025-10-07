---
mode: 'agent'
tools: ['changes', 'codebase', 'runCommands']
description: 'Write a PR (pull request) summary'
---
Your goal is to write a summary for the pull request based on the changes made in the branch.
The changes refer to the differences between the current branch and its merge base with the `main` branch. 

You can use the #runCommands `git --no-pager log origin/main..HEAD` to get the full commit messages.

Please consider the following when writing the summary:
- The summary should be concise and informative.
- It should highlight the main changes made in the pull request.
- Consider both the code changes and the commit messages.
- Use the command `git --no-pager diff main...HEAD` to get the code differences between this branch and the branch point.
- If the pull request is small, you can summarize it in a single sentence.
- Use: `git --no-pager log origin/main..HEAD` to get the commit messages.
- Please do not try to run any tests.

