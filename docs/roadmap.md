# Roadmap

!!! danger "**Not implemented** — file operations"
    Copy, move, rename, delete, and create-directory actions are planned.

!!! danger "**Upcoming feature** — editor and shell integration"
    External-editor launch, shell escape, and live style editing are planned.

!!! danger "**Not implemented** — live colour cycling"
    OnScreen/2 exposed a small fixed set of colour fields through F2–F7,
    rotating the selected field through the palette for an immediate preview;
    Ctrl+S then saved it. Listless now has a broader set of style colours,
    including contextual syntax colours, so directly reusing those function
    keys would be incomplete and confusing. A future design needs a way to
    select which colour field is being edited before offering live preview
    and persistence.

!!! danger "**Upcoming feature** — wider platform support"
    Windows and macOS platform backends remain planned.

!!! danger "**Upcoming feature** — richer viewer workflow"
    Multi-buffer viewing and a fresh backward-search prompt are planned.

See the [Porting record](porting/index.md) for engineering priorities.
