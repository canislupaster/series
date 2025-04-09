// modules are defined as an array
// [ module function, map of requires ]
//
// map of requires is short require name -> numeric require
//
// anything defined in a previous bundle is accessed via the
// orig method which is the require for previous bundles

(function (modules, entry, mainEntry, parcelRequireName, globalName) {
  /* eslint-disable no-undef */
  var globalObject =
    typeof globalThis !== 'undefined'
      ? globalThis
      : typeof self !== 'undefined'
      ? self
      : typeof window !== 'undefined'
      ? window
      : typeof global !== 'undefined'
      ? global
      : {};
  /* eslint-enable no-undef */

  // Save the require from previous bundle to this closure if any
  var previousRequire =
    typeof globalObject[parcelRequireName] === 'function' &&
    globalObject[parcelRequireName];

  var cache = previousRequire.cache || {};
  // Do not use `require` to prevent Webpack from trying to bundle this call
  var nodeRequire =
    typeof module !== 'undefined' &&
    typeof module.require === 'function' &&
    module.require.bind(module);

  function newRequire(name, jumped) {
    if (!cache[name]) {
      if (!modules[name]) {
        // if we cannot find the module within our internal map or
        // cache jump to the current global require ie. the last bundle
        // that was added to the page.
        var currentRequire =
          typeof globalObject[parcelRequireName] === 'function' &&
          globalObject[parcelRequireName];
        if (!jumped && currentRequire) {
          return currentRequire(name, true);
        }

        // If there are other bundles on this page the require from the
        // previous one is saved to 'previousRequire'. Repeat this as
        // many times as there are bundles until the module is found or
        // we exhaust the require chain.
        if (previousRequire) {
          return previousRequire(name, true);
        }

        // Try the node require function if it exists.
        if (nodeRequire && typeof name === 'string') {
          return nodeRequire(name);
        }

        var err = new Error("Cannot find module '" + name + "'");
        err.code = 'MODULE_NOT_FOUND';
        throw err;
      }

      localRequire.resolve = resolve;
      localRequire.cache = {};

      var module = (cache[name] = new newRequire.Module(name));

      modules[name][0].call(
        module.exports,
        localRequire,
        module,
        module.exports,
        this
      );
    }

    return cache[name].exports;

    function localRequire(x) {
      var res = localRequire.resolve(x);
      return res === false ? {} : newRequire(res);
    }

    function resolve(x) {
      var id = modules[name][1][x];
      return id != null ? id : x;
    }
  }

  function Module(moduleName) {
    this.id = moduleName;
    this.bundle = newRequire;
    this.exports = {};
  }

  newRequire.isParcelRequire = true;
  newRequire.Module = Module;
  newRequire.modules = modules;
  newRequire.cache = cache;
  newRequire.parent = previousRequire;
  newRequire.register = function (id, exports) {
    modules[id] = [
      function (require, module) {
        module.exports = exports;
      },
      {},
    ];
  };

  Object.defineProperty(newRequire, 'root', {
    get: function () {
      return globalObject[parcelRequireName];
    },
  });

  globalObject[parcelRequireName] = newRequire;

  for (var i = 0; i < entry.length; i++) {
    newRequire(entry[i]);
  }

  if (mainEntry) {
    // Expose entry point to Node, AMD or browser globals
    // Based on https://github.com/ForbesLindesay/umd/blob/master/template.js
    var mainExports = newRequire(mainEntry);

    // CommonJS
    if (typeof exports === 'object' && typeof module !== 'undefined') {
      module.exports = mainExports;

      // RequireJS
    } else if (typeof define === 'function' && define.amd) {
      define(function () {
        return mainExports;
      });

      // <script>
    } else if (globalName) {
      this[globalName] = mainExports;
    }
  }
})({"1nFG4":[function(require,module,exports) {
module.exports = require("640c9a7c8b1d445e")(require("b79c31e830a3c514").getBundleURL("6KyRn") + "cas.d026a4bb.js" + "?" + Date.now()).catch((err)=>{
    delete module.bundle.cache[module.id];
    throw err;
}).then(()=>module.bundle.root("jFXNV"));

},{"640c9a7c8b1d445e":"9h1Ph","b79c31e830a3c514":"acFkO"}],"9h1Ph":[function(require,module,exports) {
"use strict";
/* global __parcel__importScripts__:readonly*/ var cacheLoader = require("e18e0f4b8102d3ab");
module.exports = cacheLoader(function(bundle) {
    return new Promise(function(resolve, reject) {
        try {
            importScripts(bundle);
            resolve();
        } catch (e) {
            reject(e);
        }
    });
});

},{"e18e0f4b8102d3ab":"edxtI"}],"edxtI":[function(require,module,exports) {
"use strict";
var cachedBundles = {};
var cachedPreloads = {};
var cachedPrefetches = {};
function getCache(type) {
    switch(type){
        case "preload":
            return cachedPreloads;
        case "prefetch":
            return cachedPrefetches;
        default:
            return cachedBundles;
    }
}
module.exports = function(loader, type) {
    return function(bundle) {
        var cache = getCache(type);
        if (cache[bundle]) return cache[bundle];
        return cache[bundle] = loader.apply(null, arguments).catch(function(e) {
            delete cache[bundle];
            throw e;
        });
    };
};

},{}],"acFkO":[function(require,module,exports) {
"use strict";
var bundleURL = {};
function getBundleURLCached(id) {
    var value = bundleURL[id];
    if (!value) {
        value = getBundleURL();
        bundleURL[id] = value;
    }
    return value;
}
function getBundleURL() {
    try {
        throw new Error();
    } catch (err) {
        var matches = ("" + err.stack).match(/(https?|file|ftp|(chrome|moz|safari-web)-extension):\/\/[^)\n]+/g);
        if (matches) // The first two stack frames will be this function and getBundleURLCached.
        // Use the 3rd one, which will be a runtime in the original bundle.
        return getBaseURL(matches[2]);
    }
    return "/";
}
function getBaseURL(url) {
    return ("" + url).replace(/^((?:https?|file|ftp|(chrome|moz|safari-web)-extension):\/\/.+)\/[^/]+$/, "$1") + "/";
}
// TODO: Replace uses with `new URL(url).origin` when ie11 is no longer supported.
function getOrigin(url) {
    var matches = ("" + url).match(/(https?|file|ftp|(chrome|moz|safari-web)-extension):\/\/[^/]+/);
    if (!matches) throw new Error("Origin not found");
    return matches[0];
}
exports.getBundleURL = getBundleURLCached;
exports.getBaseURL = getBaseURL;
exports.getOrigin = getOrigin;

},{}]},[], null, "parcelRequire94c2")
/**
 * @license
 * Copyright 2015 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */ // Pthread Web Worker startup routine:
