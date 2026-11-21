# The `.conf` format

cppfig reads and writes flat `path = value` files. This page is the reference for what the parser accepts, how values are written back, and what each error means.

```conf
# Server settings
server.host = localhost
server.port = 8080          # comments may follow a value
app.greeting = "Hello, #1"  # quote a value to keep '#', quotes or edge spaces
```

## Lines

The file is processed one line at a time. Lines end with `\n`, and a trailing `\r` is treated as whitespace, so CRLF files work. Whitespace means spaces, tabs and `\r`.

| Line | Meaning |
| --- | --- |
| Empty or only whitespace | Ignored |
| First non-whitespace character is `#` | Comment, ignored |
| Anything else | An entry: `path = value` |

## Entries

```
[whitespace] path [whitespace] = [whitespace] value [whitespace] [# comment]
```

The first `=` on the line separates the path from the value, so a value may itself contain `=`.

### Paths

- Case-sensitive: `Server.Port` and `server.port` are different settings.
- One or more segments separated by `.`; no empty segments, so no leading, trailing or doubled dots.
- May not contain whitespace, control characters, `#`, `"` or `\`.
- Must match a setting's `path` in the registry. What happens to other paths is chosen by the caller: see [Unknown keys](#unknown-keys).
- Each path may appear at most once.

The registry applies the same rules to declared paths at compile time, and also rejects a path that is a prefix of another (`server` next to `server.port`).

### Unquoted values

An unquoted value runs from the first non-whitespace character after `=` up to the first `#` or the end of the line, with trailing whitespace removed. It may not contain `"`. An empty value is allowed and is passed to the setting's conversion as an empty string.

```conf
server.host = example.com   # value is "example.com"
server.tag  = a#b           # value is "a": '#' starts a comment
```

### Quoted values

A value that starts with `"` runs to the matching closing `"`. Inside the quotes `#` is ordinary text and these escapes are recognized:

| Escape | Character |
| --- | --- |
| `\\` | backslash |
| `\"` | double quote |
| `\n` | line feed |
| `\r` | carriage return |
| `\t` | tab |

Any other escape is an error. Only whitespace and an optional `# comment` may follow the closing quote. Quoted values cannot span lines; use `\n` instead.

## Values and types

The parser produces text only. Each setting's `ConfigTraits<T>::FromString` converts the text and must accept all of it:

- Integers use `std::from_chars`, so `42x`, `+1` and out-of-range values are rejected.
- `bool` accepts `true`/`false`, `yes`/`no`, `on`/`off` and `1`/`0`, all lowercase.
- `std::string` takes the text as is.

Validators then run on every value that came from the file.

## Writing

The file is never reformatted. Saving writes the original text back unchanged, so comments, order, spacing and preserved unknown keys survive. When settings are missing from the file, their defaults are appended once, under a `# Added by cppfig` line:

```conf
# existing content, untouched
server.port = 9000

# Added by cppfig
server.host = localhost
```

A value is written quoted, using the escapes above, when it is empty, starts or ends with a space or tab, or contains `=`, `#`, `"`, `\`, a line feed, a carriage return or a tab. Otherwise it is written as is. Environment overrides are never written.

## Unknown keys

`LoadOptions::unknown_keys` decides what happens to an entry whose path is not in the registry:

| Policy | Behavior |
| --- | --- |
| `UnknownKeyPolicy::kReject` (default) | Loading fails, which catches typos. |
| `UnknownKeyPolicy::kPreserve` | The entry stays in the file and is listed by `ConfDocument::GetUnknownKeys()` with its line, so it can be logged. Use this when an older binary may read a file written by a newer one, or after removing a setting. |

Entries with unknown paths must still be syntactically valid. Duplicate known paths are an error under both policies.

## Errors

Loading reports every error it finds, one per line, rather than stopping at the first. Errors that point into the file start with `file:line:column:`, as compiler diagnostics do. Columns count bytes from 1.

```
app.conf:2:1: missing '=' after the setting path
app.conf:4:3: duplicate setting 'server.port' (first set on line 2)
app.conf:5:15: cannot parse 'abc' as a value of 'server.port'
app.conf:7:15: invalid value for 'server.port': Value 0 is outside [1, 65535]
app.conf:9:12: unterminated quoted value
environment variable SERVER_PORT: cannot parse the value for 'server.port'
```

Errors in defaults and environment overrides name the setting instead, since they have no position in the file; those from the environment never quote the variable's value, which may be a secret. After 20 errors the rest are only counted, so a file of garbage cannot produce an unbounded message.
