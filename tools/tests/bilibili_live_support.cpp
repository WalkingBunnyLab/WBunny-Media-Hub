// Standalone service probe: no UI/catalog or log writer is initialized.
// The live target uses the production HTTP client, not mocked transport.
#include "core/i18n.hpp"
#include "logger/logger.hpp"

namespace i18n {
const char* tr(const char* english) { return english; }
}

void log_message(LogLevel, const char*, const char*, ...) {}
