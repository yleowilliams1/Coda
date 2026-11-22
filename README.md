# Coda

## Gild

Gild is my utils library. It's just included in file form here, it has some special compilation steps which I'll quickly list below.

Gild requires the following:
- C11 

#### Windows
- VS 2019 16.8+ for windows
- /utf-8 
- fopen/sscanf/strerror are deprecated so we compile with '_CRT_SECURE_NO_WARNINGS'
- For none MSVC/MinGW we do "_USE_MINGW_ANSI_STDIO=1" for printf(%zu)
# CLI

Coda includes a cli for prototype generation. It doesn't work on Windows as it cli.c uses posix funcitons. It's mostly written by gemini, I barely touched it so when it breaks it's not my fault. Heres the api.

## Commands

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `help` | *None* | Prints the list of available commands. |
| `set` | `FIELD VALUE` | Sets a field, stat, or skill modifier on the active prototype. |
| `write` | `NAME` | Serializes the active prototype to `NAME.proto` using `gild_io`. |
| `reset` | *None* | Resets the prototype and frees any allocated strings (`name`, `dialogue_path`). |
| `exit` / `quit` | *None* | Exits CLI mode. |
| `open` | *None* | opens existing file |
| `list` | *None* | list current |
---

## Settable Fields (`set FIELD VALUE`)

Field names and enum values are **case-insensitive**.

### General Properties

| Field | Alias | Type / Allowed Values | Example |
| :--- | :--- | :--- | :--- |
| `name` | — | String | `set name ForestGoblin` |
| `type` | — | `player`, `goblin`, `civilian` (or `*_ENTITY`) | `set type goblin` |
| `level` | `starting_level` | `u32` (`>= 0`) | `set level 5` |
| `dialogue` | `dialogue_path` | File path string | `set dialogue dialogues/goblin.dlg` |

### Base Stats (`s16`)

Accepts bare names or `ST_` prefixed names:

* `strength` (`ST_STRENGTH`)
* `agility` (`ST_AGILITY`)
* `social` (`ST_SOCIAL`)
* `constitution` (`ST_CONSTITUTION`)
* `perception` (`ST_PERCEPTION`)
* `intelligence` (`ST_INTELLIGENCE`)
* `wisdom` (`ST_WISDOM`)
* `beauty` (`ST_BEAUTY`)

```text
> set strength 12
> set ST_AGILITY 14
```

### Skill Modifiers (`s16`)

Accepts bare names or `SK_` prefixed names:

* `guns` (`SK_GUNS`)
* `unarmed` (`SK_UNARMED`)
* `dodge` (`SK_DODGE`)
* `melee` (`SK_MELEE`)
* `investigate` (`SK_INVESTIGATE`)
* `doctor` (`SK_DOCTOR`)
* `sneak` (`SK_SNEAK`)
* `thief` (`SK_THIEF`)
* `speech` (`SK_SPEECH`)
* `intimidate` (`SK_INTIMIDATE`)
* `flirt` (`SK_FLIRT`)
* `repair` (`SK_REPAIR`)
* `science` (`SK_SCIENCE`)
* `bulwark` (`SK_BULWARK`)
* `attraction` (`SK_ATTRACTION`)
* `convivial` (`SK_CONVIVIAL`)

```text
> set sneak 15
> set SK_MELEE 8
```
