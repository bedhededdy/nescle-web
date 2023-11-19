import React, { useState, useCallback, useRef } from "react";
import { Link, useNavigate, useSearchParams } from "react-router-dom";

const ResetPassword: React.FC = () => {
    const [searchParams, setSearchParams] = useSearchParams();

    const [passwordsMatch, setPasswordsMatch] = useState<boolean | null>(null);
    const [submitEnabled, setSubmitEnabled] = useState(false);

    const passwordRef = useRef<HTMLInputElement>(null);
    const confirmPasswordRef = useRef<HTMLInputElement>(null);
    const resetPasswordRef = useRef<HTMLButtonElement>(null);

    const navigate = useNavigate();

    const passwordOnChange = useCallback(() => {
        // TODO: VALIDATION LOGIC
    }, [passwordRef.current, confirmPasswordRef.current]);

    const resetPasswordCallback = useCallback(() => {
        fetch(window.apiUrl + "/api/reset-password", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            // It is the server's job to associate a token with a user
            body: JSON.stringify({password: passwordRef.current!.value, token: searchParams.get("resetToken")})
        }).then((res) => {
            if (res.ok)
                navigate("/login");
        }).catch((err) => {
            console.error(err);
        })
    }, [passwordRef.current, confirmPasswordRef.current]);

    return (
        <div>
            <div>

            </div>
            <div>
                <Link to="/login">Back to login page</Link>
            </div>
        </div>
    );
}

export default ResetPassword;
