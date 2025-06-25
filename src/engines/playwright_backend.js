// playwright_backend.js
// This script runs in a Node.js environment and interacts with Playwright.

const playwright = require('playwright');
const fs = require('fs/promises'); // Node.js file system for injectJsFile

let browser;
let browserContext; // Use a browser context for better isolation and settings management
let page; // The main/default page for convenience
let pages = new Map(); // Stores active pages, keyed by Playwright Page object reference (or unique ID if managing multiple contexts)
let exposedObjects = new Map(); // Stores QObjects metadata exposed from C++, keyed by their JS name
let syncResponseResolvers = new Map(); // Stores resolvers for synchronous IPC calls from JS back to C++

process.stdin.setEncoding('utf8');

// Function to send messages back to the C++ process
function sendMessage(type, command, data = {}, id = null) {
    const message = { type, command, data };
    if (id !== null) {
        message.id = id;
    }
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
            if (parsedMessage.type === "sync_response_from_cpp_callback") {
                // This is a response from C++ for a synchronous callback initiated by JS
                const resolver = syncResponseResolvers.get(parsedMessage.id);
                if (resolver) {
                    syncResponseResolvers.delete(parsedMessage.id);
                    resolver(parsedMessage.result);
                } else {
                    console.warn(`PLAYWRIGHT_BACKEND_JS: Received callback result for unknown ID: ${parsedMessage.id}`);
                }
            } else {
                // Regular command from C++ to JS
                handleCommand(parsedMessage);
            }
        } catch (e) {
            console.error('PLAYWRIGHT_BACKEND_JS: JSON parse error:', e.message, 'Message:', jsonMessage);
        }
    }
});

// Generic function to send a synchronous request to C++ and wait for a response
async function callCPlusPlusSynchronously(commandName, data = {}) {
    return new Promise((resolve) => {
        const id = `${Date.now()}-${Math.random()}`; // Unique ID for this request
        syncResponseResolvers.set(id, resolve); // Store resolver for this ID
        sendMessage('sync_command_to_cpp', commandName, data, id);
    });
}


