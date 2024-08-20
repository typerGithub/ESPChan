/* Это свободная программа: вы можете перераспространять ее и/или изменять ее на условиях Стандартной общественной лицензии GNU в том виде, в каком она была опубликована Фондом свободного программного обеспечения; либо версии 3 лицензии, либо (по вашему выбору) любой более поздней версии.
Эта программа распространяется в надежде, что она будет полезной, но БЕЗО ВСЯКИХ ГАРАНТИЙ; даже без неявной гарантии ТОВАРНОГО ВИДА или ПРИГОДНОСТИ ДЛЯ ОПРЕДЕЛЕННЫХ ЦЕЛЕЙ. Подробнее см. в Стандартной общественной лицензии GNU.
Вы должны были получить копию Стандартной общественной лицензии GNU вместе с этой программой. Если это не так, см. <https://www.gnu.org/licenses/>. */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <map>

// Замени на свои данные WiFi сети
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// Создание экземпляра веб-сервера
AsyncWebServer server(80);

// Структуры данных
struct User {
    String username;
    String password;
};

std::map<String, User> users; // Хранилище пользователей
std::map<String, String> categories; // Хранилище категорий
std::map<String, std::pair<String, String>> posts; // Хранилище сообщений (ID -> {никнейм, сообщение})

String currentUser = ""; // Хранилище текущего пользователя
String currentCategory = ""; // Хранилище текущей категории

// HTML код страницы
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>espChan</title>
    <style>
        body { font-family: Arial, sans-serif; }
        #messages { border: 1px solid #ccc; height: 300px; overflow-y: scroll; }
        #messageInput { width: 80%; }
        #sendButton { width: 18%; }
    </style>
    <script>
        function sendMessage() {
            var xhr = new XMLHttpRequest();
            var message = document.getElementById("messageInput").value;
            var category = document.getElementById("categorySelect").value;
            xhr.open("POST", "/post", true);
            xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            xhr.send("message=" + encodeURIComponent(message) + "&category=" + encodeURIComponent(category));
            document.getElementById("messageInput").value = "";
        }

        function clearMessages() {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/clear", true);
            xhr.send();
        }

        function registerUser() {
            var xhr = new XMLHttpRequest();
            var username = document.getElementById("regUsername").value;
            var password = document.getElementById("regPassword").value;
            xhr.open("POST", "/register", true);
            xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            xhr.send("username=" + encodeURIComponent(username) + "&password=" + encodeURIComponent(password));
        }

        function logout() {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/logout", true);
            xhr.onload = function () {
                if (xhr.status == 200) {
                    alert("Вы вышли из аккаунта");
                    window.location.reload();
                }
            };
            xhr.send();
        }

        function changeCategory() {
            var category = document.getElementById("categorySelect").value;
            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/category", true);
            xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            xhr.send("category=" + encodeURIComponent(category));
        }

        function refreshPosts() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/posts", true);
            xhr.onload = function () {
                document.getElementById("messages").innerHTML = this.responseText;
            }
            xhr.send();
        }

        function refreshCategories() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/categories", true);
            xhr.onload = function () {
                var categories = JSON.parse(this.responseText);
                var categorySelect = document.getElementById("categorySelect");
                categorySelect.innerHTML = "";
                for (var i = 0; i < categories.length; i++) {
                    var option = document.createElement("option");
                    option.value = categories[i];
                    option.text = categories[i];
                    categorySelect.add(option);
                }
                // Установка текущей категории
                categorySelect.value = currentCategory;
            }
            xhr.send();
        }

        function updateCurrentCategory(category) {
            currentCategory = category;
        }

        var currentCategory = "";

        setInterval(refreshPosts, 1000); // Обновление сообщений каждую секунду
        setInterval(refreshCategories, 10000); // Обновление категорий каждые 10 секунд
        window.onload = function() {
            var categorySelect = document.getElementById("categorySelect");
            categorySelect.onchange = function() {
                changeCategory();
                updateCurrentCategory(categorySelect.value);
            };
        };
    </script>
</head>
<body>
    <h1>Форум ESPChan</h1>
    <div>
        <select id="categorySelect"></select>
        <input id="messageInput" type="text" placeholder="Введите сообщение" />
        <button id="sendButton" onclick="sendMessage()">Отправить</button>
        <button onclick="clearMessages()">Очистить историю чата</button>
    </div>
    <div>
        <h2>Регистрация нового пользователя</h2>
        <input id="regUsername" type="text" placeholder="Введите имя пользователя" />
        <input id="regPassword" type="password" placeholder="Введите пароль" />
        <button onclick="registerUser()">Зарегистрироваться</button>
    </div>
    <div>
        <button onclick="logout()">Выйти из аккаунта</button>
    </div>
    <div id="messages"></div>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);

    // Инициализация SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Ошибка монтирования файловой системы");
        return;
    }

    // Подключение к WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Подключено к WiFi");

    // Добавление категорий
    categories["Цветы"] = "";
    categories["Суккуленты"] = "";
    categories["Электроника"] = "";
    categories["ИИ"] = "";
    categories["Linux Дистрибутивы"] = "";
    categories["MacOS"] = "";
    categories["Windows"] = "";
    categories["Программирование"] = "";
    categories["Arduino"] = "";
    categories["ESP32"] = "";
    categories["Аниме"] = "";
    
    // Обработка запросов
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", htmlPage);
    });

    server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("message", true) && request->hasParam("category", true) && !currentUser.isEmpty()) {
            String message = request->getParam("message", true)->value();
            String category = request->getParam("category", true)->value();
            if (categories.find(category) != categories.end()) {
                String postId = String(millis());
                posts[postId] = std::make_pair(currentUser, message); // Сохранение никнейма и сообщения
            }
        }
        request->send(200, "text/plain", "Message posted");
    });

    server.on("/posts", HTTP_GET, [](AsyncWebServerRequest *request){
        String response = "<h2>Сообщения:</h2><ul>";
        for (const auto& post : posts) {
            if (post.second.first == currentCategory || currentCategory == "") {
                response += "<li><strong>" + post.second.first + ":</strong> " + post.second.second + "</li>";
            }
        }
        response += "</ul>";
        request->send(200, "text/html", response);
    });

    server.on("/categories", HTTP_GET, [](AsyncWebServerRequest *request){
        String response = "[";
        bool first = true;
        for (const auto& category : categories) {
            if (!first) {
                response += ",";
            }
            response += "\"" + category.first + "\"";
            first = false;
        }
        response += "]";
        request->send(200, "application/json", response);
    });

    server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        posts.clear(); // Очистка всех сообщений
        request->send(200, "text/plain", "Chat history cleared");
    });

    server.on("/register", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();
            if (users.find(username) == users.end()) {
                users[username] = {username, password};
                currentUser = username; // Авторизация нового пользователя сразу после регистрации
                request->send(200, "text/plain", "Registration successful");
            } else {
                request->send(400, "text/plain", "Username already exists");
            }
        } else {
            request->send(400, "text/plain", "Missing username or password");
        }
    });

    server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request){
        currentUser = ""; // Очистка текущего пользователя
        request->send(200, "text/plain", "Logged out");
    });

    server.on("/category", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("category", true)) {
            currentCategory = request->getParam("category", true)->value();
        }
        request->send(200, "text/plain", "Category updated");
    });

    server.serveStatic("/", SPIFFS, "/");

    server.begin();
}

void loop() {
    // Основной цикл может быть пустым
}
