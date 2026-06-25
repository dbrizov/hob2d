#include "engine/core/engine.h"
#include "engine/core/engine_config.h"
#include "engine/core/path_utils.h"

int main(int argc, char* argv[]) {
    const hob::EngineConfig config(hob::PathUtils::get_engine_config_path());
    hob::Engine engine(config);

    engine.run();

    return 0;
}
