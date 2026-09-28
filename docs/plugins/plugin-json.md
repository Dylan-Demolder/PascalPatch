# plugin.json

Every plugin has a `plugin.json` next to its README. Three places read it:

- the app, to install the plugin and show its settings;
- the F2 window in game, for the plugin's tab;
- the plugin site, for its page.

`pack_plugin.py` and the app check it with the same rules (`validate_manifest` in `host/src/pascalpatch/plugin_store.py`).

```json
{
  "id": "ledge-trainer",
  "name": "Ledge Trainer",
  "version": "1.2.0",
  "abi": 1,
  "entry": "ledge-trainer.dll",
  "summary": "Times ledgedashes and shows the intangibility you kept.",
  "author": "your name",
  "license": "GPL-2.0-or-later",
  "tags": ["training"],
  "min_runtime": "0.5",
  "homepage": "https://github.com/you/ledge-trainer",
  "changes": "Counts ledge-hop aerials too.",
  "settings": [
    {"key": "enabled", "type": "bool", "label": "Coach ledgedashes", "default": true, "group": "Coaching"},
    {"key": "reset_key", "type": "key", "label": "Reset the tally", "default": "F9", "group": "Coaching"},
    {"key": "size", "type": "float", "label": "Text size", "default": 1.0, "min": 0.6, "max": 1.8, "group": "Look"}
  ]
}
```

## Fields

| Field | Required | What it is |
|---|---|---|
| `id` | yes | Lowercase, 2 to 64 characters: letters, digits, `.`, `_`, `-`, starting with a letter or digit. It never changes once published, because settings and installs are keyed by it. |
| `name` | yes | What people see. |
| `version` | yes | `major.minor.patch`. Every published build needs a new version (see [publishing](publishing.md)). |
| `abi` | yes | `1`. |
| `entry` | yes | Always `<id>.dll`. |
| `summary` | yes | One sentence, shown on cards in the app and on the site. Say what the player gets, not how the plugin works. |
| `author` | no | Your name or handle. |
| `license` | no | `GPL-2.0-or-later` for plugins on the site. |
| `tags` | no | For filtering on the site: `training`, `hud`, `menus`, `stats`, `fun`, `accessibility`, ... |
| `min_runtime` | no | The oldest PascalPatch with every host call you use, such as `"0.5"`. The site shows "Needs PascalPatch 0.5 or newer". The plugin must still check `PP_HOST_HAS` at load time. |
| `homepage` | no | Your plugin's repository or page. |
| `changes` | no | What is new in this version, in a sentence or two. It becomes the version's changelog entry on the site. |
| `settings` | no | The settings list, below. |

## Settings

Every setting has a row in the plugin's F2 tab and in the app's Plugins page. PascalPatch saves the value between runs, and the plugin reads it at any time:

```cpp
double size = H->setting_number(ID, "size");        // bool, int, float, key, and the index of a choice
const char* mode = H->setting_text(ID, "mode");     // text, and the value of a choice
```

Reads are cheap, so read them each frame instead of caching. A change made in the F2 window shows up on the next read.

| Field | For | Meaning |
|---|---|---|
| `key` | all | The name your code uses. Keep it stable: renaming it loses players' saved values. |
| `type` | all | `bool`, `int`, `float`, `choice`, `text` or `key`. |
| `label` | all | What the row says. |
| `default` | all | The starting value: `true`, `3`, `0.8`, `"fd"` or `"F5"`. |
| `min`, `max` | int, float | The slider's range. Without them it runs 0 to 1 for a float and 0 to 100 for an int. |
| `step` | int, float | Optional slider step. |
| `options` | choice | Required. Either a list of values (`["off", "short", "long"]`) or a list of `{"value": "cpu9", "label": "CPU level 9"}` objects. `setting_text` gives the value, and `setting_number` gives its index. |
| `help` | all | A tooltip. Use it for anything the label cannot say in a few words. |
| `group` | all | A heading. A new section starts wherever the group changes, so keep each group's settings together. Runtimes older than 0.5 ignore it. |

### Types

- **bool**: a checkbox. `setting_number` returns 0 or 1.
- **int** and **float**: sliders.
- **choice**: a drop-down. Use it for modes: `"dummy": ["stand", "shield", "jump"]`.
- **text**: free text, 256 characters at most.
- **key**: a hotkey. The default is a key name. In the F2 window players rebind it by pressing a key, and Esc unbinds it. `setting_number` returns the Windows virtual-key code, or 0 when it is unbound. See [hotkeys](input.md) for the key names and the keys that are already taken.

### Declaring settings from code

A plugin can also declare a setting itself, with the same JSON as one entry of the list:

```cpp
H->declare_setting(ID, R"({"key":"size","type":"float","label":"Text size","default":1.0,"min":0.6,"max":1.8})");
```

When plugin.json already declares that key, the call does nothing. Declaring every setting in both places keeps the plugin working when its DLL is loaded on its own, without its plugin.json, which is how the fast test loop in [testing](testing.md) runs it.

## The config file

When PascalPatch starts the game, it writes `<id>.json` beside each installed plugin's DLL. `pp_plugin_load` receives that file's path as `config_path`, or NULL when there is none. The file holds the settings as the app last saved them, plus PascalPatch's own bookkeeping. Most plugins never need it: `setting_number` and `setting_text` already account for it. A plugin that needs structured data the settings list cannot hold can read the file, but should ignore keys it does not know. Quick Match, for example, reads a `"match"` object that the character studio writes.

## Files in the package

A plugin package is a ZIP holding `plugin.json`, `<id>.dll`, `README.md`, and optionally `LICENSE` or `LICENSE.txt` and an `assets/` folder. The app refuses a package containing anything else. `pack_plugin.py` builds exactly this.
