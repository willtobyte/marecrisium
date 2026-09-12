int application::run() {
  auto* const options = sentry_options_new();

  {
    char buffer[2048]{};

    if (auto* const f = std::fopen("sentry.dsn", "r")) {
      std::fgets(buffer, sizeof buffer, f);
      std::fclose(f);
    }

    buffer[std::strcspn(buffer, "\r\n")] = '\0';
    sentry_options_set_dsn(options, buffer);
  }

  sentry_options_set_debug(options, 0);
  sentry_options_set_release(options, VERSION);
  sentry_options_set_database_path(options, ".sentry");
  sentry_options_set_sample_rate(options, 1.0);
  sentry_options_add_attachment(options, "cassette.tape");
  sentry_options_add_attachment(options, "stdout.txt");
  sentry_options_add_attachment(options, "stderr.txt");
  sentry_init(options);
  std::atexit(+[]{ sentry_close(); });

  if (const auto steamid = GetSteamID(); steamid) {
    char id[21]{};
    std::to_chars(id, id + sizeof id - 1, steamid);

    static constexpr char prefix[] = "https://steamcommunity.com/profiles/";
    char url[sizeof prefix + 20]{};
    std::snprintf(url, sizeof url, "%s%s", prefix, id);

    const auto user = sentry_value_new_user(id, nullptr, nullptr, nullptr);
    sentry_value_set_by_key(user, "profile_url", sentry_value_new_string(url));
    sentry_set_user(user);
  }

  try {
    io::mount("cartridge.rom");

    scriptengine se;
    se.run();
  } catch (const std::exception& exception) {
    const auto message = exception.what();

    std::fputs(message, stderr);
    std::fputc('\n', stderr);

    const auto event = sentry_value_new_event();
    const auto error = sentry_value_new_exception("exception", message);
    sentry_value_set_stacktrace(error, nullptr, 0);
    sentry_event_add_exception(event, error);
    sentry_capture_event(event);
    sentry_flush(3000);

#ifndef DEBUG
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Stampageddon", message, nullptr);
#endif

    return 1;
  }

  return 0;
}
