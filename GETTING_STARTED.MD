# Getting Started with OVMS Agent

This guide assumes that OVMS Agent has already been successfully installed and built on your OpenVMS system.

The goal is to take you from a working OVMS Agent installation to using the agent on a real software project.

You will learn how to:

- use Git locally or connect it to GitHub;
- configure an LLM provider;
- start OVMS Agent;
- begin working with a new or existing project;
- download a project from GitHub for analysis;
- ask the agent to inspect, build, diagnose, and modify a project safely;
- review the agent's work with Git.

You do **not** need to use GitHub to use OVMS Agent. A local Git repository is sufficient.

---

# 1. Before You Begin

This guide assumes:

- OVMS Agent has been installed successfully.
- The OVMS Agent build and regression tests pass.
- Git is installed on your OpenVMS system.
- At least one supported LLM provider is available to you.
- You have a directory where you can create or download projects.

In the examples below:

- `SYS$LOGIN` represents your OpenVMS login directory.
- `PROJECTS` is an example directory used to hold source repositories.
- `MYPROJECT` is an example project name.
- `YOURNAME` represents your GitHub username.
- `OWNER` represents the owner of another GitHub repository.

It is useful to understand the roles of the three major components.

**OVMS Agent** works with and reasons about your project.

**Git** provides local source-control history.

**GitHub** is an optional remote service for storing and sharing Git repositories.

The relationship is approximately:

```text
                    optional
Local Project + Git ---------> GitHub
        |
        |
        v
    OVMS Agent
        |
        v
   LLM Provider
```

OVMS Agent does not require GitHub.

---

# 2. Git or GitHub?

Before starting a project, decide whether you want to use Git only on your OpenVMS system or also connect the repository to GitHub.

## Local Git

A local Git repository provides:

- change tracking;
- branches;
- commits;
- diffs;
- the ability to undo changes;
- a history of work performed on the project.

No GitHub account is required.

For a new project, create a directory and initialize Git:

```text
$ CREATE/DIRECTORY SYS$LOGIN:[PROJECTS.MYPROJECT]
$ SET DEFAULT SYS$LOGIN:[PROJECTS.MYPROJECT]

$ git init
```

Verify the repository:

```text
$ git status
```

You now have a local Git repository that OVMS Agent can work with.

## GitHub

GitHub adds remote storage and collaboration features such as:

- remote repositories;
- backups;
- sharing;
- issues;
- pull requests;
- collaboration with other developers.

A common arrangement is:

```text
OpenVMS working directory
        |
        v
Local Git repository
        |
        | fetch / push
        v
GitHub repository
```

GitHub authentication must be configured before Git can push to a private repository or to a repository you own.

The exact authentication method depends on your Git/OpenVMS environment. Do not place passwords, personal access tokens, or other credentials in files that will be committed to Git.

Once a remote repository has been configured, inspect it with:

```text
$ git remote -v
```

Before pushing changes, always verify what repository and branch you are using.

---

# 3. Configure an LLM Provider

OVMS Agent is designed to be LLM-provider-neutral.

Depending on your configuration, the provider may be:

- a directly supported LLM service;
- an OpenAI-compatible API;
- Requesty or another compatible intermediary;
- another provider supported by OVMS Agent.

The important pieces of configuration are generally:

```text
Provider
Endpoint
Model
Credentials
```

Additional options may control such things as timeouts, output limits, and streaming.

## Protect Your API Credentials

Treat an LLM API key like a password.

Do not:

- commit it to Git;
- put it in a source file;
- include it in documentation;
- paste it into a prompt sent to an LLM;
- store it in a public repository.

Use the credential mechanism supported by your OVMS Agent installation.

## Test the LLM

Before giving the agent a software project, verify basic LLM communication with a simple request.

For example:

```text
ASK "What is 2 + 2?"
```

A successful answer confirms that the basic provider path is functioning.

This is worth testing separately from project operations. If a later AGENT request fails, you will know whether you are troubleshooting the LLM connection or the project itself.

---

# 4. ASK Versus AGENT

Two important concepts for a new user are **ASK** and **AGENT**.

Use `ASK` when you primarily want an answer from the configured LLM.

For example:

```text
ASK "Explain the difference between an OpenVMS logical name and a DCL symbol."
```

Use `AGENT` when you want the LLM to work with a software project using OVMS Agent's project tools.

For example:

```text
AGENT "Analyze this project and describe its structure. Do not modify any files."
```

A useful way to think about the distinction is:

```text
ASK
 |
 +----> Ask the LLM a question


AGENT
 |
 +----> LLM
          |
          +----> OVMS Agent tools
                    |
                    +----> Project
```

