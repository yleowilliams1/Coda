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
# Map Editor

This is a map editor written by claude, thanks alot. It's not very interesting but I need to put it here. It does make maps for sure.
# CLI

Coda includes a cli for prototype generation. It doesn't work on Windows as it cli.c uses posix funcitons. It's mostly written by gemini, I barely touched it so when it breaks it's not my fault, leave me alone. Heres the api.

## Commands

| Command | Arguments | Description |
| ----- | ----- | ----- |
| `help` | *None* | Show available commands and usage |
| `exit` / `quit` | *None* | Exit the CLI |
| `set` | `FIELD VALUE` (or subcommand args) | Set a field, flag, inventory slot, or hotbar slot on the prototype |
| `write` | `NAME` | Write prototype to `assets/prototype/NAME.proto` |
| `open` / `read` | `NAME` | Load prototype from `NAME.proto` (`assets/prototype/` or given path) |
| `list` | *None* | Display current prototype fields, stats, skills, and non-empty slots |
| `reset` | *None* | Reset the prototype to defaults |

## Settable Fields & Subcommands

Field names, flag names, item names, and enum values are **case-insensitive**.

You can set collision by doing set collision x y w h and set dialogue with set dialogue path
### General Properties (`set FIELD VALUE`)

| Field | Alias | Type / Allowed Values | Example |
| ----- | ----- | ----- | ----- |
| `name` | — | String | `set name ForestGoblin` |
| `type` | — | `player`, `goblin`, `civilian` (or `*_ENTITY`) | `set type goblin` |
| `level` | `starting_level` | `u32` (`>= 0`) | `set level 5` |
| `dialogue` | `dialogue_path` | File path string | `set dialogue dialogues/goblin.dlg` |

### Flags (`set flag` / `set flags`)

* `set flag <FLAG> [1|0|on|off|true|false]` — Enable or disable a flag (defaults to enabling if omitted)
* `set flag none` — Clear all flags (`ENTITY_FLAG_NONE`)

Supported flags:
* `dead` (`ENTITY_FLAG_DEAD`)
* `static` (`ENTITY_FLAG_STATIC`)
* `no_combat` (`ENTITY_FLAG_NO_COMBAT`)
* `hostile` (`ENTITY_FLAG_HOSTILE`)
* `dialogue` (`ENTITY_FLAG_DIALOGUE`)

```text
> set flag hostile
> set flag ENTITY_FLAG_HOSTILE 0
> set flag none
```

### Inventory & Hotbar (`set inventory` / `set inv` / `set hotbar`)

* `set inventory <SLOT 0..31> <ITEM|none> [COUNT]` — Set or clear an inventory slot (`COUNT` defaults to `1`)
* `set hotbar <SLOT 0..7> <ITEM|none> [COUNT]` — Set or clear a hotbar slot (`COUNT` defaults to `1`)

Supported items (`ITEM_LIST`):
* `apple` (`ITEM_APPLE`)
* `dagger` (`ITEM_DAGGER`)
* `butchers_knife` (`ITEM_BUTCHERS_KNIFE`)
* `leather_jacket` (`ITEM_LEATHER_JACKET`)
* `power_armor` (`ITEM_POWER_ARMOR`)

```text
> set inventory 0 dagger 2
> set hotbar 1 ITEM_APPLE 5
> set hotbar 1 none
```

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

