# Banking Admin Module — SFML (real graphical window)

This is the SFML rewrite of the admin console — a real OS window instead
of a terminal UI. `Models.h`, `FileManager.h/.cpp`, and `Bank.h/.cpp` are
byte-for-byte the same files from the FTXUI version: only the UI layer
changed. If you already have both projects, you now have a working example
of swapping frontends without touching business logic or storage.

## What had to be built that FTXUI gave for free

SFML is a low-level 2D drawing/window library — it has no buttons, text
inputs, lists, or layout system. `src/Widgets.h/.cpp` is a small hand-rolled
toolkit that implements what was needed:

- **`Label`** — static text
- **`Button`** — rectangle + centered text + hover highlight + click hit-test
- **`TextBox`** — single-line editable field: focus on click, blinking
  cursor, backspace, optional digits-only mode (used for PIN/amount/account
  number fields), optional character masking
- **`ListBox`** — scrollable, click-to-select row list with a clip view
  (mouse wheel scroll, highlight selected row)
- **`drawCard` / `drawPanelBackground`** — the dashboard summary tiles and
  bordered panels

Everything is hand-positioned with fixed pixel coordinates (`setPosition`/
`setSize`) since there's no automatic layout system like FTXUI's `vbox`/
`hbox`/`flex`.

## Features implemented (identical to the FTXUI version)

- Create account (auto account number, initial deposit, 4-digit PIN)
- Search / view / update customer records
- Activate / Deactivate / Lock an account
- View all accounts + full transaction/audit log (filterable by account #)
- Reset PIN with CNIC identity verification
- Delete a closed (Inactive) account + summary report

Same file-based storage under `./data` (`accounts.dat`, `transactions.dat`,
`counter.dat`) — see the FTXUI README for the on-disk format, it's unchanged.

## Building

Requires SFML 2.5+ dev libraries and a C++17 compiler.

```bash
# Debian/Ubuntu
sudo apt-get install libsfml-dev

mkdir build && cd build
cmake ..
make -j4
./banking_admin_sfml
```

The build copies `assets/DejaVuSans.ttf` and `DejaVuSans-Bold.ttf` next to
the executable automatically (CMake `POST_BUILD` step), since SFML needs an
actual font file — it can't use system fonts the way a terminal does.

## Using it

- Click a sidebar item to switch screens.
- Click into a text field to focus it, type normally, click elsewhere or
  press Enter/Tab to unfocus.
- **Accounts** screen: search (or leave blank + Search to list everyone),
  click a row in the list to select it, click **Load Selected Into Editor**
  to populate the update fields and enable status/deposit/withdraw actions.
- **Delete & Report**: account must be **Inactive** before it can be
  deleted — deactivate it first on the Accounts screen.
- `Esc` or closing the window quits.

## Honest limitations of this version vs. a production GUI

This is a minimal widget set built for this one screen set, not a general
GUI framework:

- No keyboard navigation between fields (Tab just unfocuses rather than
  moving to the next field) — everything is mouse-driven
- No text selection, copy/paste, or cursor movement within a field (only
  backspace-from-the-end editing)
- No window resizing support — layout is fixed at 1150×720
- No scrollbar visual on `ListBox`, just mouse-wheel scrolling

None of these affect the required admin functionality, but if this needs to
feel like a polished commercial app, budget more time for the widget layer
itself, not just the screens on top of it.