// This is the entry point file that is loaded first by each Web Worker
// that executes pthreads on the Emscripten application.
"use strict";
var Module = {};
// Thread-local guard variable for one-time init of the JS state
var initializedJS = false;
function assert(condition, text) {
    if (!condition) abort("Assertion failed: " + text);
}
function threadPrintErr() {
    var text = Array.prototype.slice.call(arguments).join(" ");
    console.error(text);
}
function threadAlert() {
    var text = Array.prototype.slice.call(arguments).join(" ");
    postMessage({
        cmd: "alert",
        text: text,
        threadId: Module["_pthread_self"]()
    });
}
// We don't need out() for now, but may need to add it if we want to use it
// here. Or, if this code all moves into the main JS, that problem will go
// away. (For now, adding it here increases code size for no benefit.)
var out = ()=>{
    throw "out() is not defined in worker.js.";
};
var err = threadPrintErr;
self.alert = threadAlert;
Module["instantiateWasm"] = (info, receiveInstance)=>{
    // Instantiate from the module posted from the main thread.
    // We can just use sync instantiation in the worker.
    var module = Module["wasmModule"];
    // We don't need the module anymore; new threads will be spawned from the main thread.
    Module["wasmModule"] = null;
    var instance = new WebAssembly.Instance(module, info);
    // TODO: Due to Closure regression https://github.com/google/closure-compiler/issues/3193,
    // the above line no longer optimizes out down to the following line.
    // When the regression is fixed, we can remove this if/else.
    return receiveInstance(instance);
};
// Turn unhandled rejected promises into errors so that the main thread will be
// notified about them.
self.onunhandledrejection = (e)=>{
    throw e.reason ?? e;
};
function handleMessage(e) {
    try {
        if (e.data.cmd === "load") {
            // Until we initialize the runtime, queue up any further incoming messages.
            let messageQueue = [];
            self.onmessage = (e)=>messageQueue.push(e);
            // And add a callback for when the runtime is initialized.
            self.startWorker = (instance)=>{
                Module = instance;
                // Notify the main thread that this thread has loaded.
                postMessage({
                    "cmd": "loaded"
                });
                // Process any messages that were queued before the thread was ready.
                for (let msg of messageQueue)handleMessage(msg);
                // Restore the real message handler.
                self.onmessage = handleMessage;
            };
            // Module and memory were sent from main thread
            Module["wasmModule"] = e.data.wasmModule;
            // Use `const` here to ensure that the variable is scoped only to
            // that iteration, allowing safe reference from a closure.
            for (const handler of e.data.handlers)Module[handler] = function() {
                postMessage({
                    cmd: "callHandler",
                    handler,
                    args: [
                        ...arguments
                    ]
                });
            };
            Module["wasmMemory"] = e.data.wasmMemory;
            Module["buffer"] = Module["wasmMemory"].buffer;
            Module["workerID"] = e.data.workerID;
            Module["ENVIRONMENT_IS_PTHREAD"] = true;
            (e.data.urlOrBlob ? __parcel__require__(e.data.urlOrBlob) : parcelRequire94c2("1nFG4")).then((exports)=>exports.default(Module));
        } else if (e.data.cmd === "run") {
            // Pass the thread address to wasm to store it for fast access.
            Module["__emscripten_thread_init"](e.data.pthread_ptr, /*isMainBrowserThread=*/ 0, /*isMainRuntimeThread=*/ 0, /*canBlock=*/ 1);
            // Await mailbox notifications with `Atomics.waitAsync` so we can start
            // using the fast `Atomics.notify` notification path.
            Module["__emscripten_thread_mailbox_await"](e.data.pthread_ptr);
            assert(e.data.pthread_ptr);
            // Also call inside JS module to set up the stack frame for this pthread in JS module scope
            Module["establishStackSpace"]();
            Module["PThread"].receiveObjectTransfer(e.data);
            Module["PThread"].threadInitTLS();
            if (!initializedJS) {
                // Embind must initialize itself on all threads, as it generates support JS.
                // We only do this once per worker since they get reused
                Module["__embind_initialize_bindings"]();
                initializedJS = true;
            }
            try {
                Module["invokeEntryPoint"](e.data.start_routine, e.data.arg);
            } catch (ex) {
                if (ex != "unwind") // The pthread "crashed".  Do not call `_emscripten_thread_exit` (which
                // would make this thread joinable).  Instead, re-throw the exception
                // and let the top level handler propagate it back to the main thread.
                throw ex;
            }
        } else if (e.data.cmd === "cancel") {
            if (Module["_pthread_self"]()) Module["__emscripten_thread_exit"](-1);
        } else if (e.data.target === "setimmediate") ;
        else if (e.data.cmd === "checkMailbox") {
            if (initializedJS) Module["checkMailbox"]();
        } else if (e.data.cmd) {
            // The received message looks like something that should be handled by this message
            // handler, (since there is a e.data.cmd field present), but is not one of the
            // recognized commands:
            err("worker.js received unknown command " + e.data.cmd);
            err(e.data);
        }
    } catch (ex) {
        err("worker.js onmessage() captured an uncaught exception: " + ex);
        if (ex && ex.stack) err(ex.stack);
        if (Module["__emscripten_thread_crashed"]) Module["__emscripten_thread_crashed"]();
        throw ex;
    }
}
self.onmessage = handleMessage;

//# sourceMappingURL=cas.worker.87ea8e45.js.map
