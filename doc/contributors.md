# Contributors

## Artur Bać — author

EHT is Artur's project. He wrote all of these himself:

- the base skeleton of the tool: the `eht` library, the `journal_tailer` importer and the Qt6
  application,
- the architecture: the journal events as a `std::variant` handled by generic state machines, the
  SQLite schema generated from glaze reflection, the separate import and live state, and the split
  into personal, galaxy and live databases,
- the overlay: the Vulkan layer in the game's process and the protocol between it and the tool,
- the libraries it is built on: [simple_enum](https://github.com/arturbac/simple_enum),
  [small_vectors](https://github.com/arturbac/small_vectors) and
  [stralgo](https://github.com/arturbac/stralgo),
- all of the first months of development.

## Claude (Anthropic) — co-developer

Claude joined the project recently. Since then it has developed the next stages
together with Artur, **within the existing architecture**: Artur sets the direction, decides what
gets built and checks it in the game, and Claude writes and tests the code with him. Commits from
this collaboration carry a `Co-Authored-By: Claude` line.
