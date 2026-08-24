# c-shell — a small Unix-style shell in C

This is my OSN (Operating Systems and Networks) mini-project at IIIT Hyderabad. I built a small Unix-like shell in C to understand what actually happens between typing a command and seeing its output.

I wanted to avoid hiding the interesting parts behind libraries, so the shell is mostly built from the basic Unix system calls and C functions: `getline`, `fork`, `execv`, `pipe`, `poll`, `dup2`, `lseek`, and the usual filesystem calls. I didn't use things like `readline` or `popen`.

The project is currently just the userland shell. The `xv6/` directory is there for a separate part of the work and isn't involved in the current shell.

---

## How I approached the problem

I found it easiest to think of a shell as a sequence of steps rather than one big program.

```text
input
  ↓
lexer
  ↓
parser
  ↓
router
  ↓
┌───────────────┬────────────────┐
│               │                │
builtin      external       pipeline
```

Every time I enter a command, `main.c`:

1. prints the prompt
2. reads a line using `getline`
3. sends it to the lexer
4. checks the resulting tokens with the parser
5. sends the command to the appropriate handler
6. frees the tokens and starts again

Keeping these stages separate made the project much easier to work on. For example, when something was going wrong with a command, I could first check whether the problem was in tokenisation, parsing, or execution instead of debugging everything at once.

---

## 1. The lexer

The first thing I needed was a way to turn the line typed by the user into something the rest of the shell could understand.

I wrote my own lexer instead of using a normal string-splitting function because spaces alone aren't enough to describe shell input.

For example:

```text
echo "hello world"
```

should be treated as `echo` followed by one argument, not three separate words.

The lexer recognises the operators:

```text
|  &  ;  <  >
```

and also `>>`.

Everything else is treated as part of a word and passed through `parse_word`.

I also handled the basic quoting rules:

* single quotes keep everything literal
* double quotes preserve spaces but still handle `\` and `"`
* outside quotes, `\` escapes the next character

So all of these result in one argument containing a space:

```text
echo "hello world"
echo 'hello world'
echo hello\ world
```

The lexer is basically a character-by-character loop. This was a little more work than using an existing tokenizer, but it made the behaviour much easier to control.

---

## 2. The parser

Once I had tokens, I needed to make sure they actually formed a valid command.

I kept this separate from the lexer. The lexer only answers:

> "What are the tokens?"

The parser answers:

> "Do these tokens make sense together?"

I used a small state machine:

```text
LINE → ARG → TGT / CMD / BG
```

This lets the parser distinguish normal arguments, command boundaries, redirections, pipelines, and background markers.

For example, malformed redirections or incomplete commands can be rejected here instead of making every command implementation deal with them.

This also means that the actual execution code can assume that the basic structure of the command is already valid.

---

## 3. Routing the command

After parsing, the router looks at the first word of the command.

If it is one of my built-ins:

```text
hop
reveal
peek
locate
```

it calls the corresponding function.

Otherwise it is treated as an external command and goes to `arbitrary_handler`.

From there, the shell decides whether it is dealing with a normal command or a pipeline.

So the overall flow is roughly:

```text
               input
                 ↓
               lexer
                 ↓
               parser
                 ↓
               router
              /      \
             /        \
        builtin      external
                       |
                 single/pipeline