When working on source code, `AGENT` will normally be the command you use.

---

# 5. Multiline AGENT Requests

Real programming requests are often too large for a comfortable single command line.

OVMS Agent supports multiline input for commands that accept multiline text.

Enter the command, continue entering the request, and terminate the input with:

```text
.END
```

For example:

```text
AGENT "Analyze this project.

Identify the programming languages, build system, major modules,
entry points, dependencies, and tests.

Do not modify any files.

.END
```

`.END` tells OVMS Agent that the multiline request is complete.

You do not need a special `/MULTILINE` command merely because your request spans several lines.

---

# 6. Starting a New Project

Suppose you want to create a new project called `MYPROJECT`.

First create the directory:

```text
$ CREATE/DIRECTORY SYS$LOGIN:[PROJECTS.MYPROJECT]
$ SET DEFAULT SYS$LOGIN:[PROJECTS.MYPROJECT]
```

Initialize Git:

```text
$ git init
```

Then start OVMS Agent using the startup procedure provided by your installation.

For example, if OVMS Agent is installed beneath your login directory:

```text
$ @SYS$LOGIN:[OVMS_AGENT]OVMS_AGENT
```

Once OVMS Agent is running, begin with a planning request rather than immediately asking it to create many files.

For example:

```text
AGENT "I want to create a new OpenVMS application.

First inspect the current project directory.

Do not modify anything yet.

Recommend a project structure, build strategy, test strategy,
and implementation plan.

.END
```

This gives you an opportunity to review the proposed design before allowing changes.

Once satisfied with the plan, ask the agent to implement a small, testable portion of it.

---

# 7. Working with an Existing Local Project

OVMS Agent can also work with a project that already exists on your OpenVMS system.

Before starting the agent, enter the project directory:

```text
$ SET DEFAULT SYS$LOGIN:[PROJECTS.EXISTING_PROJECT]
```

If the project already uses Git:

```text
$ git status
```

Before allowing an agent to modify an existing project, it is strongly recommended that the working tree be in a known state.

If you already have unfinished changes, either commit them or make sure you understand exactly what they are.

A clean starting point makes it much easier to distinguish your previous work from changes made during an agent session.

---

# 8. Downloading Your Own GitHub Project

If you already have a project on GitHub, you can clone it to the OpenVMS system.

For example:

```text
$ SET DEFAULT SYS$LOGIN:[PROJECTS]

$ git clone https://github.com/YOURNAME/YOURPROJECT.git
```

Then enter the project directory:

```text
$ SET DEFAULT [.YOURPROJECT]
```

Inspect the repository before doing anything else:

```text
$ git status
$ git branch
$ git remote -v
```

Start OVMS Agent and initially ask it to inspect rather than modify the project.

For example:

```text
AGENT "Inspect this project.

Do not modify any files.

Identify the languages, directory structure, build system,
main entry points, tests, and important dependencies.

Tell me how you believe the project should be built on OpenVMS.

.END
```

---

# 9. Downloading Someone Else's GitHub Project

You do not need write access to a GitHub repository in order to analyze it.

For a public repository, clone it normally:

```text
$ SET DEFAULT SYS$LOGIN:[PROJECTS]

$ git clone https://github.com/OWNER/PROJECT.git
```

Enter the new repository:

```text
$ SET DEFAULT [.PROJECT]
```

Check its state:

```text
$ git status
$ git branch
$ git remote -v
```

You now have your own local working copy.

Cloning another person's repository does **not** normally give you permission to push changes back to that person's GitHub repository.

That is fine for analysis.

You can:

- inspect the source;
- create local branches;
- modify your local copy;
- build the project;
- run tests;
- make local commits;
- experiment with fixes.

If you later want to contribute your changes to the original project, you will normally fork the repository on GitHub and submit a pull request.

---

# 10. Analyze Before You Modify

When examining unfamiliar software, begin with an inspection-only request.

A good first request is:

```text
AGENT "Analyze this project without modifying any files.

Identify:

- the programming languages;
- the directory structure;
- the build system;
- the primary entry points;
- important dependencies;
- available tests;
- OpenVMS-specific code;
- likely portability issues.

Summarize your findings before recommending any changes.

.END
```

This gives the agent an opportunity to understand the repository before changing it.

For an unfamiliar project, this is safer than immediately asking:

```text
Fix everything.
```

---

# 11. Determine How the Project Builds

After the initial inspection, ask the agent to investigate the build.

For example:

```text
AGENT "Determine how this project is built.

Do not modify any files yet.

Identify the compiler or interpreter required for each language,
the build procedure, required libraries, and external dependencies.

Tell me what you would execute to test the build.

.END
```

