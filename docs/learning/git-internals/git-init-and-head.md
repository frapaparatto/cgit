# `git init`command
`git init` creates the minimal filesystem structure of a Git repository so that Git can later store objects and knows, via HEAD, which reference to update when commits are created.

Git repo is made of two things:
1. worktree: this is the location of files meant to be in version control;
2. git directory (`.git`): here is where git stores its own data;

Usually the structure is `worktree/.git`.

## HEAD
**HEAD** is a pointer that identifies the **current checkout target**.

In normal operation, **HEAD points to a name (a branch reference)**, and **that branch points to the last commit**.
So the resolution chain is:
```
HEAD → branch name → last commit
```

Example on `main`:
* `HEAD` points to `refs/heads/main`
* `refs/heads/main` contains the commit hash
* On every `git commit`, **only the hash stored in `main` changes**
* `HEAD` does not change

If you create and switch to a new branch:
* `HEAD` is updated to point to the new branch name
* Both branches may initially point to the same commit
* Future commits update only the branch selected by `HEAD`

In **detached HEAD**:
* `HEAD` points **directly to a commit hash**, not to a name
* New commits are created, but **no branch ref is updated**
* This creates a new line of development that is temporary unless you create a branch

**Why HEAD exists:**
HEAD exists **to keep track of which reference must be updated when a new commit is created**.
It must exist from `git init`, otherwise Git would have no target ref for the first commit.

**Key idea to remember:**
HEAD does not store history — it **routes mutations** to the correct reference.
