# Contributing to launcher

This guide describes how to contribute to `launcher`, from finding
something to work on to getting a pull request merged.

## Getting started

Issues are the place to start. Any open issue is up for grabs once you say in
it that you are working on it.

If you want to make a change that has no issue yet, open one first and
describe what you have in mind, unless the change is small (a typo or an
obvious bug, for example). This way the approach is agreed on before any code
is written. For questions that are not about a specific change, ask on
[Discord](https://discord.com/invite/pV2qJscTXf).

## Development

`launcher` is built with [build2](https://build2.org). The Development
section of [README.md](../README.md) lists the requirements and the commands
to set up a build configuration. If you are new to build2, the
[build2 toolchain introduction](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml)
explains the workflow.

The repository holds one package, `launcher`, with its sources in
`launcher/launcher/` and its tests in `launcher/launcher/testscript`. The
command line parser is generated from `launcher/launcher/launcher.cli` by the
development build, which `bdep init` configures by default and which keeps
the copy in `launcher/launcher/pregenerated/` up to date. Commit that copy
together with the change to `launcher.cli`.

Work on a branch of your fork of the repository and keep it up to date with
`main` by rebasing it. The history is linear, so a branch with merge commits
cannot be merged.

## Commits

Pull requests are merged by rebasing, so each commit of a pull request ends
up in the history as it is. Each commit therefore makes one self-contained
change that builds and passes the tests on its own. If a later commit fixes
an earlier one, fold it into the commit it fixes (`git commit --fixup` and
`git rebase --autosquash`) and force-push the branch.

The commit message is a subject line in the imperative mood, without a
trailing period, that says what the commit does. It starts with one of the
verbs of the [Keep a Changelog](https://keepachangelog.com/) categories, so
that the history maps directly onto the `NEWS` entries:

* `Add` for new features.
* `Change` for changes in existing functionality.
* `Deprecate` for soon-to-be removed features.
* `Remove` for now removed features.
* `Fix` for any bug fixes.
* `Security` in case of vulnerabilities.

The subject leaves out articles (`a`, `an`, `the`), as in `Add test for
version header`. For example:

```
Add IW4x linking exception
Add authors file
```

The history of the repository has plenty of examples to follow.

## Tests

Every change that affects behavior comes with a test, and every bug fix comes
with a test that fails without the fix. The tests are written in
[Testscript](https://build2.org/build2/doc/build2-testscript-manual.xhtml)
and run with `bdep test` (or `b test` in a forwarded configuration).

Give each test an explicit id (for example, `: missing-name`), with a summary
if it tests a failure. A test checks one thing at a time. Where the output
legitimately varies (a path, a system error message), match it with a
regular expression. The diagnostics of a program are part of what the tests
check, so they are never ignored.

## Continuous integration

Before opening a pull request, run the tests on other platforms as well.
`bdep ci` submits the packages of the repository, at the current commit of
the branch, to the build2 CI service at [ci.cppget.org](https://ci.cppget.org)
and prints a link to the results:

```
git push origin <branch>
bdep ci
```

The branch has to be pushed first, since the CI service builds it from the
repository the branch is in (your fork, normally). A pull request is merged
only once its CI results are clean.

## Pull requests

A pull request targets `main` and makes one change, which its description
explains together with the reason for it. Link the issue it resolves (with
`Closes #<number>`, the issue is closed when the pull request is merged).

A maintainer reviews the pull request and may ask for changes. Push them as
new commits while the review is in progress, if that makes them easier to
follow, and fold them into the commits they change before the pull request
is merged.

## Licensing

By contributing to `launcher`, you agree that your contribution is
licensed under its license, the GNU General Public License, version 3, with
the additional permissions of version 1.1 of the IW4x Linking Exception (see
`LICENSE.md` and `LICENSE-EXCEPTION.md`).

Only contribute work that you have the right to license this way. If a
change includes code from elsewhere, say so in the pull request, together
with its origin and license.

## Security

Please report security vulnerabilities privately. See
[SECURITY.md](SECURITY.md) for how.
