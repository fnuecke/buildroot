# Console keymaps

`rootfs-overlay/usr/share/keymaps/*.bmap` are binary keymaps for busybox `loadkmap` (`CONFIG_LOADKMAP=y`).
They exist for cases where input is sent as keycodes (e.g. the keyboard block in oc2).

Generated from the kbd project's keymaps with `loadkeys -b <source>`:

| File | Source keymap | File | Source keymap |
|------|---------------|------|---------------|
| us   | us            | nl   | nl            |
| uk   | uk            | be   | be-latin1     |
| de   | de            | pl   | pl            |
| fr   | fr            | jp   | jp106         |
| it   | it            | hu   | hu            |
| es   | es            | dvorak | dvorak      |
| pt   | pt-latin1     | no   | no            |
| br   | br-abnt2      | dk   | dk            |
| se   | sv-latin1     | fi   | fi            |

kbd is GPL-2.0+, and its keymaps carry individual author notices. These are derived works, so they
ship under the same terms as the rest of the GPL userland in this image.

Latin script only for now since we'd have to also ship console fonts otherwise, which are typically
larger than what I'd want to stick on the default image. Copy in manually as needed.

To regenerate, run `loadkeys -b <source> > <file>.bmap` with the kbd package installed.
