/*jslint sloppy: true, nomen: true */

/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ivan De Marino <ivan.de.marino@gmail.com>

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

// repl.js
// This script runs inside the browser's JavaScript context (via Playwright).
// It interacts with the C++ REPL object exposed as `window._repl`.

var REPL = REPL || {};

(function () {

/**
 * Cache to hold completions
 */
var _cache = {};

/**
 * Return the Completions of the Object, applying the prefix
 *
 * @param expression The string expression to evaluate to get the object (e.g., "phantom", "page", "window.document")
 * @param prefix Limit completions to the one starting with this prefix
 */
REPL._getCompletions = function (expression, prefix) {
    let completions = [];

    if (typeof prefix !== "string") {
        prefix = "";
    } else {
        prefix = prefix.trim();
    }

    let obj;
    try {
        // Evaluate the expression to get the target object
        // Use a safe evaluation for window context
        if (expression === "window") {
            obj = window;
        } else if (expression === "this") {
            obj = this; // 'this' context in the immediate scope where repl.js runs
        } else {
            obj = eval(expression); // Danger zone: eval'ing user input directly
        }
    } catch (e) {
        // console.error("REPL JS (getCompletions): Error evaluating expression:", expression, e);
        return [];
    }

    if (obj === null || (typeof obj !== 'object' && typeof obj !== 'function')) {
        // Primitives like strings, numbers don't have properties to complete
        return [];
    }

    // Try to get QObject inherited class's name from the C++ _repl object
    let className = null;
    try {
        if (window._repl && typeof window._repl.getClassName === 'function') {
            className = window._repl.getClassName(obj); // Call C++ method
        }
    } catch (e) {
        // console.warn("REPL JS (getCompletions): _repl.getClassName failed:", e);
    }


    if (className && className !== "QObject") { // If it's a QObject, use C++ introspection
        // Initialize completions for this class as needed
        if (_cache[className] === undefined) {
            if (window._repl && typeof window._repl.enumerateCompletions === 'function') {
                _cache[className] = window._repl.enumerateCompletions(obj); // Call C++ method
            } else {
                _cache[className] = [];
                console.warn("REPL JS (getCompletions): _repl.enumerateCompletions not available.");
            }
        }

        let key = className;
        if (prefix !== "") {
            key = className + "-" + prefix; // Unique key for cached filtered results
            if (_cache[key] === undefined) {
                // Filter out completions from the full list
                const regexp = new RegExp("^" + prefix);
                _cache[key] = _cache[className].filter(function (elm) {
                    return regexp.test(elm);
                });
            }
        }
        completions = _cache[key] || [];

    } else { // Fallback for native JavaScript objects
        try {
            // Enumerate properties (own and inherited)
            for (let k in obj) {
                if (k.indexOf(prefix) === 0) { // Check prefix here
                    // Exclude properties that are functions if you only want data members,
                    // or include them. Original code seemed to include both.
                    // For now, include all matching properties.
                    completions.push(k);
                }
            }
            completions.sort();
        } catch (e) {
            // console.error("REPL JS (getCompletions): Error iterating native object properties:", e);
        }
    }

    return completions;
};

/**
 * This utility function is used to pretty-print the result of an expression.
 * @see https://developer.mozilla.org/En/Using_native_JSON#The_replacer_parameter
 *
 * @param k Property key name - empty string if it's the object being stringified
 * @param v Property value
 */
REPL._expResStringifyReplacer = (function () {
    const seen = new WeakSet(); // Use WeakSet for garbage collection
    return function (k, v) {
        // Filter out properties if we're at the top level and have a lastEval object.
        // This is primarily for the first level of stringification of the evaluated result.
        if (k === "" && REPL._lastEval && v === REPL._lastEval) {
            let mock = {};
            try {
                // Get all possible completions for the object (real properties/methods)
                const iarr = REPL._getCompletions("", ""); // Get all completions of the REPL._lastEval (empty string means itself)
                // Note: REPL._getCompletions needs to work correctly without 'expression' if 'obj' is passed directly.
                // The current _getCompletions needs 'expression', so this might need adjustment.
                // For now, let's assume REPL._lastEval is directly inspectable.
                // Or, modify _getCompletions to take an object directly if obj is not a string path.

                // Simplified: just iterate over own properties for stringify,
                // and mark functions. For deep inspection, the user will call `_getCompletions`.
                if (typeof REPL._lastEval === 'object' && REPL._lastEval !== null) {
                    for (let prop in REPL._lastEval) {
                        try {
                            if (typeof REPL._lastEval[prop] === 'function') {
                                mock[prop] = "[Function]";
                            } else {
                                mock[prop] = REPL._lastEval[prop];
                            }
                        } catch (e) {
                            mock[prop] = "[Error Accessing Property]";
                        }
                    }
                    return mock;
                }
                return v; // If not an object, return as is
            } catch (e) {
                // Fallback for errors during introspection
                return "[Error in Stringifier]";
            }
        }

        // Handle circular references
        if (typeof v === 'object' && v !== null) {
            if (seen.has(v)) {
                return '[Circular]';
            }
            seen.add(v);

            // Special handling for QObject-like objects from C++
            if (typeof v._className === 'function' && typeof v.objectName === 'function') {
                try {
                    return `[QObject ${v._className()} (name: ${v.objectName()})]`;
                } catch (e) {
                    return `[QObject (Error getting name)]`;
                }
            }
            // Handle NodeList/HTMLCollection specifically to show item count
            if (v instanceof NodeList || v instanceof HTMLCollection) {
                return `[${v.constructor.name} (length: ${v.length})]`;
            }

            // You might want to stringify specific properties for known complex objects.
            // For example, for a 'page' object (if exposed globally):
            if (v && typeof v.url === 'function' && typeof v.title === 'function') {
                try {
                    return `[Page (URL: ${v.url()}, Title: ${v.title()})]`;
                } catch(e) {
                    return `[Page (Error accessing properties)]`;
                }
            }
        }

        // Functions are normally ignored by JSON.stringify; replace with a string
        if (typeof v === "function") {
            return "[Function]";
        }

        return v;
    };
})();

})();
