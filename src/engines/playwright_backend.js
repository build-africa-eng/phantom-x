// playwright_backend.js
// This script runs in a Node.js environment and interacts with Playwright.

const playwright = require('playwright');
let browser;
let page; // The main/default page
let pages = new Map(); // Stores active pages, keyed by a unique ID (e.g., Playwright page object)
let exposedObjects = new Map(); // Stores QObjects exposed from C++, keyed by their JS name

process.stdin.setEncoding('utf8');

// Function to send messages back to the C++ process
function sendMessage(type, command, data = {}) {
    const message = { type, command, data };
    const jsonString = JSON.stringify(message);
    const messageWithLength = `${jsonString.length}\n${jsonString}`;
    process.stdout.write(messageWithLength);
}

// Function to send synchronous responses back to the C++ process
function sendSyncResponse(id, result) {
    const response = { type: "sync_response", id: id, result: result };
    const jsonString = JSON.stringify(response);
    const messageWithLength = `${jsonString.length}\n${jsonString}`;
    process.stdout.write(messageWithLength);
}

// IPC Protocol: Read length-prefixed JSON messages from stdin
let buffer = "";
process.stdin.on('data', (chunk) => {
    buffer += chunk;
    while (true) {
        const newlineIndex = buffer.indexOf('\n');
        if (newlineIndex === -1) {
            break; // No full length prefix yet
        }

        const lengthStr = buffer.substring(0, newlineIndex);
        const messageLength = parseInt(lengthStr, 10);

        if (isNaN(messageLength) || messageLength < 0) {
            console.error('PLAYWRIGHT_BACKEND_JS: Invalid message length received. Clearing buffer.');
            buffer = ""; // Clear corrupted buffer
            break;
        }

        if (buffer.length < newlineIndex + 1 + messageLength) {
            break; // Not enough data for the full message yet
        }

        const jsonMessage = buffer.substring(newlineIndex + 1, newlineIndex + 1 + messageLength);
        buffer = buffer.substring(newlineIndex + 1 + messageLength); // Remove processed message

        try {
            const parsedMessage = JSON.parse(jsonMessage);
            handleCommand(parsedMessage);
        } catch (e) {
            console.error('PLAYWRIGHT_BACKEND_JS: JSON parse error:', e.message, 'Message:', jsonMessage);
        }
    }
});