```

---

# Built-in commands

## `hop`

`hop` is basically my version of `cd`, but with a few extra features.

It supports:

```text
hop ~
hop .
hop ..
hop -
hop name
```

The reason `hop` has to be a built-in is important: changing directory has to change the directory of the shell itself. If I ran `cd`-like behaviour inside a child process, only the child would move and the shell would stay in the old directory.

For normal paths I first try the path literally. If that doesn't work, I look for the directory using a small frecency table.

### Frecency

The table is stored in:

```text
~/.c_shell_frecency
```

I used the score:

```text
visits / (age_in_seconds + 1.0)
```

The idea is that a directory I visited recently should usually be more useful than one I visited many times a long time ago.

For example, if I type:

```text
hop projects
```

and there isn't a directory called `projects` in the current location, the shell can search the frecency table and jump to the best matching directory.

I also made sure that entries pointing to directories that no longer exist don't stay around forever.

The path handling itself is in `path_utils.c`, so commands that need to understand `~`, `.`, `..`, or `-` can use the same code.

---

## `reveal`

`reveal` is the directory-listing command.

It lists entries alphabetically and puts `/` after directory names so they are easy to distinguish.

The main options are:

```text
reveal
reveal somedir
reveal -a
reveal -t
reveal -at
```

`-a` shows hidden files and `-t` recursively enters subdirectories.

I also allow the flags to be combined, so `-at` and `-ta` work.

---

## `peek`

`peek` started as a simple file-printing command, but I added reverse reading because it was an interesting problem to solve.

It supports:

```text
peek file
peek file1 file2
peek -
peek -n file
peek -r file
peek -rn file
```

With no file, it reads from standard input.

### Reading forwards

The normal case is straightforward: read the file and write it to standard output.

### Reading backwards

The interesting case is `peek -r`.

For a regular file, I don't read the whole file from the beginning just to reverse it. Instead, I use `lseek` to start near the end and read backwards in 4 KiB chunks.

But this doesn't work for pipes or stdin because they aren't seekable. In that case `lseek` gives `ESPIPE`, so I switch to a different approach: store the input lines in memory and print them in reverse.

So there are two paths:

```text
regular file
    ↓
lseek from the end
    ↓
read backwards

stdin / pipe
    ↓
store lines
    ↓
print backwards
```

The `-n` option adds line numbers, counting only non-empty lines.

---

## `locate`

`locate` is similar to `which`, except that I print every executable match instead of stopping at the first one.

For:

```text
locate gcc
```

I check the current directory first and then search the directories in `$PATH`.

I also handle empty entries in `$PATH` as the current directory.

The same lookup logic is reused when I need to find an external command to execute. This avoids having two separate implementations for essentially the same problem.

---

# Running external commands

Anything that isn't one of the four built-ins is treated as an external command.

There are a few cases.

If the command contains `/`, I try to execute that path directly.

If it starts with `%`, for example:

```text
%gcc
```

I skip the current-directory lookup and search only `$PATH`.

Otherwise I first try:

```text
./command
```

and then search `$PATH`.

Once I find the executable, the actual process creation is handled using:

```text
fork()
execv()
```

The shell stays in the parent process while the command runs in the child.

---

# Redirection

For redirection I used file descriptors rather than trying to copy data around manually.

The shell supports:

```text
<
>
>>
```

For example:

```text
command < input.txt
command > output.txt
command >> output.txt
```

The basic idea is:

1. open the required file
2. get its file descriptor
3. use `dup2()` to connect it to stdin/stdout
4. run the command

I kept this logic in `redirection.c` so that the same code can be used by normal commands as well as pipeline stages.

---

# Pipelines

Pipelines were probably the part that took the most effort.

For something like:

```text
cat big.txt | grep foo | wc -l
```

each command needs to run as its own process, and the output of one process has to become the input of the next.

I create a pipe between each pair of commands and then fork the processes.

Conceptually:

```text
cat
 |
pipe
 |
grep
 |
pipe
 |
wc
```

Each child uses `dup2()` to connect its stdin/stdout to the appropriate pipe.

One thing that took some debugging was making sure the unused ends of the pipes are closed at the right time. If a process keeps a write end open when it shouldn't, another process can keep waiting because it never sees EOF.

---

# Pipelines + redirection

The more interesting case is when pipelines and output redirection are used together.

For example:

```text
cat big.txt | grep foo > out.txt
```

The final stage has to write to the file instead of simply writing to the terminal.

I also support cases where output from a pipeline stage needs to be captured by the parent.

For this I use an `OutputCapture` structure and keep track of the relevant pipe ends in the parent.

The parent uses:

```text
poll()
```

to monitor multiple pipe outputs instead of blocking on one pipe at a time.

This was probably the trickiest part of the project because several child processes can be running and producing output at the same time.

The basic idea is:

```text
pipeline processes
       ↓
     pipes
       ↓
     poll()
       ↓
