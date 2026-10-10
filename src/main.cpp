// SPDX-License-Identifier: GPL-3.0-only

#include "app/application.h"

int main(int argc, char** argv) {
    const auto application = imagemgr::Application::create();
    return application->run(argc, argv);
}
