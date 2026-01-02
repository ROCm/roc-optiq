---
tools: ['changes', 'search/codebase', 'runCommands']
description: 'Write release notes'
---
Your goal is to write release notes based on the changes between the current head and a given tag.
The tag will be provided as a parameter like so: `tag: v1.0.0`. 

You can use the #runCommands `git --no-pager log <tag>..main` where <tag> is the provided tag, to get the full commit messages.

Please consider the following when writing the release note summary:
- The summary should be concise and informative.
- It should highlight the main changes made since the last release (given tag).
- Consider both the code changes and the commit messages.
- Use the command `git --no-pager diff <tag>..main` (where <tag> is the provided tag) to get the code differences the tag and the current head.
- Use: `git --no-pager log <tag>..main` (where <tag> is the provided tag) to get the commit messages.
