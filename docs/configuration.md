# Configuration

Styles load in order: packaged system styles, a legacy single config file,
then `~/.config/listless/styles/syntax/`. Later sources override earlier
ones. The repository's `style/syntax/` directory contains examples.

Use `--syntax-dir` to replace the personal style directory for one launch:

```sh
./build/lss --syntax-dir /path/to/styles --syntax yaml document.yaml
```

The full config-field reference is in the
[porting record](porting/08-style-config.md).
