# Building

Install Conan, then set up profile and dependencies (once each):

```shell
mise use -g conan # or: brew install conan / pip install conan
conan profile detect --force
make conan
```

Build and run:

```shell
make run # debug
# or
make build buildtype=Release && CARTRIDGE=cartridge ./build/marecrisium # release
```
