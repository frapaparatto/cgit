# Git Remotes, Fetch, and Branch References — Deep Dive

## The Three Copies of "master"

After your setup, there are THREE things called "master" in your local repo. They look similar but are fundamentally different:

```
master                ← Your LOCAL branch. You own it. You commit on it. It moves when YOU do something.
origin/master         ← A snapshot of your fork's master on GitHub. Read-only. Moves only when you fetch from origin.
upstream/master       ← A snapshot of git/git's master on GitHub. Read-only. Moves only when you fetch from upstream.
```

### What "snapshot" means concretely

When you run `git fetch upstream`, Git connects to github.com/git/git, downloads any new commits, and updates `upstream/master` to point where their master currently is. But YOUR `master` doesn't move. It stays exactly where it was.

Think of it this way:

```
After cloning (day 1):
  upstream/master → commit A
  origin/master   → commit A
  master          → commit A          # All three point to the same commit

Someone pushes to git/git (day 2, you don't know yet):
  git/git's master → commit B         # On GitHub, the original moved
  upstream/master → commit A          # YOUR local snapshot is STALE — still A
  master          → commit A          # YOUR branch — unchanged

You run git fetch upstream (day 2):
  upstream/master → commit B          # NOW your snapshot is updated
  master          → commit A          # YOUR branch — STILL unchanged. Fetch doesn't touch your branches.

You run git rebase upstream/master (day 2):
  upstream/master → commit B
  master          → commit B          # NOW your branch moved — you explicitly told it to
```

This is the key insight: **fetch updates snapshots, not your branches.** Your branches only move when you explicitly tell them to (commit, rebase, merge, reset).

### Why this design?

Imagine you're in the middle of working on something. You've made 3 commits on your `master`. If `git fetch` automatically moved your `master` to match upstream, your 3 commits would be lost or conflicted without warning. By keeping fetch and branch-update separate, you stay in control.

## The Full Picture

```
┌─────────────────────────────────────────────────────────────────┐
│                     GITHUB (remote servers)                      │
│                                                                  │
│   github.com/git/git              github.com/frapaparatto/git   │
│   (upstream)                      (origin)                       │
│   └── master → commit X           └── master → commit Y         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
        │                                    ▲
        │ git fetch upstream                 │ git push origin
        │ (download snapshots)               │ (upload your work)
        ▼                                    │
┌─────────────────────────────────────────────────────────────────┐
│                     YOUR LOCAL REPO                              │
│                                                                  │
│   Remote-tracking refs (READ-ONLY snapshots):                    │
│   ├── upstream/master → last known state of git/git's master    │
│   └── origin/master   → last known state of your fork's master  │
│                                                                  │
│   Local branches (YOUR work):                                    │
│   ├── master              → your main branch                    │
│   └── microproject-test   → your feature branch                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Commands and What They Affect

| Command | What moves | What doesn't move |
|---|---|---|
| `git fetch upstream` | `upstream/master` (and all `upstream/*`) | `master`, `origin/master`, your branches |
| `git fetch origin` | `origin/master` (and all `origin/*`) | `master`, `upstream/master`, your branches |
| `git rebase upstream/master` | Your current branch (e.g. `master`) | `upstream/master`, `origin/master` |
| `git push origin master` | `origin/master` on GitHub (your fork updates) | `upstream/master`, local snapshots |
| `git commit` | Your current branch (adds a new commit) | Everything else |

## The Daily Workflow Explained Step by Step

```bash
git fetch upstream
```
"Download all new commits from git/git and update my local snapshots (upstream/master etc.). Don't touch my branches."

```bash
git checkout master
```
"Switch to my local master branch." Needed because you might be on a feature branch.

```bash
git rebase upstream/master
```
"Take my local master and replay it on top of wherever upstream/master is now." After this, your master matches the latest state of git/git. If you had local commits, they get replayed on top (so your changes sit on top of the latest code).

```bash
git checkout -b microproject-test
```
"Create a new branch from where I am now (which is the latest upstream/master) and switch to it." This is where you do your actual work.

```bash
# ... work, test, commit ...
git push origin microproject-test
```
"Upload my microproject-test branch to my fork on GitHub." This triggers CI. Does NOT affect git/git in any way.

## How to See All of This

```bash
git remote -v                    # Shows your remotes and their URLs
git branch -a                    # Shows ALL branches: local + remote-tracking
git log --oneline --graph --all  # Visual graph of all branches and where they point
git log master..upstream/master  # Shows commits that upstream has but you don't
```

## Key Takeaways

1. `upstream/master` and `origin/master` are NOT branches you work on. They're bookmarks that say "last time I checked, this is where that remote's master was."
2. `git fetch` updates bookmarks. `git rebase` or `git merge` updates your actual branch.
3. You can ALWAYS see where everything points with `git log --oneline --graph --all`.
4. `git push origin` only affects your fork. Never touches upstream.