parent handles the available output
       ↓
files / stdout
```

---

# How the files are organised

I tried to keep each part of the shell in its own file instead of putting everything into `main.c`.

```text
c-shell/
├── Makefile
├── include/
│   ├── shell.h
│   ├── prompt.h
│   ├── input.h
│   ├── lexer.h
│   ├── parser.h
│   ├── router.h
│   ├── hop.h
│   ├── reveal.h
│   ├── peek.h
│   ├── locate.h
│   ├── path_utils.h
│   ├── command.h
│   ├── pipeline.h
│   ├── redirection.h
│   └── arbitrary.h
│
└── src/
    ├── main.c
    ├── prompt.c
    ├── input.c
    ├── lexer.c
    ├── parser.c
    ├── router.c
    ├── shell.c
    ├── hop.c
    ├── peek.c
    ├── reveal.c
    ├── locate.c
    ├── path_utils.c
    │
    └── arbitrary/
        ├── arbitrary.c
        ├── command.c
        ├── redirection.c
        └── pipeline.c
```

The main idea is:

| File            | What it handles                 |
| --------------- | ------------------------------- |
| `main.c`        | Main REPL loop                  |
| `input.c`       | Reading input                   |
| `lexer.c`       | Tokenising the input            |
| `parser.c`      | Checking command syntax         |
| `router.c`      | Deciding which handler to call  |
| `shell.c`       | Shell state                     |
| `prompt.c`      | Prompt                          |
| `hop.c`         | Directory navigation + frecency |
| `reveal.c`      | Directory listing               |
| `peek.c`        | File reading                    |
| `locate.c`      | Finding executables             |
| `path_utils.c`  | Path handling                   |
| `command.c`     | Running one external command    |
| `redirection.c` | `<`, `>`, `>>`                  |
| `pipeline.c`    | Pipes and multiple commands     |
| `arbitrary.c`   | Handling external commands      |

This separation was mainly for my own sanity. It also made it possible to work on one part without constantly changing unrelated code.

---

# Building

The project needs a reasonably modern `gcc` or `clang` and a POSIX environment.

I developed and tested it on macOS 14.

From the project root:

```sh
cd c-shell
make
```

Then:

```sh
./shell.out
```

To clean the generated files:

```sh
make clean
```

The Makefile uses:

```text
-Wall -Wextra -Werror
-std=c2x
-D_POSIX_C_SOURCE=200809L
-fno-asm
```

so the code is compiled with warnings treated as errors.

---

# Things I didn't get to

There are a few things I left incomplete because they didn't fit into the time available.

### Job control

`&` is recognised by the parser, but proper asynchronous background execution isn't implemented yet. There is no `SIGCHLD` handling or `jobs` command.

### History

There is no command history.

### Ctrl-C

I haven't added custom signal handling for `Ctrl-C`, so the normal terminal behaviour is used.

### Globbing

Things like:

```text
*.c
?
```

aren't expanded. Paths have to be given explicitly.

---

# A few things I learned from the project

The part I found most useful was seeing how the different Unix concepts fit together.

A command that looks simple from the outside actually involves several different pieces:

```text
typed command
     ↓
tokenisation
     ↓
syntax checking
     ↓
process creation
     ↓
file descriptor setup
     ↓
exec
```

Pipelines made this even clearer because suddenly there are multiple processes, multiple file descriptors, and multiple pipes that all have to be coordinated.

I also ended up reusing several pieces of code instead of implementing the same idea twice. For example, path resolution is shared between commands, and the executable lookup used by `locate` is also used when running external commands.

The main thing I was trying to get out of this project was not to make a complete replacement for Bash. It was to understand what a shell is actually doing underneath the commands we normally take for granted. That was the reason I chose to implement the parsing, process creation, pipes, redirection, and file handling myself.
