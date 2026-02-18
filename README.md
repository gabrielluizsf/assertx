# AssertX 

## Installation Guide

This guide explains how to install the **AssertX binary** on Linux, macOS, and Windows and make it accessible from anywhere via your system `PATH`.

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
