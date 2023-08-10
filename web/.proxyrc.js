let serveStatic = require('serve-static');

module.exports = function (app) {
  app.use((req, res, next) => {
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    next();
  });

  app.use("/db", serveStatic("./db"));
  app.use("/cas.wasm.map", serveStatic("./cas.wasm.map"));
};