OVMS Agent understands supported native build profiles and can use compiler and interpreter output as evidence when diagnosing problems.

A toolchain may be installed on OpenVMS without being initialized in the current DCL process. This distinction can be important for products such as Java, Perl, and Python.

If a tool cannot be invoked, do not immediately assume that it is not installed.

It may simply require its vendor setup procedure or foreign-command environment to be initialized.

---

# 12. Diagnosing a Build Failure

Suppose a build produces compiler errors.

A useful request is:

```text
AGENT "Investigate the compiler errors from the last build.

Explain the root cause first.

Do not modify any files yet.

Identify the files and lines involved and recommend the smallest
safe correction.

.END
```

Once you understand the proposed solution, you can authorize the agent to make the change.

This creates a useful workflow:

```text
Build
  |
  v
Compiler diagnostics
  |
  v
Agent analysis
  |
  v
Proposed correction
  |
  v
Guarded modification
  |
  v
Rebuild
```

OVMS Agent preserves native diagnostic evidence so that compiler and interpreter messages can participate in the reasoning process rather than being reduced to a simple success/failure result.

---

# 13. Before Letting the Agent Modify a Project

Git provides an important safety net.

Before substantial agent work, check:

```text
$ git status
$ git branch
```

For significant work, create a separate branch.

For example:

```text
$ git checkout -b agent-work
```

The exact Git command available may vary with the Git version installed on your OpenVMS system.

A recommended workflow is:

```text
Known Git state
      |
      v
Create working branch
      |
      v
Agent inspection
      |
      v
Agent modification
      |
      v
Build and test
      |
      v
Review Git diff
      |
      v
Commit
```

This keeps experimental agent work separate from known-good code.

---

# 14. Guarded Writes and Approval

OVMS Agent is designed so that reasoning about a change and being authorized to perform that change are separate concepts.

An LLM suggesting a modification does not automatically mean that unrestricted modification should occur.

Depending on the configured approval mode and requested operation, OVMS Agent may require authorization before performing actions that modify project state.

When approval is requested:

1. Read the proposed operation.
2. Verify the target files.
3. Verify that the operation matches your request.
4. Approve it only if you want the operation performed.
5. Build and test afterward.
6. Review the resulting Git changes.

Do not treat an approval prompt as something that should automatically be answered "yes."

It is part of the safety boundary.

---

# 15. Review What Changed

After an agent modification, return to DCL when appropriate and inspect Git.

Start with:

```text
$ git status
```

Then inspect the changes:

```text
$ git diff
```

Do not rely solely on the agent's description of what changed.

Git provides an independent view of the actual working tree.

If the changes are correct, build and test them before committing.

---

# 16. Commit a Known-Good Change

Once you have reviewed the changes and the project passes its tests:

```text
$ git status
$ git diff
```

Then stage the intended files:

```text
$ git add filename
```

Commit them:

```text
$ git commit -m "Describe the change"
```

If the repository belongs to you and has a configured GitHub remote, you can then push the branch according to your normal Git workflow.

Keeping commits small and focused makes both human review and future agent analysis easier.

---

# 17. Recommended First Session

A good first OVMS Agent session does not need to modify anything.

Try this workflow on an existing project.

First:

```text
AGENT "Analyze this project without modifying anything.

Explain the directory structure, languages, build system,
entry points, tests, and dependencies.

.END
```

Then:

```text
AGENT "Determine how to build and test this project on OpenVMS.

Do not modify anything.

Report any missing dependencies or toolchain problems.

.END
```

Then:

```text
AGENT "Review this project for likely bugs, portability problems,
unsafe assumptions, and missing tests.

Do not modify anything.

Rank your findings by importance and explain the evidence for each.

.END
```

At this point you have exercised much of the agent's analytical capability without changing the repository.

Choose one finding and ask the agent to investigate it in greater depth before authorizing a modification.

---

# 18. A Practical Development Loop

Once you are comfortable with OVMS Agent, a typical development session becomes:

```text
             +--------------------+
             | Describe the goal  |
             +---------+----------+
                       |
                       v
             +--------------------+
             | Agent investigates |
             +---------+----------+
                       |
                       v
             +--------------------+
             | Review its plan    |
             +---------+----------+
                       |
                       v
             +--------------------+
             | Guarded change     |
             +---------+----------+
                       |
                       v
             +--------------------+
             | Build / test       |
             +---------+----------+
                       |
                       v
             +--------------------+
             | Review Git diff    |
             +---------+----------+
                       |
                 problem?
                  /     \
                yes      no
                /         \
               v           v
        diagnose again    commit
```

