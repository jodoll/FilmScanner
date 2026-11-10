# Agent permissions

How OpenCode is allowed to act in this repository, and why the rules are shaped
the way they are. The policy lives in `opencode.jsonc` at the repository root.

That file is not generated. Changes to it prompt for review; see
[Changing the policy](#changing-the-policy).

---

## The model

The checkpoint is a **prompt plus human judgement**. Everything else is a
trade-off about *when* that prompt happens.

| What | When the human sees it | Rule |
| --- | --- | --- |
| Implementation code (`lib/`, `src/`, `test/`) | at execution: when the code runs, if it runs | free edit |
| Reads (`read`) | never inside the project; at execution outside it | `allow` (`.env*` and external paths `ask`) |
| Instructions and the spec (`AGENTS.md`, `PERMISSIONS.md`, `docs/`, everything under `.opencode/` except plugins) | at edit | `ask` |
| Shell commands | at execution | `ask` (git reads are free) |
| Policy (`opencode.jsonc`) | at edit | `ask` |
| Plugins (`.opencode/plugin/*`) | **never** | `deny` |
| Git machinery (`.git/*`, gitconfig, `push`) | n/a — execute on every later command | `deny` |

The distinction that drives the table is not "dangerous" versus "safe". It is
**whether a later prompt exists**:

- Implementation code is free to edit because using it always goes through an
  execution prompt — a build, a test run, a shell command.
- Instructions, the spec and the policy are `ask` because they change what a
  later session will do, and that change would otherwise be invisible.
- Plugins have no later prompt at all, so the edit is the only gate. They are
  `deny`.

---

## Why each group is protected

### Instructions and the spec — `ask`

`AGENTS.md`, `PERMISSIONS.md` and `docs/**` are the agent's instructions and the
behaviour specification. A silent change there misleads every future session, so
it prompts. The rest of `.opencode/` outside `plugin/` — `skills/`, `command/`,
`tool/`, `agent/` — is the same class: content that shapes later behaviour and
is invisible unless the edit is reviewed.

### Policy file — `ask`

`opencode.jsonc`. Approving an edit here is approving the rules that govern
everything afterwards. It is not `deny` because a policy change should stay
proposable by the agent, with a human reading the diff — but the config
**hot-reloads on save**, so an approved edit takes effect immediately. There is
no second step to catch a mistake.

### Plugins — `deny`

`.opencode/plugin/*` is JavaScript loaded by the OpenCode server at startup. It
is not a tool call and not a shell command: it runs in the server process that
enforces permissions, with no prompt at load time and no prompt at use time. It
can rewrite files, spawn processes, or delete the saved-approval rows directly.

That makes it the only artifact where "the prompt is the checkpoint" does not
hold, because after the write there is no checkpoint left.

`deny` is a speed bump, not a wall. A shell command is `ask`, and
`cat > .opencode/plugin/x.js` would prompt and then work. The deny's value is
that it makes the casual path conspicuous: an agent that should have used `edit`
is instead piping a file through the shell, which is visible in the approval
prompt and is exactly the signal worth stopping on.

### Git machinery — `deny`

`.git/`, `*.gitconfig`, the system gitconfigs, and `git push`. These are not
denied because they are destructive in a single command — that is what the
`ask` on shell already covers. They are denied because a change there executes
on **every later** git command, including the ones that are free. A
`[alias] status = !cmd` in `.git/config` defeats the read-only allowlist
permanently, without ever prompting again.

---

## Operating rules

**Last matching rule wins.** Rules are evaluated in order and the last match
applies, so `edit allow *` and `read allow *` come first and every `ask` and
`deny` follows them. Keep that order when editing the file.

**`glob` and `grep` cannot be path-scoped.** Their rules are keyed on the glob
pattern and the regular expression, not on a path, so allowing them would allow
searches anywhere the process can reach. They stay on `ask`.

**`deny` cannot be approved.** `ask` can. That is the only hard distinction;
everything else is a question of where the prompt appears.

**Saved approvals override `ask`, never `deny`.** When you approve a tool with
the persistent option, OpenCode stores a project-scoped `allow` rule that is
appended after the config rules, silently defeating every matching `ask` in this
document. Approve **once**, not always.

The stored approvals live in `~/.local/share/opencode/opencode.db`, table
`permission`. Enumerate them with:

```sh
sqlite3 -header -column "$HOME/.local/share/opencode/opencode.db" \
  "select id, action, resource from permission where project_id = '<project-id>';"
```

Delete them through the API rather than by editing the DB, so the server's state
stays consistent:

```sh
opencode api delete /api/permission/saved/<id>
```

`GET /api/permission/saved` can report an empty list even when rows exist (a
worktree may have more than one project id), so trust the `select`.

**Config changes apply without a restart.** Hot reload is why the policy file is
protected and why the plugin deny exists.

---

## What no rule can fix

- **Running the tests runs agent-authored code.** `pio test -e native` compiles
  `lib/**` and `test/**` and executes them on the host. Those files are free to
  edit by design, so reviewing the diff is the control, not the permission
  layer.
- **Shell is the universal escape hatch.** Any `ask`-ed command can write a
  file, fetch code, or run a script. The prompts make this visible; they do not
  prevent it.
- **The shell file-write convention is not enforced.** `AGENTS.md` says file
  changes go through `edit`/`write`; the rules cannot verify it.
- **Repository aliases shadow allowed verbs.** `git status` free means the
  literal command, not whatever `.git/config` says `status` means — which is why
  `.git/*` is denied.
- **Trust in the clone.** A partial clone fetches on read; the rules do not
  inspect that.

---

## Changing the policy

`opencode.jsonc` prompts on edit (`opencode.json*` is `ask`), so the agent can
propose a change and you decide. Two ways to change it:

1. **By hand.** Edit `opencode.jsonc` directly. Preferred for anything other
   than a one-line tweak.
2. **Through the agent, with `edit`.** It prompts; approve the diff if you want
   it. Review it before you run `git commit` — the config hot-reloads, so the
   change is live the moment it lands.

After a change, commit `opencode.jsonc`.
