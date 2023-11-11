
// This file is jsx, because embind generates no typedefs for
// the emscripten global Module object. On account of this,
// tsc will refuse to compile this file, siting that there is no constructor
// for Module.
import React from "react";
import ReactDOM from "react-dom/client";
import App from "./components/App.tsx";

// Importing normalize.css in the scss file didn't work, so we do it here
import "normalize.css";
import "./index.scss";

import Module from "./core/a.out.js";

// Prevent the app from rendering until the Emscripten module has loaded
new Module().then((module) => {
  window["emuModule"] = module;
  window["emulator"] = new module.ESEmu();
  window["emulator"].powerOn();
  window["emulator"].setSampleFrequency(48000);
  window["lastRenderTime"] = Date.now();
  window["frameQueue"] = [];
  window["frameDelta"] = 0;

  document.addEventListener("visibilitychange", () => {
    // if (document.visibilityState === "hidden") {
      window["lastRenderTime"] = Date.now();
      window["frameDelta"] = 0;
    // }
      console.log("hey our visiblity changed! make sure this is handled correctly!");
  }, [])

  ReactDOM.createRoot(document.getElementById("root")).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>,
  );
});
