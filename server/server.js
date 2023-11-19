const bcrypt = require("bcrypt");
const cookieParser = require("cookie-parser");
require("dotenv").config();
const express = require("express");
const fs  = require("fs");
const httpStatus = require("http-status-codes").StatusCodes;
const jwt = require("jsonwebtoken");
const mysql = require("mysql2/promise");
const path = require("path");

const app = express();

const port = process.env.PORT;
const secureCookies = process.env.SECURE_COOKIES;
const privateKey = fs.readFileSync("jwt-private-key.pem");
const publicKey = fs.readFileSync("jwt-public-key.pem");

const sqlOptions = {
    host: process.env.MYSQL_HOST,
    user: process.env.MYSQL_USER,
    password: process.env.MYSQL_PASSWORD,
    database: process.env.MYSQL_DATABASE
};

// Use IIFE because node doesn't allow top level await
/** @type mysql.Connection */
let connection;
(async () => {
    try {
        connection = await mysql.createConnection(sqlOptions);
        console.log("Connected to database");
    } catch (err) {
        console.error("Error connecting to database " + err.stack);
    }
})();

// Allow JSON responses, serve the site, and make cookies work
app.use(express.json());
app.use(express.static("../client/dist"));
app.use(cookieParser());

app.get("/api/load-state", (req, res) => {
    console.log("load req uid: " + req.query.uid);
    console.log("load req game: " + req.query.game);

    console.log("Authenticated?:", authenticateToken(req.cookies.authToken));

    res.json({message: "load req " + req});
});

app.post("/api/save-state", (req, res) => {
    console.log("save req uid: " + req.body.uid);
    console.log("save req game: " + req.body.game);

    console.log("Authenticated?:", authenticateToken(req.cookies.authToken));

    res.json({message: "save req body " + req.body});
});

app.post("/api/register", async (req, res) => {
    const email = req.body.email;
    const password = req.body.password;

    // TODO: FOR IMPROVED PERFORMANCE WE COULD CACHE A LIST OF USERNAMES
    // IN MEMORY AND CHECK THAT FIRST BEFORE QUERYING THE DATABASE
    if (!email || !password) {
        res.status(httpStatus.BAD_REQUEST).send("Missing username or password");
        return;
    }

    if (!validateEmail(email)) {
        res.status(httpStatus.BAD_REQUEST).send("Invalid email");
        return;
    }

    // FIXME: COULD CRASH
    const [emailQueryResult] = await connection.execute("select email from user where email = ?", [email]);

    if (emailQueryResult.length > 0) {
        res.status(httpStatus.BAD_REQUEST).send("User already registered with this email");
        return;
    }

    // FIXME: ISSUE, THE PASSWORD ACTUALLY STORES WITH THE SALT
    // APPENDED TO IT. THIS IS AN ISSUE BECAUSE AN ATTACKER
    // DOESN'T ACTUALLY NEED TO KNOW THE SALT IN ORDER TO
    // DECRYPT THE PASSWORD
    const salt = bcrypt.genSaltSync(10);
    const hashedPassword = bcrypt.hashSync(password, salt);

    // FIXME: COULD CRASH
    const insertQueryResult = await connection.execute("insert into user (email, password, salt) values (?, ?, ?)", [email, hashedPassword, salt]);
    // FIXME: CHECK INSERT SUCCESS
    res.status(httpStatus.OK).send("Registration successful");
});

app.post("/api/login", async (req, res) => {
    const email = req.body.email;
    const password = req.body.password;

    if (!email || !password) {
        res.status(httpStatus.BAD_REQUEST).send("Missing username or password");
        return;
    }

    if (!validateEmail(email)) {
        res.status(httpStatus.BAD_REQUEST).send("Invalid email");
        return;
    }

    // FIXME: COULD CRASH
    const [usernameQueryResult] = await connection.execute("select salt, password from user where email = ?", [email]);
    if (usernameQueryResult.length === 0) {
        res.status(httpStatus.BAD_REQUEST).send("Invalid username or password");
        return;
    }

    const hashedPassword = bcrypt.hashSync(password, usernameQueryResult[0].salt);

    if (hashedPassword === usernameQueryResult[0].password.toString()) {
        const token = jwt.sign({email: email}, privateKey, {
            algorithm: "RS256",
            expiresIn: "1h",
            subject: email
        });

        res.cookie("authToken", token, {
            httpOnly: true,
            secure: secureCookies,
            sameSite: "strict",
            maxAge: 1000 * 60 * 60 * 24 * 3 // 3 days
        });

        res.status(httpStatus.OK).send("Login successful");
        return;
    }

    res.status(httpStatus.BAD_REQUEST).send("Invalid username or password");
});

app.post("/api/save-settings", (req, res) => {

});

app.get("/api/load-settings", (req, res) => {

});

app.post("/api/forgot-password", (req, res) => {

});

app.post("/api/reset-password", (req, res) => {

});

// Catch all route to avoid breaking on refresh
// Any state on the client will not be preserved unless we either
// send it to the server on refresh or store it on the client
app.get("/*", (req, res) => {
    res.sendFile(path.join(__dirname, "../client/dist/index.html"));
});

function authenticateToken(token) {
    try {
        jwt.verify(token, publicKey, {algorithms: ["RS256"]});
    } catch (err) {
        return false;
    }
    return true;
}

function validateEmail(email) {
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

app.listen(port, () => {
    console.log("Server running on port " + port);
});
