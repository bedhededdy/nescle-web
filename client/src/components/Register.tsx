import React, { useRef, useCallback, useState } from "react";
import { Link, useNavigate } from "react-router-dom";

import "./styles/Register.scss";

const Register: React.FC = () => {
    const emailRef = useRef<HTMLInputElement>(null);
    const passwordRef = useRef<HTMLInputElement>(null);
    const confirmPasswordRef = useRef<HTMLInputElement>(null);
    const registerRef = useRef<HTMLButtonElement>(null);

    const navigate = useNavigate();

    const registerCallback = useCallback(() => {
        const email = emailRef.current!.value;
        const password = passwordRef.current!.value;
        const confirmPassword = passwordRef.current!.value;



        if (password !== confirmPassword) {
            console.log("Passwords do not match");
            return;
        }

        fetch(window.apiUrl + "/api/register", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({email: email, password: password})
        }).then((res) => {
            console.log("Register responded with result: ", res.status);
            if (res.ok)
                navigate("/login");
        }).catch((err) => {
            console.error(err);
        })
    }, [emailRef.current, passwordRef.current]);

    return (
        <div className="registerContainer">
            <div className="registerChild">
                <h2>Register</h2>
                <div>
                    <input ref={emailRef} type="text" placeholder="Email" />
                    <input ref={passwordRef} type="password" placeholder="Password" />
                    <input ref={confirmPasswordRef} type="password" placeholder="Confirm Password" />
                    <button ref={registerRef} onClick={registerCallback}>Register</button>
                </div>
                <div>
                    <Link to="/login">Return to the Homepage</Link>
                </div>
            </div>
        </div>
    );
}

export default Register;
