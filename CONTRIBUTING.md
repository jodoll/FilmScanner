# Contributing

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/): a type, a
scope, a colon, and a subject. These rules apply to every commit, whether it
comes from a person or an agent.

### Types

Use exactly one of these types. Extend this list deliberately when a new type is
genuinely needed; do not invent one in a commit message.

| Type | Use for |
|---|---|
| `feat` | New firmware behavior or a user-visible capability |
| `fix` | A bug fix |
| `refactor` | Restructuring that does not change behavior |
| `test` | Anything that exists for testing: native and hardware tests, fakes, fixtures, test setup, test procedures and testing documentation |
| `docs` | Human-facing documentation: `docs/`, `README.md`, `CONTRIBUTING.md` |
| `build` | `platformio.ini`, dependencies, toolchains and build configuration |
| `tools` | Agent and repository tooling: `AGENTS.md`, `.opencode/`, permissions, skills, helper scripts |
| `chore` | Housekeeping that fits nowhere else: `.gitignore`, licensing, cleanup |
| `revert` | Reverting an earlier commit |

### Scope

The scope is required. Keep it a short lower-case area name such as `core`,
`hal`, `proto`, `config`, `native`, `testing`, `plan` or `agents`. Use a new
area name when none of the usual ones fits; scopes are not a closed list.

### Subject

The subject after the colon starts with an upper-case letter. The first word
stays lower-case only when it is a literal identifier — a method, type, command,
config key or file name that the commit is about.

```text
feat(core): Reject scanOneFrame outside IDLE
feat(core): scanOneFrame transitions to SEEK
tools(agents): Require approval for git commits
```
