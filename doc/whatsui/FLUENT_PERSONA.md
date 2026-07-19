# Fluent Persona

`Persona` composes a Fluent Avatar (or a presence-only state) with an optional
four-level text identity. It follows Fluent React v9's size mapping:

| Persona size | Avatar size |
| --- | --- |
| `ExtraSmall` | 20 DIP |
| `Small` | 28 DIP |
| `Medium` | 32 DIP |
| `Large` | 36 DIP |
| `ExtraLarge` | 40 DIP |
| `Huge` | 56 DIP |

## Usage

```cpp
auto person = std::make_unique<wui::Persona>("Ada Lovelace", wui::PersonaSize::ExtraLarge);
person->setAvatarColor(wui::AvatarColor::Brand);
person->setPresence(wui::PresenceStatus::Available);
person->setSecondaryText("Task owner");
```

`primaryText` defaults to `name`. `secondaryText`, `tertiaryText`, and
`quaternaryText` are optional. Text is single-line per level and safely
ellipsized against the assigned bounds, so a dense people list cannot paint
through adjacent columns.

Typography follows Fluent's hierarchy: the primary line is `body1` (upgraded
to `subtitle2` for `ExtraLarge` and `Huge`); optional lines use `caption1`
(`body1` for `Huge`). Media spacing follows the Fluent size ramp (6/8/10/12
DIP) rather than applying an arbitrary fixed gap.

`textPosition` is semantic (`After`, `Before`, or `Below`) rather than a hard
coded left/right string. The current core has no global RTL layout direction;
applications can select `Before`/`After` at their locale layout boundary until
that shared direction facility exists.

## Presence and interaction

Presence is optional and overlays the Avatar using `PresenceBadge` geometry.
`presenceOnly(true)` shows just the status indicator, useful for compact
availability lists. A presence-only Persona without a configured status is
empty by design.

Persona is passive by default: it consumes no pointer input and exposes a
named `Group`. Supplying `onClick(...)` deliberately opts into focus,
hover/pressed feedback, keyboard activation (Enter/Space), and `Button`
semantics. This prevents decorative identity content from becoming an
accidental button.

## Accessibility

The generated root name includes the effective primary text, all non-empty
detail lines, and current presence. `accessibleLabel` overrides that generated
name. The identity child remains an `Image` (Avatar) and the presence child
remains an informative status semantic, while the Persona root represents the
combined group or interactive button.

## Verification

- `WhatsUIFluentPersonaTests`: sizing, four lines, presence-only behavior,
  passive/interactive policy, and semantic snapshots.
- `WhatsUIFluentPersonaVisualTests`: Software captures at 100% and 150% DPI,
  covering status overlays, before/below positions, text truncation, and
  presence-only rendering.
