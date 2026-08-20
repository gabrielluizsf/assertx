# AssertX 

## Installation Guide

This guide explains how to install the **AssertX binary** on Linux, macOS, and Windows and make it accessible from anywhere via your system `PATH`.


## ⚡ Quick Install

Install AssertX instantly with one command:

### 🐧 Linux / macOS

```bash
curl -fsSL https://raw.githubusercontent.com/gabrielluizsf/assertx/main/install.sh | sh
```

### 🪟 Windows (PowerShell)

```sh
irm https://raw.githubusercontent.com/gabrielluizsf/assertx/main/install.ps1 | iex
```

---

## 📦 Download

Download the correct archive for your operating system:

| OS      | Archive                | Binary        |
| ------- | ---------------------- | ------------- |
| Linux   | `assertx-linux.tar.gz` | `assertx`     |
| macOS   | `assertx-macos.tar.gz` | `assertx`     |
| Windows | `assertx-windows.zip`  | `assertx.exe` |

---

### 🐧 Linux Installation

#### 1. Extract archive

```bash
tar -xzf assertx-linux.tar.gz
```

This will extract the binary:

```bash
assertx
```

---

#### 2. Move binary to a directory in PATH

Recommended location:

```bash
sudo mv assertx /usr/local/bin/
```

Alternative (no sudo required):

```bash
mkdir -p ~/.local/bin
mv assertx ~/.local/bin/
```

---

#### 3. Make executable

```bash
chmod +x /usr/local/bin/assertx
```

or

```bash
chmod +x ~/.local/bin/assertx
```

---

#### 4. Verify installation

```bash
assertx
```

---

### 🍎 macOS Installation

#### 1. Extract archive

```bash
tar -xzf assertx-macos.tar.gz
```

---

#### 2. Move binary

```bash
sudo mv assertx /usr/local/bin/
```

or user-only install:

```bash
mkdir -p ~/.local/bin
mv assertx ~/.local/bin/
```

---

#### 3. Make executable

```bash
chmod +x /usr/local/bin/assertx
```

or

```bash
chmod +x ~/.local/bin/assertx
```

---

#### 4. Verify

```bash
assertx
```

---

###  Windows Installation (Automatic via PowerShell)

#### 1. Extract and install using PowerShell

Place `assertx-windows.zip` in your current folder.

Open **PowerShell** and run:

```powershell
$log="$PWD\assertx_install.log";$z="$PWD\assertx-windows.zip";$d="$env:LOCALAPPDATA\AssertX";"=== AssertX Install $(Get-Date) ==="|Out-File $log -Append;try{if(!(Test-Path $z)){throw "ZIP não encontrado"};ni $d -it Directory -f|Out-Null;tar -xf $z -C $d 2>>$log;if(!(Test-Path "$d\assertx.exe")){throw "assertx.exe não encontrado"};$p=[Environment]::GetEnvironmentVariable("Path","User");if($p-notlike"*AssertX*"){[Environment]::SetEnvironmentVariable("Path","$p;$d","User")};"SUCCESS instalado"|Out-File $log -Append;Write-Host "✅ AssertX instalado"}catch{("ERROR: "+$_.Exception.Message)|Out-File $log -Append}
```

This command will:

* Extract the archive
* Install `assertx.exe` to:

```
C:\Program Files\AssertX\
```

* Automatically add AssertX to your system `PATH`

---

#### 2. Restart your terminal

Close and reopen PowerShell, CMD, or your terminal.

---

#### 3. Verify installation

```cmd
assertx
```

or

```cmd
where assertx
```

Expected output:

```
C:\Program Files\AssertX\assertx.exe
```

---

## ✅ Verify PATH configuration

Linux/macOS:

```bash
which assertx
```

Windows:

```cmd
where assertx
```

Expected output example:

Linux/macOS:

```
/usr/local/bin/assertx
```

Windows:

```
C:\Program Files\AssertX\assertx.exe
```

---

## 📁 Recommended Install Locations

