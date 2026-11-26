# <img src="https://keepassxc.org/assets/img/keepassxc.svg" width="40" height="40"/> KeePassXC

[![OpenSSF Best Practices](https://bestpractices.coreinfrastructure.org/projects/6326/badge)](https://bestpractices.coreinfrastructure.org/projects/6326)
[![TeamCity Build Status](https://ci.keepassxc.org/app/rest/builds/buildType:\(project:KeepassXC\)/statusIcon)](https://ci.keepassxc.org/?guest=1)
[![codecov](https://codecov.io/gh/keepassxreboot/keepassxc/branch/develop/graph/badge.svg)](https://codecov.io/gh/keepassxreboot/keepassxc)
[![GitHub release](https://img.shields.io/github/release/keepassxreboot/keepassxc)](https://github.com/keepassxreboot/keepassxc/releases/)

[![Matrix community channel](https://img.shields.io/matrix/keepassxc:matrix.org?label=Community%20channel)](https://app.element.io/#/room/#keepassxc:mozilla.org)
[![Matrix development channel](https://img.shields.io/matrix/keepassxc-dev:matrix.org?label=Development%20channel)](https://app.element.io/#/room/#keepassxc-dev:mozilla.org)

## Interactive CLI in this fork

The `feature/interactive-completion` branch is based on KeePassXC 2.8.0. It lets you navigate database groups like directories in a shell.

- **Group navigation.** Use `ls`, `cd`, and `pwd` to browse the database. Entry and group commands accept paths relative to the current group or absolute paths from the database root. Use `..` for the parent group and `cd -` to return to the previous group.
- **Context-aware Tab completion.** Complete commands, options, field names, and paths. A group candidate ends in `/` so you can continue completing within it. When several candidates match, Tab extends their common prefix, then lists and cycles through the candidates.
- **Two-line prompt.** The first line shows the database name and current group path. The second is for input. Color distinguishes groups, field labels, and clipboard status. Set `NO_COLOR=1` to disable it.
- **Show and copy.** `show -c` (short for `show --copy`) displays the entry and copies its password. The default summary masks protected fields. `--copy-attribute` copies a named field or the current TOTP. Both `show --copy` and `clip` clear the clipboard after 10 seconds by default. On macOS, press Enter during the countdown to clear it immediately.

### Build on macOS

With Xcode Command Line Tools and Homebrew installed, build the CLI from the fork. This recipe was verified on Apple Silicon. The explicit Readline paths select GNU Readline for context-aware completion.

```sh
git clone --branch feature/interactive-completion https://github.com/Yunie9/keepassxc.git
cd keepassxc
brew install cmake qttools qtsvg botan qrencode minizip readline

readline_prefix="$(brew --prefix readline)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
  -DWITH_APP_BUNDLE=OFF -DKPXC_MINIMAL=ON -DKPXC_FEATURE_DOCS=OFF \
  -DReadline_INCLUDE_DIR="$readline_prefix/include" \
  -DReadline_LIBRARY="$readline_prefix/lib/libreadline.dylib"
cmake --build build --target keepassxc-cli --parallel
```

See [INSTALL.md](INSTALL.md) for general KeePassXC build instructions and other platforms.

### Use the interactive CLI

Open a database and unlock it when prompted.

```sh
./build/src/cli/keepassxc-cli open "$HOME/Documents/personal.kdbx"
```

This example assumes a group named `Work Space` with an entry named `GitHub Work`. Replace the names with ones from your database. Type the commands after `>`.

```text
[personal.kdbx] /
> cd "Work Space"

[personal.kdbx] /Work Space
> pwd
/Work Space

[personal.kdbx] /Work Space
> ls
GitHub Work

[personal.kdbx] /Work Space
> show -c "GitHub Work"
```

Try typing `show -c Git` and pressing Tab to complete the entry name. Run `help show` or `help clip` to see the available options.

```text
show --copy --timeout 20 "GitHub Work"
show "GitHub Work" --copy-attribute "API Key"
show "GitHub Work" --copy-attribute totp
clip "GitHub Work" 20
```

For a custom field, type the entry path first, then press Tab after `--copy-attribute` and the start of the field name. The `API Key` and `totp` examples require the corresponding field or TOTP setup. Set the timeout to `0` to leave the copied value on the clipboard until you clear or replace it.

## About KeePassXC

[KeePassXC](https://keepassxc.org) is a modern, secure, and open-source password manager that stores and manages your most sensitive information. You can run KeePassXC on Windows, macOS, and Linux systems. KeePassXC is for people with extremely high demands of secure personal data management. It saves many different types of information, such as usernames, passwords, URLs, attachments, and notes in an offline, encrypted file that can be stored in any location, including private and public cloud solutions. For easy identification and management, user-defined titles and icons can be specified for entries. In addition, entries are sorted into customizable groups. An integrated search function allows you to use advanced patterns to easily find any entry in your database. A customizable, fast, and easy-to-use password generator utility allows you to create passwords with any combination of characters or easy to remember passphrases.

## Quick Start

The [QuickStart Guide](https://keepassxc.org/docs/KeePassXC_GettingStarted.html) gets you started using KeePassXC on your Windows, macOS, or Linux computer using pre-compiled binaries from the [downloads page](https://keepassxc.org/download). Additionally, individual Linux distributions may ship their own versions, so please check your distribution's package list to see if KeePassXC is available. Detailed documentation is available in the [User Guide](https://keepassxc.org/docs/KeePassXC_UserGuide.html).

## Features List

KeePassXC has numerous features for novice and power users alike. Our goal is to create an application that can be used by anyone while still offering advanced features to those that need them.

### Core Features

* Create, open, and save databases in the KDBX format (KeePass-compatible with KDBX4 and KDBX3)
* All information is encrypted at rest and never exposed outside the program
* Store sensitive information in entries that are organized by groups
* Password generator
* Search for entries
* TOTP storage and generation
* YubiKey/OnlyKey challenge-response support
* Auto-Type passwords into applications
* Browser integration with Google Chrome, Mozilla Firefox, Microsoft Edge, Chromium, Vivaldi, Brave, and Tor-Browser
* Support for passkeys using the browser integration
* Entry icon download
* Import databases from CSV, 1Password, Bitwarden, Proton Pass, and KeePass1 formats

### Advanced
* Database reports (password health, HIBP, and statistics)
* Database export to CSV, XML, and HTML formats
* TOTP storage and generation
* Field references between entries
* File attachments and custom attributes
* Entry history and data restoration
* Command line interface (keepassxc-cli)
* SSH Agent integration
* FreeDesktop.org Secret Service (replace Gnome keyring, etc.)
* Additional encryption choices: Twofish and ChaCha20

For a full list of changes, read the [CHANGELOG](CHANGELOG.md) document. \
For a full list of keyboard shortcuts, see [KeyboardShortcuts.adoc](./docs/topics/KeyboardShortcuts.adoc)

## Building KeePassXC

Detailed instructions are available in the [Build and Install](./INSTALL.md) page and in the [Wiki](https://github.com/keepassxreboot/keepassxc/wiki/Building-KeePassXC).

## Contributing

We are always looking for suggestions on how to improve KeePassXC. If you find any bugs or have an idea for a new feature, please let us know by opening a report in the [issue tracker](https://github.com/keepassxreboot/keepassxc/issues) on GitHub, or join us on [Matrix community channel](https://matrix.to/#/!zUxwGnFkUyycpxeHeM:matrix.org?via=matrix.org) or [Matrix development channel](https://matrix.to/#/!RhJPJPGwQIFVQeXqZa:matrix.org?via=matrix.org), or on IRC in [Libera.Chat](https://web.libera.chat/) channels #keepassxc and #keepassxc-dev.

You may directly contribute your own code by submitting a pull request. Please read the [CONTRIBUTING](.github/CONTRIBUTING.md) document for further information.

Contributors are required to adhere to the project's [Code of Conduct](CODE-OF-CONDUCT.md).

## Generative AI

Generative AI is fast becoming a first-party feature in most development environments, including GitHub itself. If the majority of a code submission is made using Generative AI (e.g., agent-based or vibe coding) then **we will document that in the pull request.** All code submissions go through a rigorous review process regardless of the development workflow or submitter.

## License

KeePassXC code is licensed under GPL-2 or GPL-3. Additional licensing for third-party files is detailed in [COPYING](./COPYING).
