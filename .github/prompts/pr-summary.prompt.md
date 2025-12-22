---
tools: ['changes', 'search/codebase', 'runCommands']
description: 'Write a PR (pull request) summary'
argument-hint: 'base-branch'
---
Your goal is to write a summary for the pull request based on the changes made in the branch.

The changes refer to the differences between the current branch and its merge base with the base branch provided by the user.

If the user provides a branch name argument, use it as the base branch.
If no branch name is provided, use `main` as the default base branch.

You can use the #runCommands `git --no-pager log origin/<BASE_BRANCH>..HEAD` to get the full commit messages.

Please consider the following when writing the summary:
- The summary should be concise and informative.
- It should highlight the main changes made in the pull request.
- Consider both the code changes and the commit messages.
- Use the command `git --no-pager diff <BASE_BRANCH>...HEAD` (replacing `<BASE_BRANCH>` with the determined base branch) to get the code differences between this branch and the branch point.
- If the pull request is small, you can summarize it in a single sentence.
- Use: `git --no-pager log origin/<BASE_BRANCH>..HEAD` to get the commit messages.
- Please do not try to run any tests.

