void sourcepool::insert(std::string_view name) {
  const auto filename = std::format("objects/{}.lua", name);
  const auto chunk = std::format("@{}", filename);
  const auto buffer = io::read(filename);
  binding::load(L, buffer, chunk);
}