// Command handler
async function handleCommand(message) {
    const { type, command, id, params } = message;

    // console.log(`PLAYWRIGHT_BACKEND_JS: Received ${type} command: ${command} (ID: ${id || 'N/A'})`);

    let result;
    let error = null;

    try {
        switch (command) {
            case "init":
                // Launch browser, create default page
                browser = await playwright.chromium.launch({ headless: true }); // headless: true for production
                browserContext = await browser.newContext(); // Use a context for settings
                page = await browserContext.newPage();
                pages.set('default', page); // Store the default page
                console.log('PLAYWRIGHT_BACKEND_JS: Browser and default page initialized.');

                // Attach common page event listeners
                setupPageEventListeners(page, 'default'); // For default page

                sendMessage('event', 'initialized'); // Inform C++ that page is ready
                break;

            case "shutdown":
                if (browser) {
                    await browser.close();
                    browser = null;
                    browserContext = null;
                    page = null;
                    pages.clear();
                    exposedObjects.clear();
                    console.log('PLAYWRIGHT_BACKEND_JS: Browser closed.');
                }
                process.exit(0);
                break;

            case "closePage": // Command to close a specific page
                // params: { pageId: 'default' or some other ID }
                // For now, only 'default' page is explicitly handled.
                if (params.pageId === 'default' && page) {
                    await page.close();
                    pages.delete('default');
                    page = null; // Clear main page
                } else {
                    // Logic for closing other pages if they were tracked by ID
                }
                break;

            // --- Core Page Navigation and Loading ---
            case "load":
                // params: { url, operation, body, headers }
                if (page) {
                    try {
                        // Playwright doesn't have direct 'operation' like Get/Post for page.goto
                        // For POST, page.goto is usually for forms. For direct POST, use page.request.post()
                        // For headers, set them globally on context or per request.
                        const headers = params.headers || {};
                        await page.setExtraHTTPHeaders(headers); // Set custom headers for this navigation

                        if (params.operation === 1 || params.operation === 2) { // GET or HEAD
                            const response = await page.goto(params.url, { waitUntil: 'domcontentloaded', timeout: params.timeout || 30000 });
                            result = !!response;
                        } else if (params.operation === 3) { // POST
                            // Playwright's page.goto doesn't handle POST bodies directly.
                            // Needs to use page.request for full control.
                            // For simplicity, let's just do a GET for now or require content.
                            // A proper POST would involve:
                            // const response = await page.request.post(params.url, { data: Buffer.from(params.body, 'base64') });
                            // For now, let's just navigate to the URL and log a warning for POST body.
                            console.warn(`PLAYWRIGHT_BACKEND_JS: POST operation with body is not fully implemented via page.goto. URL: ${params.url}`);
                            const response = await page.goto(params.url, { waitUntil: 'domcontentloaded', timeout: params.timeout || 30000 });
                            result = !!response;
                        } else {
                            console.warn(`PLAYWRIGHT_BACKEND_JS: Unsupported network operation: ${params.operation}. Falling back to GET.`);
                            const response = await page.goto(params.url, { waitUntil: 'domcontentloaded', timeout: params.timeout || 30000 });
                            result = !!response;
                        }
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error during load:', e.message);
                        error = e.message;
                        result = false;
                    }
                } else {
                    error = "Page not initialized.";
                    result = false;
                }
                break;

            case "setHtml":
                // params: { html, baseUrl }
                if (page) {
                    try {
                        await page.setContent(params.html, { baseURL: params.baseUrl, waitUntil: 'domcontentloaded' });
                        result = true;
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error during setHtml:', e.message);
                        error = e.message;
                        result = false;
                    }
                } else {
                    error = "Page not initialized.";
                    result = false;
                }
                break;

            case "reload":
                if (page) {
                    await page.reload({ waitUntil: 'load' });
                    result = true;
                }
                break;

            case "stop":
                if (page) {
                    // Playwright doesn't have a direct 'stop' method.
                    // The closest is often to abort requests or close the page/context.
                    // For now, we'll try to stop any pending navigation.
                    // A more robust solution might involve aborting specific network requests.
                    // Playwright's `page.waitForNavigation` can be cancelled by `page.close()`, but this is too drastic.
                    console.warn("PLAYWRIGHT_BACKEND_JS: 'stop' command is a no-op for Playwright currently. Consider implementing request interception to abort.");
                    result = true; // Assume success for now
                }
                break;

            case "canGoBack":
            case "goBack":
            case "canGoForward":
            case "goForward":
            case "goToHistoryItem":
                console.warn(`PLAYWRIGHT_BACKEND_JS: History navigation command '${command}' is not directly supported by Playwright's page object. Returning false.`);
                result = false; // Playwright doesn't expose a direct history API like QtWebKit.
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
            case "getWindowName":
                if (page) result = await page.evaluate(() => window.name);
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
                if (page) {
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

            case "sendEvent":
                // params: { type, arg1, arg2, mouseButton, modifierArg }
                if (page) {
                    const eventType = params.type;
                    switch (eventType) {
                        case "click":
                        case "dblclick":
                        case "mousedown":
                        case "mouseup":
                        case "mousemove":
                            // arg1, arg2 are x, y coordinates
                            await page.mouse[eventType](params.arg1, params.arg2, { button: params.mouseButton || 'left' });
                            break;
                        case "keydown":
                        case "keyup":
                        case "keypress":
                            // arg1 is key, arg2 is text (optional)
                            // Playwright typically uses page.keyboard.press or page.keyboard.type
                            // Need to map Qt key codes to Playwright keys or use direct character.
                            if (params.arg2) { // If text provided, assume direct typing
                                await page.keyboard.type(params.arg2);
                            } else if (params.arg1) { // Else, try to press the key code
                                // This requires mapping Qt virtual key codes to Playwright string keys
                                console.warn(`PLAYWRIGHT_BACKEND_JS: sendEvent keydown/up/press with Qt key codes (arg1) not fully mapped. Arg1: ${params.arg1}`);
                                await page.keyboard.press(String.fromCharCode(params.arg1)); // Basic attempt
                            }
                            break;
                        default:
                            console.warn(`PLAYWRIGHT_BACKEND_JS: Unsupported event type for sendEvent: ${eventType}`);
                            break;
                    }
                    result = true;
                } else {
                    error = "Page not initialized.";
                    result = false;
                }
                break;

            case "uploadFile":
                // params: { selector, fileNames[] }
                if (page) {
                    await page.setInputFiles(params.selector, params.fileNames);
                    result = true;
                }
                break;

            case "applySettings":
                // params: { settingsMap }
                if (browserContext) {
                    const settings = params.settingsMap;

                    if (settings.userAgent !== undefined) {
                        await browserContext.setUserAgent(settings.userAgent);
                    }
                    if (settings.customHeaders !== undefined) {
                        // Playwright's setExtraHTTPHeaders is for *future* requests on the page.
                        // For context-wide headers, it's set during context creation.
                        // If it's a page setting, apply to current page:
                        if (page) await page.setExtraHTTPHeaders(settings.customHeaders);
                    }
                    if (settings.viewportSize !== undefined && page) {
                        await page.setViewportSize(settings.viewportSize);
                    }
                    if (settings.ignoreSslErrors !== undefined) {
                        // This is a browser launch option, or context option for newContext.
                        // Can't change on existing context/page dynamically for this.
                        console.warn(`PLAYWRIGHT_BACKEND_JS: 'ignoreSslErrors' cannot be changed dynamically on existing context. Apply during browser/context creation.`);
                    }
                    if (settings.offlineStoragePath !== undefined) {
                        console.warn(`PLAYWRIGHT_BACKEND_JS: 'offlineStoragePath' not directly configurable post-context creation.`);
                    }
                    if (settings.localStoragePath !== undefined) {
                        console.warn(`PLAYWRIGHT_BACKEND_JS: 'localStoragePath' not directly configurable post-context creation.`);
                    }
                    // Disk cache settings are typically browser launch args.
                    // Playwright contexts can clear cache with browserContext.clearCookies/clearPermissions().
                    // Max disk cache size is not directly exposed.

                    // Resource timeout can be set per page or per operation.
                    if (settings.resourceTimeout !== undefined && page) {
                        page.setDefaultTimeout(settings.resourceTimeout);
                        page.setDefaultNavigationTimeout(settings.resourceTimeout);
                    }
                    if (settings.zoomFactor !== undefined && page) {
                        // Playwright page doesn't have a direct zoomFactor. You'd evaluate JS to change CSS zoom.
                        await page.evaluate(z => document.body.style.zoom = z, settings.zoomFactor);
                    }

                    // SSL protocols/ciphers/certs are mostly browser launch options.
                    console.warn("PLAYWRIGHT_BACKEND_JS: SSL, disk cache, and some storage settings are typically set at browser/context launch and are not dynamically changeable.");
                    result = true;
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            case "getUserAgent":
                if (page) result = await page.evaluate(() => navigator.userAgent);
                break;
            case "setUserAgent":
                if (browserContext) await browserContext.setUserAgent(params.userAgent);
                else if (page) await page.setUserAgent(params.userAgent); // Fallback for old Playwright or just current page
                break;

            case "getViewportSize":
                if (page) result = await page.viewportSize();
                break;
            case "setViewportSize":
                if (page) await page.setViewportSize({ width: params.width, height: params.height });
                break;

            case "getClipRect":
                // Playwright screenshot options handle clipRect directly. No persistent clipRect on page.
                result = { x: 0, y: 0, width: 0, height: 0 }; // Placeholder
                break;
            case "setClipRect":
                // This is a rendering setting, not a persistent page property in Playwright.
                // We'll pass it to renderImage/renderPdf.
                console.warn("PLAYWRIGHT_BACKEND_JS: setClipRect is a rendering-specific parameter, not a persistent page property.");
                break;

            case "getScrollPosition":
                if (page) result = await page.evaluate(() => ({ x: window.scrollX, y: window.scrollY }));
                break;
            case "setScrollPosition":
                if (page) await page.evaluate(pos => window.scrollTo(pos.x, pos.y), params);
                break;

            case "setNavigationLocked":
                // Playwright doesn't have a direct 'navigation locked' flag.
                // This would involve request interception to block navigations.
                console.warn("PLAYWRIGHT_BACKEND_JS: setNavigationLocked not implemented. Requires request interception.");
                result = true;
                break;
            case "navigationLocked":
                result = false; // Not implemented
                break;

            case "getCustomHeaders":
                if (page) result = page.request.headers(); // Headers of last request, not globally set
                break;
            case "setCustomHeaders":
                if (page) await page.setExtraHTTPHeaders(params.headers);
                break;

            case "getZoomFactor":
                if (page) result = await page.evaluate(() => parseFloat(document.body.style.zoom) || 1.0);
                break;
            case "setZoomFactor":
                if (page) await page.evaluate(z => document.body.style.zoom = z, params.zoom);
                break;

            case "getOfflineStoragePath":
            case "getOfflineStorageQuota":
            case "getLocalStoragePath":
            case "getLocalStorageQuota":
                console.warn(`PLAYWRIGHT_BACKEND_JS: Storage path/quota commands '${command}' are not directly exposed by Playwright.`);
                result = (command.includes("Quota")) ? -1 : "";
                break;


            case "renderImage":
                // params: { clipRect, onlyViewport, scrollPosition }
                if (page) {
                    const screenshotOptions = {};
                    if (params.clipRect && (params.clipRect.width > 0 || params.clipRect.height > 0)) {
                        screenshotOptions.clip = params.clipRect;
                    }
                    screenshotOptions.fullPage = !params.onlyViewport;
                    screenshotOptions.encoding = 'base64'; // Always base64 for IPC

                    // Apply scroll position before screenshot if not full page
                    if (page && params.scrollPosition && !screenshotOptions.fullPage) {
                        await page.evaluate(pos => window.scrollTo(pos.x, pos.y), params.scrollPosition);
                    }

                    try {
                        result = await page.screenshot(screenshotOptions);
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error taking screenshot:', e.message);
                        error = e.message;
                        result = "";
                    }
                } else {
                    error = "Page not initialized.";
                    result = "";
                }
                break;

            case "renderPdf":
                // params: { paperSize, clipRect }
                if (page) {
                    const pdfOptions = {
                        printBackground: true, // Typically needed for accurate renders
                    };

                    // Map paperSize options
                    if (params.paperSize) {
                        if (params.paperSize.format) pdfOptions.format = params.paperSize.format;
                        if (params.paperSize.width) pdfOptions.width = params.paperSize.width; // e.g., "8.5in"
                        if (params.paperSize.height) pdfOptions.height = params.paperSize.height; // e.g., "11in"
                        // Margins
                        const margins = {};
                        if (params.paperSize.margin && typeof params.paperSize.margin === 'object') {
                            if (params.paperSize.margin.top) margins.top = params.paperSize.margin.top;
                            if (params.paperSize.margin.bottom) margins.bottom = params.paperSize.margin.bottom;
                            if (params.paperSize.margin.left) margins.left = params.paperSize.margin.left;
                            if (params.paperSize.margin.right) margins.right = params.paperSize.margin.right;
                            if (Object.keys(margins).length > 0) pdfOptions.margin = margins;
                        }
                        if (params.paperSize.orientation === 'landscape') pdfOptions.landscape = true;
                        // Header/Footer templates (more complex, might need JS evaluation)
                        // Playwright uses displayHeaderFooter with template for this.
                        if (params.paperSize.header) {
                            console.warn("PLAYWRIGHT_BACKEND_JS: PDF headers not fully implemented.");
                        }
                        if (params.paperSize.footer) {
                            console.warn("PLAYWRIGHT_BACKEND_JS: PDF footers not fully implemented.");
                        }
                    }

                    try {
                        const pdfBuffer = await page.pdf(pdfOptions);
                        result = pdfBuffer.toString('base64');
                    } catch (e) {
                        console.error('PLAYWRIGHT_BACKEND_JS: Error generating PDF:', e.message);
                        error = e.message;
                        result = "";
                    }
                } else {
                    error = "Page not initialized.";
                    result = "";
                }
                break;


            // --- Cookie Management ---
            case "getCookies":
                if (browserContext) {
                    const cookies = await browserContext.cookies();
                    result = cookies.map(c => ({
                        name: c.name,
                        value: c.value,
                        domain: c.domain,
                        path: c.path,
                        expires: c.expires ? new Date(c.expires * 1000).toGMTString() : undefined, // Convert timestamp to GMT string
                        httponly: c.httpOnly,
                        secure: c.secure,
                        // session: c.session // Playwright has this, PhantomJS might not
                    }));
                } else {
                    error = "Browser context not initialized.";
                }
                break;

            case "setCookies":
                if (browserContext) {
                    const cookies = params.cookies.map(c => ({
                        name: c.name,
                        value: c.value,
                        domain: c.domain,
                        path: c.path,
                        // expires: c.expires ? Math.floor(new Date(c.expires).getTime() / 1000) : -1, // Convert GMT string to timestamp
                        // Playwright expects expires as seconds since epoch or Date object.
                        // If expires is a string like "GMT format", parse it.
                        expires: c.expires ? (new Date(c.expires).getTime() / 1000) : undefined, // Playwright handles undefined for session
                        httpOnly: c.httponly,
                        secure: c.secure
                    })).filter(c => c.name && c.value && c.domain); // Filter out invalid cookies

                    await browserContext.addCookies(cookies);
                    result = true;
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            case "addCookie":
                if (browserContext) {
                    const c = params.cookie;
                    const cookie = {
                        name: c.name,
                        value: c.value,
                        domain: c.domain,
                        path: c.path,
                        expires: c.expires ? (new Date(c.expires).getTime() / 1000) : undefined,
                        httpOnly: c.httponly,
                        secure: c.secure
                    };
                    if (cookie.name && cookie.value && cookie.domain) { // Basic validation
                        await browserContext.addCookies([cookie]);
                        result = true;
                    } else {
                        result = false;
                        error = "Invalid cookie format for addCookie.";
                    }
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            case "deleteCookie":
                if (browserContext) {
                    // Playwright doesn't have a direct deleteCookie by name.
                    // Need to get all, filter, then set the rest. Or clear all and re-add.
                    // The easiest way is to set an expired cookie.
                    const cookieName = params.cookieName;
                    await browserContext.addCookies([{
                        name: cookieName,
                        value: 'deleted',
                        domain: page.url().split('/')[2], // Use current page's domain as a guess
                        path: '/',
                        expires: 0 // Set to expire immediately
                    }]);
                    result = true; // Assume success
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            case "clearCookies":
                if (browserContext) {
                    await browserContext.clearCookies();
                    result = true;
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            // --- Network / Proxy ---
            case "setNetworkProxy":
                // Playwright proxy settings are typically on browser launch or newContext.
                // Re-creating browserContext is not ideal for dynamic changes.
                // For now, log a warning. A full solution would restart context or use request interception.
                console.warn("PLAYWRIGHT_BACKEND_JS: Dynamic proxy change not fully supported. Apply proxy at browser/context launch.");
                result = true; // Assume success for now
                break;

            case "clearMemoryCache":
                if (browserContext) {
                    // Playwright context.clearCookies clears cache-related storage
                    // and context.clearPermissions() might also affect.
                    await browserContext.clearCookies(); // Clears HTTP cache, session cookies, etc.
                    await browserContext.clearPermissions();
                    result = true;
                } else {
                    error = "Browser context not initialized.";
                    result = false;
                }
                break;

            case "setIgnoreSslErrors":
                // This is a launch option, not dynamically changeable.
                console.warn("PLAYWRIGHT_BACKEND_JS: 'setIgnoreSslErrors' is a browser launch option, not dynamically changeable.");
                result = true;
                break;

            case "setSslProtocol":
            case "setSslCiphers":
            case "setSslCertificatesPath":
            case "setSslClientCertificateFile":
            case "setSslClientKeyFile":
            case "setSslClientKeyPassphrase":
                console.warn(`PLAYWRIGHT_BACKEND_JS: SSL configuration '${command}' is a browser launch option, not dynamically changeable.`);
                result = true;
                break;

            case "setResourceTimeout":
                if (page) {
                    page.setDefaultTimeout(params.timeoutMs);
                    page.setDefaultNavigationTimeout(params.timeoutMs);
                    result = true;
                }
                break;

            case "setMaxAuthAttempts":
                console.warn("PLAYWRIGHT_BACKEND_JS: setMaxAuthAttempts not directly supported by Playwright.");
                result = true;
                break;


            // --- Frame Management ---
            // Playwright's `page.frames()` returns all frames, and `frame.childFrames()` for children.
            // A "current frame" is usually set implicitly when interacting with page/frame elements.
            // For these commands, we will operate on `page.mainFrame()` or explicitly provided frame references.
            case "getFramesCount":
                if (page) result = page.frames().length;
                break;
            case "getFramesName":
                if (page) result = page.frames().map(f => f.name()).filter(Boolean); // Filter empty names
                break;
            case "getFrameName":
                // For the currently active frame (main frame by default)
                if (page) result = page.mainFrame().name();
                break;
            case "getFocusedFrameName":
                // Playwright doesn't have a direct 'focused frame'.
                // This would need custom tracking (e.g., via page.keyboard.press focusing elements in a frame)
                console.warn("PLAYWRIGHT_BACKEND_JS: getFocusedFrameName not implemented. Returning empty string.");
                result = "";
                break;

            case "switchToFrameByName":
                // params: { name }
                if (page) {
                    const targetFrame = page.frames().find(f => f.name() === params.name);
                    if (targetFrame) {
                        // For Playwright, switching frame means making subsequent operations target that frame.
                        // We need to store which frame is "current" for subsequent evaluateJs etc.
                        // This implies state management in the backend. For now, assume page.mainFrame() is the default.
                        page = targetFrame; // !!! This is a simplification: subsequent calls to `page` will now operate on `targetFrame`. This breaks the "main page" concept.
                                           // A better way is to pass a `frameId` from C++ and use `pages.get(frameId)`.
                        console.warn("PLAYWRIGHT_BACKEND_JS: switchToFrameByName currently makes the target frame the new 'page' object. This might not be desired for all operations.");
                        result = true;
                    } else {
                        result = false;
                        error = `Frame with name '${params.name}' not found.`;
                    }
                } else { result = false; error = "Page not initialized."; }
                break;

            case "switchToFrameByPosition":
                // params: { position }
                if (page) {
                    const frames = page.frames();
                    if (params.position >= 0 && params.position < frames.length) {
                        page = frames[params.position]; // Simplification, see above
                        result = true;
                    } else {
                        result = false;
                        error = `Frame at position ${params.position} not found.`;
                    }
                } else { result = false; error = "Page not initialized."; }
                break;

            case "switchToMainFrame":
                if (browserContext) {
                    page = pages.get('default'); // Restore default page
                    result = true;
                } else { result = false; error = "Browser context not initialized."; }
                break;

            case "switchToParentFrame":
                if (page && page.parentFrame()) {
                    page = page.parentFrame(); // Switch to parent
                    result = true;
                } else { result = false; error = "No parent frame to switch to."; }
                break;

            case "switchToFocusedFrame":
                // Not directly supported, see getFocusedFrameName.
                console.warn("PLAYWRIGHT_BACKEND_JS: switchToFocusedFrame not implemented.");
                result = false;
                break;


            // --- JavaScript-to-C++ Callbacks (Exposing QObject for JS bridge) ---
            case "exposeObject":
                // params: { objectName, methods[], properties{} }
                if (page) {
                    const objectName = params.objectName;
                    const methodsToExpose = params.methods || [];
                    const propertiesToExpose = params.properties || {};

                    // Store exposed object metadata for potential later use (e.g., property access)
                    exposedObjects.set(objectName, { methods: new Set(methodsToExpose), properties: new Set(Object.keys(propertiesToExpose)) });

                    // Expose a general function that browser JS can call to invoke C++ methods
                    await page.exposeFunction(`_callCPlusPlus_${objectName}`, async (methodName, args) => {
                        console.log(`PLAYWRIGHT_BACKEND_JS: JS called C++ method via exposed function: ${objectName}.${methodName}(${JSON.stringify(args)})`);
                        const response = await callCPlusPlusSynchronously('callExposedQObjectMethod', {
                            objectName: objectName,
                            methodName: methodName,
                            args: args
                        });
                        return response.result; // Return the result from C++ back to browser JS
                    });

                    // Inject a script that creates the global object in the browser
                    // and maps its methods to the exposed Playwright function.
                    const jsExposureScript = `(() => {
                        window.${objectName} = {};
                        const methods = ${JSON.stringify(Array.from(methodsToExpose))};
                        for (const methodName of methods) {
                            window.${objectName}[methodName] = async (...args) => {
                                // Calls the Playwright exposed function
                                return await window._callCPlusPlus_${objectName}(methodName, args);
                            };
                        }
                        // Property getters/setters: for properties, we'd ideally expose getters/setters too.
                        // For now, if JS calls a method that is actually a C++ property setter/getter,
                        // it will be routed through the method call mechanism.
                        // For example, if 'phantom.cookiesEnabled = true;' is called, Playwright will
                        // look for a 'setCookiesEnabled' method on the exposed object.
                        // Q_PROPERTY conversion handles this implicitly in C++.
                        console.log('PLAYWRIGHT_BACKEND_JS: Injected JS for exposed object: ${objectName}');
                    })();`;

                    await page.addInitScript({ content: jsExposureScript });

                    console.log(`PLAYWRIGHT_BACKEND_JS: Exposed object '${objectName}' to browser JS.`);
                    result = true;
                } else {
                    error = "Page not initialized.";
                    result = false;
                }
                break;

            case "clickElement":
                // params: { selector }
                if (page) {
                    await page.click(params.selector);
                    result = true;
                } else { error = "Page not initialized."; result = false; }
                break;

            case "showInspector":
                // Playwright does not have an embedded inspector in the same way QtWebKit did.
                // This command would typically launch Playwright in non-headless mode for visual debugging.
                // The `remotePort` param is not directly used by Playwright's built-in inspector.
                console.log("PLAYWRIGHT_BACKEND_JS: 'showInspector' command received. Launching Playwright in non-headless mode for visual inspection if not already in headless mode.");
                // To actually open DevTools, you need to launch non-headless browser and then `page.evaluate(() => { debugger; })`
                // or just launch with `headless: false`.
                // For now, this is just a log.
                result = 0; // Indicate no specific port opened for external connection
                break;

            default:
                console.warn(`PLAYWRIGHT_BACKEND_JS: Unknown command: ${command}`);
                error = `Unknown command: ${command}`;
        }
    } catch (e) {
        console.error(`PLAYWRIGHT_BACKEND_JS: Error executing command ${command}:`, e.message, e.stack);
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
            lineNumber: error.stack ? (error.stack.split('\n')[1] ? error.stack.split('\n')[1].match(/:(\d+):\d+\)?$/)?.[1] || 0 : 0) : 0, // Crude line number from stack
            sourceID: p.url(),
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
            errorText: request.failure() ? request.failure().errorText : 'Unknown error',
            id: request.url()
        });
    });

    p.on('load', () => {
        sendMessage('event', 'loadFinished', { success: true, url: p.url() }); // Re-emit on load
    });

    p.on('domcontentloaded', () => {
        // DOM content is loaded, might be a good time to inject initial scripts like bootstrap.js
        // The `initialized` signal is typically for when the main JS environment is ready,
        // often after exposed objects are set up.
        // It's already triggered by the "init" command in `handleCommand` once.
    });

    p.on('url', url => {
        sendMessage('event', 'urlChanged', { url: url });
    });

    p.on('title', title => {
        sendMessage('event', 'titleChanged', { title: title });
    });

    p.on('framenavigated', async frame => {
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
            const confirmResult = await callCPlusPlusSynchronously('javaScriptConfirmRequested', { message: dialog.message() });
            if (confirmResult.result) {
                await dialog.accept();
            } else {
                await dialog.dismiss();
            }
        } else if (dialog.type() === 'prompt') {
            const promptResponse = await callCPlusPlusSynchronously('javaScriptPromptRequested', { message: dialog.message(), defaultValue: dialog.defaultValue() });
            if (promptResponse.accepted) {
                await dialog.accept(promptResponse.result);
            } else {
                await dialog.dismiss();
            }
        } else if (dialog.type() === 'beforeunload') {
            // PhantomJS's javascriptInterrupt is similar to this.
            // It asks if the JS execution should be interrupted or continue.
            const interruptResult = await callCPlusPlusSynchronously('javascriptInterruptRequested');
            if (interruptResult.result) {
                // If C++ decides to interrupt, you might block navigation or close page.
                // For beforeunload, Playwright's default is to wait for accept/dismiss.
                await dialog.dismiss(); // Simulate interruption by dismissing navigation
            } else {
                await dialog.accept(); // Continue navigation
            }
        } else {
            console.warn(`PLAYWRIGHT_BACKEND_JS: Unhandled dialog type: ${dialog.type()}. Dismissing.`);
            await dialog.dismiss(); // Dismiss unknown dialogs
        }
    });

    // File choosers
    p.on('filechooser', async fileChooser => {
        console.log('PLAYWRIGHT_BACKEND_JS: File chooser opened.');
        const filePickerResponse = await callCPlusPlusSynchronously('filePickerRequested', { oldFile: '' }); // oldFile is typically the value attribute
        if (filePickerResponse.handled && filePickerResponse.chosenFile) {
            await fileChooser.setFiles(filePickerResponse.chosenFile);
        } else {
            await fileChooser.setFiles([]); // No files selected
        }
    });

    // Handle new pages (e.g., window.open)
    browserContext.on('page', newPage => {
        console.log('PLAYWRIGHT_BACKEND_JS: New page created by browser context.');
        // This is complex: need to send a unique identifier for this new page back to C++.
        // C++ will then create a new WebPage instance with a new PlaywrightEngineBackend,
        // and that backend will need to control *this specific Playwright page object*.
        // For now, let's just log and dismiss the new page.
        // A robust solution needs a mechanism to map C++ IEngineBackend instances to Playwright page objects.
        newPage.close(); // For now, close new pages created by window.open
        console.warn("PLAYWRIGHT_BACKEND_JS: New page created by browser context. Auto-closing for now. Full implementation pending.");
    });
}