// Command handler
async function handleCommand(message) {
    const { type, command, id, params } = message;

    // console.log(`PLAYWRIGHT_BACKEND_JS: Received ${type} command: ${command} (ID: ${id || 'N/A'})`);

    let result;
    let error;

    try {
        switch (command) {
            case "init":
                // Launch browser, create default page
                browser = await playwright.chromium.launch({ headless: true }); // headless: true for production
                page = await browser.newPage();
                pages.set('default', page); // Store the default page
                console.log('PLAYWRIGHT_BACKEND_JS: Browser and default page initialized.');

                // Attach common page event listeners
                setupPageEventListeners(page, 'default');

                sendMessage('event', 'initialized'); // Inform C++ that page is ready
                break;

            case "shutdown":
                if (browser) {
                    await browser.close();
                    browser = null;
                    page = null;
                    pages.clear();
                    exposedObjects.clear();
                    console.log('PLAYWRIGHT_BACKEND_JS: Browser closed.');
                }
                process.exit(0);
                break;

            // --- Core Page Navigation and Loading ---
            case "load":
                // params: { url, operation, body, headers }
                if (page) {
                    try {
                        sendMessage('event', 'loadStarted', { url: params.url });
                        const response = await page.goto(params.url, { waitUntil: 'domcontentloaded' }); // Wait for DOM to be ready
                        sendMessage('event', 'loadFinished', { success: !!response, url: params.url });
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error during load:', e.message);
                        sendMessage('event', 'loadFinished', { success: false, url: params.url, error: e.message });
                        error = e.message;
                    }
                } else {
                    error = "Page not initialized.";
                }
                break;

            case "setHtml":
                // params: { html, baseUrl }
                if (page) {
                    try {
                        await page.setContent(params.html, { baseURL: params.baseUrl });
                        sendMessage('event', 'loadFinished', { success: true, url: page.url() }); // Simulate load finished
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error during setHtml:', e.message);
                        error = e.message;
                    }
                } else {
                    error = "Page not initialized.";
                }
                break;

            case "reload":
                if (page) await page.reload({ waitUntil: 'load' });
                break;

            case "stop":
                if (page) await page.close(); // Closest equivalent is closing the page in Playwright
                break;

            case "canGoBack":
            case "goBack":
            case "canGoForward":
            case "goForward":
            case "goToHistoryItem":
                console.warn(`PLAYWRIGHT_BACKEND_JS: History navigation command '${command}' is not directly supported by Playwright's default page object. Requires custom history management.`);
                // Playwright doesn't expose a direct history API like QtWebKit.
                // This would need a custom implementation in the Node.js backend to track history
                // or use a more advanced Playwright feature like context.storageState()
                // For now, return false/error
                result = false;
                error = "History navigation not fully implemented.";
                break;

            // --- Page Content Properties ---
            case "getHtml":
                if (page) result = await page.content();
                break;
            case "getTitle":
                if (page) result = await page.title();
                break;
            case "getUrl":
                if (page) result = page.url();
                break;
            case "getPlainText":
                if (page) result = await page.textContent('body'); // Gets text content of body element
                break;

            // --- JavaScript Execution & Interaction ---
            case "evaluateJs":
                // params: { script }
                if (page) {
                    result = await page.evaluate(params.script);
                }
                break;

            case "injectJsFile":
                // params: { filePath, encoding, libraryPath, inPhantomScope }
                // For Playwright, injecting a file typically means reading its content and evaluating it.
                // Or, if it's a URL, adding a script tag.
                // Assuming filePath is a local path to be read and injected.
                if (page) {
                    const fs = require('fs/promises'); // Node.js file system
                    try {
                        const scriptContent = await fs.readFile(params.filePath, params.encoding || 'utf8');
                        result = await page.evaluate(scriptContent);
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Failed to inject JS file:', e.message);
                        error = e.message;
                        result = false;
                    }
                } else {
                    error = "Page not initialized.";
                    result = false;
                }
                break;

            case "appendScriptElement":
                // params: { scriptUrl }
                if (page) {
                    await page.addScriptTag({ url: params.scriptUrl });
                }
                break;

            case "exposeObject":
                // params: { objectName, methods[], properties{} }
                if (page) {
                    const objectName = params.objectName;
                    const methodsToExpose = params.methods || [];
                    const propertiesToExpose = params.properties || {};

                    // Create a proxy object in the browser's window that maps to Playwright's exposeFunction
                    const jsExposureScript = `(() => {
                        window.${objectName} = {}; // Create the global object
                        window.${objectName}._exposedMethods = new Set(${JSON.stringify(methodsToExpose)});
                        window.${objectName}._exposedProperties = new Set(Object.keys(${JSON.stringify(propertiesToExpose)}));

                        // For each method, expose a function that calls back to C++
                        for (const methodName of window.${objectName}._exposedMethods) {
                            window.${objectName}[methodName] = async (...args) => {
                                // Call a globally exposed Playwright function (like `callCPlusPlusMethod`)
                                // that Playwright then routes to our Node.js backend.
                                // We'll expose `_callCPlusPlus` on the Node.js side using page.exposeFunction
                                return await window._callCPlusPlus('${objectName}', methodName, args);
                            };
                        }

                        // Property getters/setters (simplified, assumes Playwright's default property access is handled)
                        // For direct property access like phantom.cookiesEnabled = true
                        // This requires more complex reflection or exposing specific get/set functions per property
                        // For now, these are not directly handled unless specific getter/setter methods are exposed
                        // via the `methodsToExpose` list (e.g., `setCookiesEnabled`, `getCookiesEnabled`).
                        // The Q_PROPERTY system implicitly maps to these in C++.
                    })();`;

                    // Expose a function that the browser's JS can call, which then calls back to Node.js
                    await page.exposeFunction('_callCPlusPlus', async (objName, methodName, args) => {
                        // This function is called from the browser's JS context.
                        // We need to send this back to the C++ side as a synchronous command.
                        console.log(`PLAYWRIGHT_BACKEND_JS: JS called C++ method: ${objName}.${methodName}(${JSON.stringify(args)})`);
                        const syncCommandResult = await new Promise((resolve) => {
                            // Generate a unique ID for this specific callback
                            const callbackId = `${Date.now()}-${Math.random()}`;
                            exposedObjects.set(callbackId, { resolve, objName, methodName }); // Store promise resolver

                            // Send a synchronous command to C++
                            // C++ side will need a handler for 'callExposedQObjectMethod'
                            // This would then map to the actual QObject method.
                            sendSyncResponse(callbackId, {
                                type: "callback_to_cpp",
                                objName: objName,
                                methodName: methodName,
                                args: args
                            });
                        });
                        return syncCommandResult; // Return result from C++ back to browser JS
                    });

                    await page.addInitScript({ content: jsExposureScript });
                    console.log(`PLAYWRIGHT_BACKEND_JS: Exposed object '${objectName}' to browser JS.`);
                } else {
                    error = "Page not initialized.";
                }
                break;

            // --- More IEngineBackend implementations will go here ---

            default:
                console.warn(`PLAYWRIGHT_BACKEND_JS: Unhandled command: ${command}`);
                error = `Unknown command: ${command}`;
        }
    } catch (e) {
        console.error(`PLAYWRIGHT_BACKEND_JS: Error executing command ${command}:`, e.message);
        error = e.message;
    }

    if (type === "sync_command") {
        sendSyncResponse(id, { success: !error, result: result, error: error });
    }
}

