
// This file is jsx, because embind generates no typedefs for
// the emscripten global Module object. On account of this,
// tsc will refuse to compile this file, siting that there is no constructor
// for Module.
import React from "react";
import ReactDOM from "react-dom/client";
import App from "./components/App.tsx";
import NESCLERouter from "./components/NESCLERouter.tsx";

// Importing normalize.css in the scss file didn't work, so we do it here
import "normalize.css";
import "./index.scss";

import Module from "./core/a.out.js";

// Prevent the app from rendering until the Emscripten module has loaded
if (!window["emuModule"]) {
  // FIXME: WE MAY HAVE TO RE-INIT WEBASM WITH THE ROUTING, I'M NOT SURE
  new Module().then((module) => {
    window["emuModule"] = module;
    window["emulator"] = new module.ESEmu();
    window["emulator"].powerOn();
    window["emulator"].setSampleFrequency(48000);
    window["lastRenderTime"] = Date.now();
    window["frameQueue"] = [];
    window["frameDelta"] = 0;
    window["authToken"] = "";
    window["apiUrl"] = "http://localhost:3000";

    document.addEventListener("visibilitychange", () => {
        window["lastRenderTime"] = Date.now();
        window["frameDelta"] = 0;
    }, []);

    ReactDOM.createRoot(document.getElementById("root")).render(
      <React.StrictMode>
        <NESCLERouter />
      </React.StrictMode>,
    );
  });
} else {
  // We don't want to re-init the app if we are sent here by the router, so
  // we check if the emuModule has already been defined
  ReactDOM.createRoot(document.getElementById("root")).render(
    <React.StrictMode>
      <NESCLERouter />
    </React.StrictMode>,
  );
}
