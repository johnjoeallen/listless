# Configuration

Styles load in order: packaged system styles, then
`~/.config/listless/styles/syntax/`. Later sources override earlier ones. The
repository's `style/syntax/` directory contains examples.

The legacy `~/.config/listless/style.conf` file is no longer read. To migrate
an existing configuration, move its `Style` blocks into one or more `.conf`
files in `~/.config/listless/styles/syntax/`.

Use `--syntax-dir` to replace the personal style directory for one launch:

```sh
./build/lss --syntax-dir /path/to/styles --syntax yaml document.yaml
```

The full config-field reference is in the
[porting record](porting/08-style-config.md).
