# Public API Reference

The public headers are the authoritative API source. Doxygen renders only
`include/meshcore` and the API landing page; private implementation headers and
test symbols are excluded.

## Generate Locally

Install Doxygen 1.9 or newer in your development environment. Graphviz is not
required. From the repository root:

```sh
cmake -S . -B build.api-docs -DMESHCORE_BUILD_DOCS=ON \
  -DMESHCORE_BUILD_TESTS=OFF -DMESHCORE_BUILD_EXAMPLES=OFF \
  -DMESHCORE_INSTALL=OFF
cmake --build build.api-docs --target meshcore_docs
```

Open `build.api-docs/docs/html/index.html` in a browser. Documentation is
generated in the selected build directory; do not commit generated HTML.
Doxygen reference/documentation warnings fail the target. Ordinary library
builds leave this option OFF and do not require Doxygen.

The documentation CI job runs this generation and the local Markdown check:

```sh
python3 tools/check_docs.py --repo-root .
```

The checker validates local Markdown targets/heading anchors and shell-block
syntax. It does not fetch external URLs or execute examples. Build/run the
affected example when changing a command and use
[testing guidance](testing.md) for public-contract evidence.
