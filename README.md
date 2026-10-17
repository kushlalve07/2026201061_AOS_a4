# mygit: A Mini Version Control System

A small Git clone in C++17. It stores files as content-addressed objects (SHA-1 + zlib), exactly in Git's object format, so real `git` can read the objects it creates.

---

## Build and Run

- **Requirements:** `g++` (C++17), OpenSSL (`libcrypto`, for SHA-1), zlib (for compression)
  - Ubuntu: `sudo apt install g++ libssl-dev zlib1g-dev`
  - macOS: `brew install openssl` (the makefile finds it through `pkg-config`)
- **Build:** `make`, which produces the `./mygit` binary
- **Clean:** `make clean`
- **Run:** `./mygit <command> [args]`, always from the repository's root directory

---

## Files

| File | Purpose |
|---|---|
| `main.cpp` | Reads the command name and calls the matching `cmdXxx()` function |
| `mygit.h` | Shared constants (paths), structs and function declarations |
| `objects.cpp` | Core object logic: SHA-1, zlib compress/decompress, read/write objects, tree and commit parsing |
| `refs.cpp` | Reading and updating `HEAD` and branch files |
| `index.cpp` | Reading and writing the staging area (`.mygit/index`) |
| `init.cpp` | Requirement 1: `init` |
| `hash_object.cpp` | Requirement 2: `hash-object` |
| `cat_file.cpp` | Requirement 3: `cat-file` |
| `write_tree.cpp` | Requirement 4: `write-tree` |
| `ls_tree.cpp` | Requirement 5: `ls-tree` |
| `add.cpp` | Requirement 6: `add` |
| `commit.cpp` | Requirement 7: `commit` |
| `log.cpp` | Requirement 8: `log` |
| `checkout.cpp` | Requirement 9: `checkout` |

---

## Repository Layout (`.mygit/`)

```
.mygit/
├── objects/xx/yyyy...   every blob, tree and commit (zlib-compressed)
├── refs/heads/main      SHA of the latest commit on branch main
├── HEAD                 "ref: refs/heads/main", or a raw commit SHA (detached HEAD)
└── index                staging area, one line per file: <mode> <sha> <path>
```

---

## Core Idea: Objects

- There are 3 object types: **blob** (file content), **tree** (directory listing) and **commit** (snapshot plus metadata).
- Every object is stored as `<type> <size>\0<content>`.
- **SHA-1** of that whole string gives the object's 40-character name.
- The string is **zlib-compressed** and saved at `objects/<first 2 chars>/<remaining 38 chars>`.
- The same content always gives the same SHA, so identical files are stored only once (deduplication).
- On every read, the object is decompressed, its size is checked and its SHA-1 is recomputed (integrity check).
- **Tree entry format:** `<mode> <name>\0<20 raw SHA bytes>`, sorted by name the way Git sorts them.
- **Modes:** `100644` normal file, `100755` executable file, `40000` directory.

---

## Workflow of Each Command

### 1. `./mygit init`
- Creates `.mygit/objects/` and `.mygit/refs/heads/`.
- Writes `HEAD` containing `ref: refs/heads/main`, and creates an empty `index`.
- If the repository already exists, it prints a message and changes nothing.
- The branch file `refs/heads/main` is created only at the first commit (unborn branch).

### 2. `./mygit hash-object [-w] <file>`
- Reads the file in binary mode and builds `blob <size>\0<content>`.
- Computes and prints its SHA-1.
- With `-w`: compresses it and stores it in `objects/`. If the object already exists, it is skipped.
- Without `-w`: only prints the hash and writes nothing.

### 3. `./mygit cat-file (-p | -s | -t) <sha>`
- Finds the object from its SHA, decompresses it and verifies it.
- `-p`: prints the content. A tree is printed in `ls-tree` format, because raw tree content is binary.
- `-s`: prints the content size in bytes.
- `-t`: prints the type (`blob`, `tree` or `commit`).