// Attach basic Playwright page event listeners and relay them to C++
function setupPageEventListeners(p, pageId) {
    p.on('console', msg => {
        sendMessage('event', 'javaScriptConsoleMessage', { message: msg.text() });
    });

    p.on('pageerror', error => {
        // Playwright pageerror does not provide sourceID or lineNumber directly for all errors
        sendMessage('event', 'javaScriptError', {
            message: error.message,
            lineNumber: error.stack ? error.stack.split('\n')[1] : 0, // Crude line number from stack
            sourceID: page.url(),
            stack: error.stack
        });
    });

    p.on('request', request => {
        sendMessage('event', 'resourceRequested', {
            url: request.url(),
            method: request.method(),
            headers: request.headers(),
            id: request.url() // Use URL as temp ID for simplicity
        });
    });

    p.on('response', async response => {
        sendMessage('event', 'resourceReceived', {
            url: response.url(),
            status: response.status(),
            statusText: response.statusText(),
            headers: response.headers(),
            id: response.url() // Use URL as temp ID
        });
    });

    p.on('requestfailed', request => {
        sendMessage('event', 'resourceError', {
            url: request.url(),
            errorText: request.failure().errorText,
            id: request.url()
        });
    });

    p.on('load', () => {
        sendMessage('event', 'loadFinished', { success: true, url: p.url() }); // Re-emit on load
        sendMessage('event', 'initialized'); // When DOM is ready and initial scripts might be run
    });

    p.on('domcontentloaded', () => {
        // DOM content is loaded, might be a good time to inject initial scripts like bootstrap.js
        // However, we still need 'page.evaluate' which needs 'initialized'
    });

    p.on('url', url => {
        sendMessage('event', 'urlChanged', { url: url });
    });

    p.on('title', title => {
        sendMessage('event', 'titleChanged', { title: title });
    });

    p.on('framenavigated', async frame => {
        // This is a more granular event than full page load
        // Can be used to update frameUrl, frameTitle, etc.
        sendMessage('event', 'frameNavigated', {
            url: frame.url(),
            name: frame.name(),
            isMainFrame: frame.isMainFrame(),
        });
    });

    // Handle dialogs (alert, confirm, prompt, beforeunload)
    p.on('dialog', async dialog => {
        console.log(`PLAYWRIGHT_BACKEND_JS: Dialog type: ${dialog.type()}, message: ${dialog.message()}`);
        if (dialog.type() === 'alert') {
            sendMessage('event', 'javaScriptAlertSent', { message: dialog.message() });
            await dialog.accept();
        } else if (dialog.type() === 'confirm') {
            // Need to send to C++ and wait for result.
            // This requires a synchronous call from JS backend to C++ process
            // and the C++ process calling the Callback object to get user input.
            // The `sendSyncResponse` in the JS side is for C++ calling JS.
            // For JS calling C++ synchronously, it's more complex with current IPC.
            // For now, auto-accept or auto-dismiss for confirm/prompt.
            sendMessage('event', 'javaScriptConfirmRequested', { message: dialog.message() });
            await dialog.accept(); // Or dialog.dismiss();
        } else if (dialog.type() === 'prompt') {
            sendMessage('event', 'javaScriptPromptRequested', { message: dialog.message(), defaultValue: dialog.defaultValue() });
            await dialog.accept(dialog.defaultValue()); // Or pass a value from C++
        } else if (dialog.type() === 'beforeunload') {
            sendMessage('event', 'javascriptInterruptRequested'); // Similar to beforeunload
            await dialog.accept();
        } else {
            await dialog.dismiss(); // Dismiss unknown dialogs
        }
    });

    // File choosers
    p.on('filechooser', async fileChooser => {
        console.log('PLAYWRIGHT_BACKEND_JS: File chooser opened.');
        // This is tricky: needs to send to C++, wait for C++ to get file path from user, then send back.
        // For now, auto-accept with no files.
        sendMessage('event', 'filePickerRequested', { oldFile: '' }); // oldFile is typically the value attribute
        await fileChooser.setFiles([]); // No files selected for now
    });
}
