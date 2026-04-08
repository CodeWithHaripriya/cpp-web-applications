#include "crow.h"
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <curl/curl.h>

bool sendEmail(const std::string& to, const std::string& subject, const std::string& body) {
    CURL* curl;
    CURLcode res = CURLE_OK;

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist* recipients = nullptr;
        curl_easy_setopt(curl, CURLOPT_USERNAME, "haripriyaajai99@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "Ammu@1804");
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "haripriyaajai99@gmail.com");

        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        std::string message = "To: " + to + "\r\n"
                              "From: haripriyaajai99@gmail.com\r\n"
                              "Subject: " + subject + "\r\n"
                              "\r\n" + body + "\r\n";

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            std::string* str = static_cast<std::string*>(userdata);
            size_t len = std::min(size*nmemb, str->size());
            memcpy(ptr, str->c_str(), len);
            str->erase(0, len);
            return len;
        });
        curl_easy_setopt(curl, CURLOPT_READDATA, &message);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK);
    }
    return false;
}

int main() {
    crow::SimpleApp app;

    // ------------------- Initialize DB -------------------
    sqlite3* db;
    if (sqlite3_open("users.db", &db)) {
        std::cerr << "Can't open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Auto-create table if it doesn't exist
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "age INTEGER,"
        "email TEXT"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, create_table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }
    sqlite3_close(db);

    // Serve the frontend
    CROW_ROUTE(app, "/")([](const crow::request& req){
    return crow::response(R"(
        <html>
        <head>
            <title>Submission Form</title>
            <style>
                body {
                    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                    background: #f7f9fc;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    margin: 0;
                }

                .container {
                    background: #ffffff;
                    padding: 30px 40px;
                    border-radius: 12px;
                    box-shadow: 0 8px 20px rgba(0,0,0,0.1);
                    max-width: 400px;
                    width: 100%;
                    text-align: center;
                }

                h2 {
                    margin-bottom: 25px;
                    color: #333333;
                }

                input[type="text"], input[type="number"], input[type="email"] {
                    width: 100%;
                    padding: 12px 15px;
                    margin: 10px 0;
                    border: 1px solid #d1d5db;
                    border-radius: 8px;
                    font-size: 16px;
                    outline: none;
                    transition: border 0.3s;
                }

                input[type="text"]:focus, input[type="number"]:focus, input[type="email"]:focus {
                    border-color: #4f46e5;
                }

                button {
                    width: 100%;
                    padding: 12px;
                    background: #4f46e5;
                    color: white;
                    border: none;
                    border-radius: 8px;
                    font-size: 16px;
                    cursor: pointer;
                    transition: background 0.3s;
                }

                button:hover {
                    background: #4338ca;
                }

                .footer {
                    margin-top: 15px;
                    font-size: 14px;
                    color: #6b7280;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h2>Submit Your Details</h2>
                <form id="userForm">
                    <input type="text" id="name" placeholder="Name" required>
                    <input type="number" id="age" placeholder="Age" required>
                    <input type="email" id="email" placeholder="Email" required>
                    <button type="submit">Submit</button>
                </form>
                <div class="footer">We respect your privacy. Your data is safe.</div>
            </div>

            <script>
                document.getElementById("userForm").addEventListener("submit", async (e) => {
                    e.preventDefault();
                    const data = {
                        name: document.getElementById("name").value,
                        age: parseInt(document.getElementById("age").value),
                        email: document.getElementById("email").value
                    };
                    try {
                        const res = await fetch("/submit", {
                            method: "POST",
                            headers: {"Content-Type":"application/json"},
                            body: JSON.stringify(data)
                        });
                        const msg = await res.text();
                        alert(msg);
                        if(res.ok) document.getElementById("userForm").reset();
                    } catch(err) {
                        alert("Submission failed. Please try again.");
                    }
                });
            </script>
        </body>
        </html>
    )");
});

    // POST route
    CROW_ROUTE(app, "/submit").methods("POST"_method)
([](const crow::request& req){
    auto data = crow::json::load(req.body);
    if (!data) return crow::response(400, "Invalid JSON");

    std::string name = data["name"].s();
    int age = data["age"].i();
    std::string email = data["email"].s();

    sqlite3* db;
    if (sqlite3_open("users.db", &db)) {
        std::cerr << "Can't open DB: " << sqlite3_errmsg(db) << std::endl;
        return crow::response(500, "DB Error");
    }

    // Use prepared statement to avoid SQL issues
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (name, age, email) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return crow::response(500, "DB Insert Error");
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, age);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return crow::response(500, "DB Insert Error");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Saved to DB: " << name << std::endl;

    // ----------------- Send Email -----------------
    if (sendEmail(email, "Submission Successful",
                  "Hi " + name + ", your submission was received successfully!")) {
        std::cout << "Email sent to " << email << std::endl;
    } else {
        std::cerr << "Failed to send email to " << email << std::endl;
    }

    // ----------------- Response to client -----------------
    return crow::response(200, "Saved successfully!");
});
    
    app.port(8080).multithreaded().run();
}