### 4. `./mygit write-tree`
- Walks the current directory **recursively**, skipping `.mygit`.
- For each file, it stores a blob and records its mode.
- For each subdirectory, it first builds that directory's tree (children first).
- Sorts the entries, stores the tree object and prints the root tree's SHA.
- Empty directories and symlinks are skipped.

### 5. `./mygit ls-tree [--name-only] <tree_sha>`
- Reads the tree object and checks that it really is a tree.
- Default output: `<mode> <type> <sha>\t<name>` for each entry.
- `--name-only`: only the names, and directories end with `/`.

### 6. `./mygit add <path>...` / `./mygit add .`
- Each path is cleaned up: `./a`, absolute paths and trailing `/` are all accepted.
  - Paths outside the repository, or inside `.mygit`, are rejected.
- **File:** stores a blob and records `<mode> <sha> <path>` in the index.
- **Directory:** adds every file under it recursively.
  - Files that were staged earlier but have since been deleted are removed from the index.
- **Deleted path:** stages its removal.
- **All-or-nothing:** changes are made in memory and written once at the end.
  - If any path fails, the index stays unchanged.
  - The index is written to a temporary file and then renamed, so a crash cannot leave it half-written.

### 7. `./mygit commit [-m "message"]`
- Reads the index and rebuilds the directory structure as tree objects (deepest directories first).
- The parent is the commit that `HEAD` currently points to; the first commit has no parent.
- Refuses to commit if nothing is staged, or if the snapshot is the same as the parent's.
- Builds the commit object:
  ```
  tree <sha>
  parent <sha>                   (not present on the first commit)
  author <name> <email> <unix-time> <timezone>
  committer <name> <email> <unix-time> <timezone>

  <message>
  ```
- **Default message** when `-m` is not given: `Default commit message`.
- **Name and email:** taken from `MYGIT_AUTHOR_NAME` / `MYGIT_AUTHOR_EMAIL` if set, otherwise from the login user.
- Updates the branch file (or `HEAD` itself when detached) and prints the new commit's SHA.

### 8. `./mygit log`
- Starts at the commit `HEAD` points to and follows `parent` links back to the first commit.
- For each commit, prints its SHA, parent SHA, committer, timestamp and message (newest first).

### 9. `./mygit checkout <commit_sha | branch>`
- Accepts a full commit SHA, which gives a **detached HEAD**, or a branch name such as `main`.
- Checks that the target exists and really is a commit.
- **Safety check:** refuses if checkout would destroy unsaved work:
  - edited tracked files,
  - staged changes that are not committed,
  - untracked files that would be overwritten.
- **Restoring the snapshot:**
  1. Deletes tracked files that are not in the target commit, and removes any directories this leaves empty.
  2. Writes every file of the target commit from its blob, recreating folders and setting the executable bit.
  3. Rewrites the index to match the target, and points `HEAD` at the commit or branch.
- Untracked files that are not in the way are left untouched.

---

## Error Handling

- Every command checks its arguments and prints a usage message on misuse.
- Exit codes: 0 for success, 1 for any error.
- Clear errors for:
  - no repository found,
  - invalid or missing SHA,
  - wrong object type (for example a blob given to `ls-tree` or `checkout`),
  - corrupt objects, index, `HEAD` or branch files,
  - unreadable files and permission problems.
- Tree entries such as `..`, `.` or `.mygit` are rejected during checkout, so a crafted object cannot write outside the project folder.
- File-system calls use `std::error_code`, so failures are reported without throwing exceptions or crashing.

---

## Assumptions and Limitations

- Commands must be run from the repository root. Parent folders are not searched for `.mygit`.
- SHAs must be the full 40 lowercase hex characters. Short SHAs are not supported.
- Only one branch (`main`). There is no command to create branches.
- Empty directories and symlinks are not tracked.
- The index is a simple text file, not Git's binary format, so filenames containing a newline are not supported.
- `log` follows a single line of history: each commit has at most one parent, since there is no merging.
