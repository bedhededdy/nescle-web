import React from "react";
import { BrowserRouter, Routes, Route } from "react-router-dom";

import Login from "./Login";
import NESCLE from "./NESCLE";
import ForgotPassword from "./ForgotPassword";
import Register from "./Register";

// Weird hack to get the root component to render right
import "./styles/App.scss";

const NESCLERouter: React.FC = () => {
    return (
        <BrowserRouter>
            <Routes>
                <Route path="/" element={<Login />} />
                <Route path="/login" element={<Login />} />
                <Route path="/emu" element={<NESCLE />} />
                <Route path="/forgot-password" element={<ForgotPassword />} />
                <Route path="/register" element={<Register />} />
            </Routes>
        </BrowserRouter>
    );
}

export default NESCLERouter;
