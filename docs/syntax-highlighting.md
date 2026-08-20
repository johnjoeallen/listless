# Syntax highlighting

Listless selects styles from a file extension. Bundled styles include C/C++,
Python, shell, JavaScript/TypeScript, Go, Rust, Ruby, Java, Kotlin, C#,
Swift, SQL, Lua, PHP, Perl, Pascal/Bascal, JSON, YAML, and XML.

Use `--syntax` to choose a style explicitly (case-insensitive), including
for piped text:

```sh
./build/lss --syntax yaml example.txt
curl -s https://example.test/data.json | ./build/lss --syntax json
```

YAML, JSON, and XML use generic structural rules rather than full language
parsers. See [configuration](configuration.md) for local style files.

!!! info "Limitation"
    Highlighting intentionally is not a full language parser; uncommon,
    grammar-dependent constructs use fallback colours.
