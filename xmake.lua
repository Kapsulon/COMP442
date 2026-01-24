add_rules("mode.debug", "mode.release")

set_languages("c++23")

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

if is_mode("release") then
    set_optimize("fastest")
end

set_config("build.compdb", true)
add_rules("plugin.compile_commands.autoupdate")

add_requires(
    "spdlog",
    "ctre"
)

target("lexdriver")
    set_kind("binary")
    add_packages("spdlog", "ctre")
    add_includedirs("src")
    add_files("src/*/**.cpp")
    add_files("src/lexdriver.cpp")
