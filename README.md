# Mare Crisium

[![macOS](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml)
[![Windows](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml)

```text
- From where comes all this disbelief? It looks like a revolt... Against who?
- You have too much to learn, Antônio. I can't have disbelief if I never had belief...
To believe in what? In a symbol? In an absent force created by ignorance?
Yes, I'm a rebel... Against the fools like you!
```

## Build and Run

Install Conan (choose one method)

```shell
mise use -g conan # recommended
# brew install conan
# pip install conan
```

Initialize Conan profile (only once)

```shell
conan profile detect --force
```

Install dependencies, (also only once)

```shell
make conan
```

Build and run.

```shell
make run
```
