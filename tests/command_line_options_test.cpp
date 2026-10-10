// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <glib.h>
#include <glibmm/variantdict.h>

#include <optional>
#include <string>
#include <vector>

#include "io/command_line_options.h"

using namespace imagemgr;

TEST_CASE("command line: named options are parsed", "[command-line]") {
    auto options_dict = Glib::VariantDict::create();
    options_dict->insert_value("directory", Glib::ustring("/start"));
    options_dict->insert_value("export-to", Glib::ustring("/exports"));
    options_dict->insert_value("quality", 80);

    const CommandLineOptions options = command_line_options_from_dict(*options_dict);

    REQUIRE(options.initial_directory == std::optional<std::string>("/start"));
    REQUIRE(options.export_directory == std::optional<std::string>("/exports"));
    REQUIRE(options.export_quality == std::optional<int>(80));
}

TEST_CASE("command line: an empty dictionary yields no overrides", "[command-line]") {
    auto options_dict = Glib::VariantDict::create();

    const CommandLineOptions options = command_line_options_from_dict(*options_dict);

    REQUIRE_FALSE(options.initial_directory.has_value());
    REQUIRE_FALSE(options.export_directory.has_value());
    REQUIRE_FALSE(options.export_quality.has_value());
}

TEST_CASE("command line: a positional argument becomes the startup directory", "[command-line]") {
    auto options_dict = Glib::VariantDict::create();
    options_dict->insert_value(G_OPTION_REMAINING,
                               std::vector<Glib::ustring>{Glib::ustring("/positional")});

    const CommandLineOptions options = command_line_options_from_dict(*options_dict);

    REQUIRE(options.initial_directory == std::optional<std::string>("/positional"));
}

TEST_CASE("command line: --directory wins over a positional argument", "[command-line]") {
    auto options_dict = Glib::VariantDict::create();
    options_dict->insert_value("directory", Glib::ustring("/explicit"));
    options_dict->insert_value(G_OPTION_REMAINING,
                               std::vector<Glib::ustring>{Glib::ustring("/positional")});

    const CommandLineOptions options = command_line_options_from_dict(*options_dict);

    REQUIRE(options.initial_directory == std::optional<std::string>("/explicit"));
}