The agent is most useful when it can repeatedly inspect real evidence, make a bounded change, and test the result.

---

# 19. What OVMS Agent Can See and Do

Depending on configuration and approval policy, OVMS Agent can reason about information such as:

- project source files;
- project directory structure;
- Git state;
- build procedures;
- OpenVMS filespecs;
- supported language build profiles;
- compiler diagnostics;
- interpreter diagnostics;
- tool results;
- previous operations within the active task.

The project root is an important security and correctness boundary.

Do not assume that pointing OVMS Agent at one project gives it unrestricted authority over unrelated files elsewhere on the OpenVMS system.

Likewise, access to a local Git repository does not automatically imply permission to push to its GitHub remote.

---

# 20. Good Prompts Produce Better Work

Give the agent a concrete goal and useful constraints.

Instead of:

```text
AGENT "Fix this project."
```

prefer:

```text
AGENT "Investigate why the current build fails.

Use the compiler diagnostics as evidence.

Do not modify anything until you can explain the root cause.

Then recommend the smallest correction that preserves the
existing architecture.

.END
```

For implementation work, describe what must remain unchanged as well as what should change.

For example:

```text
AGENT "Add the requested feature.

Preserve the existing command syntax and output unless a change
is required for the feature.

Keep compatibility with the project's existing OpenVMS compiler.

Run the relevant tests after the modification.

.END
```

Clear constraints make the agent's work easier to review.

---

# 21. Troubleshooting

## The LLM Does Not Respond

First test the provider with a simple `ASK`.

If that fails, investigate the LLM configuration before troubleshooting project tools.

Check:

- provider configuration;
- endpoint;
- model name;
- credentials;
- network connectivity.

## Git Can Fetch but Cannot Push

Fetching a public repository and pushing to a repository require different permissions.

Check:

```text
$ git remote -v
```

Verify that you have write permission to the destination repository and that Git authentication is configured correctly.

## GitHub Keeps Asking for Credentials

Review your Git authentication configuration.

Do not work around authentication problems by placing credentials directly in source files or committed command procedures.

## The Agent Says a File Is Outside the Project

The project root is deliberately enforced.

Confirm that you started the agent in, or explicitly configured it for, the intended project.

Do not weaken the project boundary merely to make an unexpected path work.

## A Compiler or Interpreter Cannot Be Found

An installed toolchain may not yet be initialized in the current DCL process.

Check whether the product requires a setup command procedure or foreign-command symbol.

Installed and initialized are not necessarily the same thing.

## The Compiler Reports an Error

Let the agent inspect the actual compiler diagnostics.

For example:

```text
AGENT "Analyze the most recent compiler failure.

Do not modify files.

Explain each actionable diagnostic and identify the likely root cause.

.END
```

## Git Metadata and Working Files Disagree

OpenVMS Git environments can have platform-specific behavior involving repository metadata and working-tree materialization.

Do not immediately repair or rewrite `.git` metadata based on Unix assumptions.

Consult the project's OpenVMS Git documentation before manipulating repository internals.

---

# 22. Safety When Analyzing Unfamiliar Projects

Downloading source code is not the same thing as trusting it.

When analyzing a project from someone else:

1. Clone the repository.
2. Inspect it before executing anything.
3. Ask OVMS Agent to analyze the source and build procedures without modification.
4. Review unfamiliar command procedures before running them.
5. Understand what external tools or services the build uses.
6. Build only after you are comfortable with what will execute.

A useful first request is:

```text
AGENT "Inspect this downloaded project for me.

Do not modify files and do not execute its build yet.

Identify its build procedures and summarize what they would execute,
including external programs, scripts, network operations, or other
potentially significant actions.

.END
```

An agent can help inspect unfamiliar code, but it should not turn an untrusted repository into trusted code merely because an LLM has looked at it.

---

# 23. Where to Go Next

Once you are comfortable with the workflow in this guide, explore the main OVMS Agent documentation for:

- command reference;
- approval modes;
- guarded writes;
- project-root behavior;
- native build support;
- language profiles;
- planning and saved plans;
- Git/OpenVMS operational guidance;
- provider configuration;
- external tools;
- advanced agent workflows.

The basic pattern remains the same:

```text
Give the agent a goal
        |
        v
Let it gather evidence
        |
        v
Review its reasoning and proposed action
        |
        v
Authorize bounded changes
        |
        v
Build and test
        |
        v
Verify with Git
```

Start small.

Let the agent understand the project before asking it to change the project.

And keep Git between experimentation and regret.
