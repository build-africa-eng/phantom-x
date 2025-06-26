/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "main.h"

#include "phantom.h"
#include "config.h" // Include config for potential settings access
#include "terminal.h" // Include terminal for console output

#include <QCoreApplication>
#include <QCommandLineParser> // Use QCommandLineParser if it's the standard Qt one
#include <QDebug>
#include <QDir> // For path manipulation
#include <QFile> // For checking script existence
#include <QTime> // For measuring execution time

// REMOVE THIS LINE: #include <QWebSettings> // No longer used after WebKit removal

// extern const struct QCommandLineConfigEntry flags[]; // Declared in config.cpp, might not be needed here if only main parses it

// Forward declarations if any (e.g., for custom message handlers)

// main function (entry point)
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // Set application info for QSettings (used by Config)
    QCoreApplication::setOrganizationName("PhantomX");
    QCoreApplication::setApplicationName("PhantomJS");
    QCoreApplication::setApplicationVersion("3.0.0"); // Update version as appropriate

    // Initialize global Config singleton
    Config* config = Config::instance();
    // Initialize global Terminal singleton (used for all console output)
    Terminal* terminal = Terminal::instance();

    // Command-line parsing
    // QCommandLineParser is typically used for standard Qt applications
    // If you are using your custom QCommandLine class:
    QCommandLine cmdLineParser(&app); // Using your custom QCommandLine class

    // Set the configuration entries (the 'flags' array)
    // You need to ensure 'flags' is accessible here.
    // If 'flags' is global in config.cpp, you might need an extern declaration in a common header
    // or pass it from main to QCommandLine.
    // Assuming 'flags' is accessible, or passed in main somehow, or defined here.
    // For simplicity, let's assume `Config` or `Phantom` handles the main parsing setup.
    // If QCommandLineConfigEntry flags[] is truly external, it needs to be made accessible here.
    // If it's used by Phantom for parsing, then main.cpp just calls Phantom.

    // A more typical flow with `QCommandLine` (your custom one):
    // You would pass `flags` to `QCommandLine::setConfig` somewhere, often in Phantom's constructor
    // or when setting up the command-line parsing logic.
    // For now, let's rely on Phantom to handle this setup.

    // Check for help or version request if your QCommandLine has that logic built-in
    // If parse fails or help/version requested, QCommandLine should print and exit.
    // QCommandLine should be configured with `flags` array somewhere.

    // Create the PhantomJS engine instance
    Phantom phantom(&app);

    // If parsing command line args is done by phantom.init, call it.
    // Otherwise, parse here and pass to phantom.
    // Assuming phantom.init() handles command line parsing and config loading.
    bool initOk = phantom.init(argc, argv); // Pass argc, argv for command line parsing

    if (!initOk) {
        // If init fails (e.g., parse error, unrecognized options), exit.
        return 1; // Indicate error
    }

    // After parsing, check for help/version if it wasn't handled by phantom.init
    if (phantom.helpRequested()) {
        phantom.showHelp(); // This should exit the app
        return 0;
    }
    if (phantom.versionRequested()) {
        phantom.showVersion(); // This should exit the app
        return 0;
    }

    // Execute the main script (if provided)
    QString scriptPath = phantom.scriptPath();
    if (!scriptPath.isEmpty()) {
        // Run the script
        qDebug() << "Main: Running script:" << scriptPath;
        int exitCode = phantom.executeScript(scriptPath, phantom.scriptArgs());
        return exitCode;
    } else if (phantom.isInteractive()) {
        // Start interactive REPL
        qDebug() << "Main: Starting interactive REPL.";
        phantom.startInteractive(); // This is typically blocking until user exits
        return 0;
    } else {
        // If no script and not interactive, maybe print help or an error
        terminal->cerr("No script provided and not in interactive mode. Use --help for usage.");
        return 1;
    }

    return 0; // Should not be reached in normal operation
}
