// Modules to control application life and handle IPC
const { app, ipcMain } = require("electron");

const gqlmapi = require(`../build/${
  process.env.USE_DEBUG_MODULE ? "Debug" : "Release"
}/electron-cppgraphql`);
let serviceStarted = false;

function startService() {
  gqlmapi.startService();
  serviceStarted = true;
}

function stopService() {
  serviceStarted = false;
  gqlmapi.stopService();
}

exports.startGraphQL = function() {
  // Register the IPC callbacks
  ipcMain.handle("startService", startService);
  ipcMain.handle("stopService", stopService);
  ipcMain.handle("parseQuery", (_event, query) => gqlmapi.parseQuery(query));
  ipcMain.handle("discardQuery", (_event, queryId) =>
    gqlmapi.discardQuery(queryId)
  );
  ipcMain.on("fetchQuery", (event, queryId, operationName, variables) =>
    gqlmapi.fetchQuery(
      queryId,
      operationName,
      variables,
      (payload) => {
        if (serviceStarted) {
          event.reply("fetched", queryId, payload);
        }
      },
      () => {
        if (serviceStarted) {
          event.reply("completed", queryId);
        }
      }
    )
  );
  ipcMain.handle("unsubscribe", (_event, queryId) =>
    gqlmapi.unsubscribe(queryId)
  );

  // Quit when all windows are closed.
  app.on("window-all-closed", stopService);
};

exports.preloadPath = require.resolve("./preload");
