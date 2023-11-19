
import React, { useRef, useCallback, useState } from "react";
import { Link, useNavigate } from "react-router-dom";

import "./styles/Login.scss";

const Login: React.FC = () => {
    // TODO: ADD EMAIL VALIDATION
    const [loginEnabled, setLoginEnabled] = useState(false);

    const emailRef = useRef<HTMLInputElement>(null);
    const passwordRef = useRef<HTMLInputElement>(null);
    const loginRef = useRef<HTMLButtonElement>(null);

    const navigate = useNavigate();

    const loginCallback = useCallback(() => {
        const email = emailRef.current!.value;
        const password = passwordRef.current!.value;

        fetch(window.apiUrl + "/api/login", {
            method: "POST",
            credentials: "include",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({email: email, password: password})
        }).then((res) => {
            if (res.ok)
                navigate("/emu");
        }).catch((err) => {
            console.error(err);
        })
    }, [emailRef.current, passwordRef.current]);

    const inputFieldOnChange = useCallback(() => {
        if (emailRef.current!.value && passwordRef.current!.value)
            setLoginEnabled(true);
        else
            setLoginEnabled(false);
    }, []);

    return (
        <div className="loginContainer">
            <div className="loginChild">
                <h2>Login</h2>
                <div>
                    <input ref={emailRef} onChange={inputFieldOnChange} type="text" placeholder="Email" />
                    <input ref={passwordRef} onChange={inputFieldOnChange} type="password" placeholder="Password" />
                    <button ref={loginRef} disabled={!loginEnabled} onClick={loginCallback} >Login</button>
                </div>
                <div>
                    <Link to="/emu">Skip Login</Link>
                    <Link to="/register">Register</Link>
                    <Link to="/forgot-password">Forgot Password</Link>
                </div>
            </div>
        </div>
    );
}

export default Login;