| OS      | Location                             |
| ------- | ------------------------------------ |
| Linux   | `/usr/local/bin/` or `~/.local/bin/` |
| macOS   | `/usr/local/bin/` or `~/.local/bin/` |
| Windows | `C:\Program Files\AssertX\`          |

---

## 🎉 Done

AssertX is now globally accessible:

```bash
assertx ./tests
```

from anywhere in your system.


## 🧪 Using xassert.h in your tests

- **xassert.h is header-only — no compilation or linking required**

    - Just move it into your tests/ folder
        ```tree
        your-project/
        │
        ├─ src/
        │   └─ xmath.h
        │
        ├─ tests/
        │   ├─ xassert.h
        │   └─ xmath_test.c
        │
        └─ README.md

        ```

    - Include xassert.h in your test file

        - Example:
            ```c
            #include <stdio.h>
            #include "../src/xmath.h"
            #include "xassert.h"

            void test_sum()
            {
                assert_equal(xsum(5, 10), 15, "5 + 10 should be 15");
            }

            void test_div()
            {
                assert_equal(xdiv(10, 0), -1, "10/0 should be -1");
            }

            void test_is_even()
            {
                assert_true(is_even(4), "4 is even");
                assert_false(is_even(5), "5 is odd");
            }
            ```

    - Run tests using the assertx binary
        ```sh
        assertx ./tests
        ```

## Running tests

```sh
assertx ./tests
```

Compiling is nearly all of the time a run takes — the tests themselves are
usually a few milliseconds — so assertx does two things about it.

**It compiles in parallel.** Test files do not depend on each other, so they
are built several at a time, one per processor by default.

**It only rebuilds what changed.** Each test records what it was compiled
from, and on the next run a test whose sources are all older than its binary
is not compiled again. Editing one test rebuilds one test; editing a shared
header rebuilds everything that includes it.

On a suite of 20 files that each pull in a large header:

| | before | after |
|---|---|---|
| first run | 18.2s | 10.8s |
| nothing changed | 18.2s | 0.3s |
| one test changed | 18.2s | 2.0s |
| shared header changed | 18.2s | 10.2s |

### Where the cache lives

The compiled tests are what make the caching possible, and they are kept in
the system temporary directory — not in a `build/` folder beside your sources:

| OS | Directory |
|---|---|
| Linux | `$TMPDIR`, or `/tmp` |
| macOS | `$TMPDIR` (the per user directory macOS cleans up) |
| Windows | `%TMP%`, then `%TEMP%` — whatever `GetTempPath` returns |

Nothing in there is worth keeping: all of it can be produced again from the
test files. Every operating system already empties its own temporary directory,
so the cache is cleared without anybody having to remember to do it, nothing is
left behind in your project, and there is no `build/` to add to `.gitignore`.

Each test directory gets its own subdirectory, named after the project and a
hash of the directory's absolute path:

```
/tmp/assertx/myproject-3f2a1b9c/
```

so two projects that both have a `foo_test.c` never write over each other.
Deleting that directory, or passing `--no-cache`, forces a full rebuild.

Use `--cache-dir` if you would rather keep the cache somewhere else — a CI
runner that caches a directory between builds is the usual reason:

```sh
assertx ./tests --cache-dir .assertx-cache
```

### Options

```
assertx <test_directory> [options]

  -j <n>        compile up to n test files at once (default: one per processor)
  --no-cache    rebuild everything, even what has not changed
  --cc <name>   use this compiler instead of gcc
  --cache-dir <path>
                keep the cache here instead of in the system temporary directory
  -h, --help    print this
```

Environment variables: `CC` (same as `--cc`), `ASSERTX_CFLAGS` (replaces the
default `-Wall -Wextra -g`), `ASSERTX_JOBS` (same as `-j`),
`ASSERTX_CACHE_DIR` (same as `--cache-dir`).

> Parallel compilation is used on Linux, macOS and other Unix systems. On
> Windows the compiles still run one after another; the caching works the
> same everywhere.
