---
mode: 'agent'
tools: ['changes', 'codebase', 'runCommands']
description: 'Perform a code review'
---
Your goal is to performed a code review oon the changes made in the branch.

As part of the review perform the following tasks:  
- List the files that have been changed, use the #changes tool to get the changed files.
- Show the differences (changes) in the files.
- If you execute "git diff" make sure it done in non-interactive mode, use the "--no-pager" flag.
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

