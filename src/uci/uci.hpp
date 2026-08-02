#pragma once

namespace ChessEngine {

// Start the Universal Chess Interface (UCI) protocol communication loop.
// Listens to commands from standard input and responds to standard output.
void uci_loop();

} // namespace ChessEngine
