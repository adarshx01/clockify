#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <vector>
#include <memory>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define TIMEZONE_OFFSET 19800
#define MAX_TASKS 20
#define TASK_CHECK_INTERVAL 1000
#define DISPLAY_TASK_DURATION 60000

const char* ssid = "ssid";
const char* password = "wifi-password";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

struct Task {
    String description;
    int startHours;
    int startMinutes;
    int endHours;
    int endMinutes;
    bool notified;
    String id;
};

std::vector<Task> tasks;
std::shared_ptr<Task> currentDisplayTask;
bool timerMode = false;
int timerSeconds = 0;
bool timerRunning = false;
unsigned long timerStartTime = 0;
unsigned long lastTaskShowTime = 0;
unsigned long lastTaskCheck = 0;


const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Clock Manager</title>
    <style>
    :root {
      --primary-color: #4CAF50;
      --primary-hover: #45a049;
      --text-color: #e0e0e0;
      --secondary-bg: rgba(255, 255, 255, 0.1);
      --border-color: rgba(255, 255, 255, 0.2);
    }

    body {
        font-family: Arial, sans-serif;
        margin: 0;
        padding: 0;
        background: linear-gradient(27deg, #151515 5px, transparent 5px) 0 5px,
                    linear-gradient(207deg, #151515 5px, transparent 5px) 10px 0px,
                    linear-gradient(27deg, #222 5px, transparent 5px) 0px 10px,
                    linear-gradient(207deg, #222 5px, transparent 5px) 10px 5px,
                    linear-gradient(90deg, #1b1b1b 10px, transparent 10px),
                    linear-gradient(#1d1d1d 25%, #1a1a1a 25%, #1a1a1a 50%, transparent 50%, transparent 75%, #242424 75%, #242424);
        background-color: #131313;
        background-size: 20px 20px;
        color: var(--text-color);
        line-height: 1.6;
        min-height: 100vh;
    }

    .container {
        max-width: 50rem;
        margin: 0 auto;
        padding: 2rem;
    }

    h2, h3 {
        color: var(--text-color);
    }

    .control-panel, .task-panel {
        margin: 1.25rem 0;
        padding: 1.6rem;
        background-color: var(--secondary-bg);
        border: 1px solid var(--border-color);
        border-radius: 0.625rem;
    }

    .timer-controls {
        display: flex;
        align-items: center;
        justify-content: space-between;
        flex-wrap: wrap;
        gap: 1rem;
    }

    .styled-btn {
      border-radius: 1.3rem;
      width: 70%;
      height: 2rem;
      margin: 2rem auto;
      padding: 0 1rem;
      background: linear-gradient(to right, #C2FFC7, violet, blue); 
      color: white;
      font-size: 0.8rem;
      font-weight: bold; 
      border: none;
      cursor: pointer;
      transition: all 0.3s ease-in-out;
      display: inline-block;
      text-align: center; 
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); 
    }

    .delete-btn {
        background: linear-gradient(to right, violet, orange, orange, violet);
        width: 6rem;
        height: 2rem;
        padding: 0 1rem;
        font-size: 0.8rem;
        color: white;
      	font-weight: bold;
        margin-left: 1rem;
        transition: all 0.2s ease-in-out;
        border-radius: 1.3rem;
        border: none;
    }
	
    .delete-btn:hover {
        transform: scale(1.05);
    }

    .styled-btn:hover {
        transform: scale(1.05);
    }

    .styled-input {
        font-size: 0.9rem;
        padding-left: 0.5rem;
        padding-right: 0.5rem;
        padding-top: 0.05rem;
        padding-bottom: 0.05rem;
        border-radius: 0.5rem;
        background-color: var(--secondary-bg);
        border: 1px solid var(--border-color);
        color: var(--text-color);
        margin: 5px 0;
        width: 80%;
        display: block;
        height: 2rem;
        margin: 10px auto;
    }
    .styled-input2 {
        font-size: 0.9rem;
        padding-left: 0.5rem;
        padding-right: 0.5rem;
        padding-top: 0.05rem;
        padding-bottom: 0.05rem;
        border-radius: 0.5rem;
        background-color: var(--secondary-bg);
        border: 1px solid var(--border-color);
        color: var(--text-color);
        margin: 5px 0;
        width: 40%;
        display: block;
        margin: 10px auto;
    }

.time-inputs {
  display: flex;
  gap: 20px; 
}

.time-group {
  display: flex;
  align-items: center; 
  gap: 10px; 
}

    .task {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding: 0.625rem;
        margin: 0.3125rem 0;
        background: var(--secondary-bg);
        border-radius: 0.5rem;
        border: 1px solid var(--border-color);
        font-size: 0.9rem;
    }

    .task-info {
        flex-grow: 1;
    }

    @media (max-width: 37.5rem) {

        .control-panel, .task-panel {
            padding: 1rem;
        }
        input, .styled-btn {
            width: 100%;
            margin: 0.3125rem 0;
        }
        .timer-controls {
            flex-direction: column;
            align-items: stretch;
        }
        .time-inputs {
            flex-direction: column;
        }
        .time-group {
            width: 100%;
        }
    }
    </style>
</head>
<body>
<div class="container" style="text-align: center;">
  <h2 style="display: inline;">Smart Clock<span style="display: inline; font-size: 0.7rem; margin-left:0.5rem;">v0.1</span></h2>
  <div class="task-list" id="taskList">
    <h3>Tasks</h3>
  </div>
  <div class="task-panel" style="display: flex; flex-direction: column; align-items: center;">
    <h3>Add Task</h3>
    <input type="text" id="taskDesc" placeholder="Task Details" class="styled-input" />
    <div class="time-inputs" style="display: flex; justify-content: space-between; width: 100%;">
      <div class="time-group">
        <label>Start Time:</label>
        <input type="time" id="startTime" placeholder="Start Time" class="styled-input2" />
      </div>
      <div class="time-group">
        <label>End Time:</label>
        <input type="time" id="endTime" class="styled-input2" />
      </div>
    </div>
    <button class="styled-btn" style="width: 8rem; margin-top: 10px;" onclick="addTask()">Add Task</button>
  </div>
  <div class="control-panel" style="display: flex; flex-direction: column; align-items: center;">
    <h3>Timer Control</h3>
    <div class="timer-controls" style="display: flex; flex-direction: column; align-items: center; gap: 10px;">
      <button id="toggleTimerBtn" class="styled-btn" style="width: 8rem;" onclick="toggleTimer()">Timer On</button>
      <div style="display: flex; flex-direction: row; align-items: center; gap: 10px;">
        <input type="number" id="timerMinutes" placeholder="Minutes" class="styled-input" />
        <button class="styled-btn" style="width: 8rem;" onclick="setTimer()">Set Timer</button>
      </div>
      <button id="startStopBtn" class="styled-btn" style="width: 8rem;" onclick="startStopTimer()">Start Timer</button>
    </div>
  </div>
  <h4>Made with ❤️ by Adarsh</h4>
</div>

  <script>
    function toggleTimer() {
      fetch("/toggle-timer", { method: "POST" })
        .then((response) => response.text())
        .then((state) => alert("Timer mode: " + state));
    }

    function setTimer() {
      const minutes = document.getElementById("timerMinutes").value;
      if (!minutes) {
        alert("Please enter minutes.");
        return;
      }
      fetch("/set-timer", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ minutes: minutes }),
      });
    }

let isTimerRunning = false; 
let isTimerModeOn = false; 

function toggleTimer() {
  isTimerModeOn = !isTimerModeOn; 
  const toggleTimerBtn = document.getElementById("toggleTimerBtn");
  toggleTimerBtn.textContent = isTimerModeOn ? "Timer Off" : "Timer On"; 
  fetch("/toggle-timer", { method: "POST" }) 
    .then((response) => response.text())
    .then((state) => console.log("Timer mode toggled:", state));
}

function startStopTimer() {
  isTimerRunning = !isTimerRunning; 
  const startStopBtn = document.getElementById("startStopBtn");
  startStopBtn.textContent = isTimerRunning ? "Stop Timer" : "Start Timer"; 
  fetch("/start-stop-timer", { method: "POST" }) 
    .then(() => console.log("Timer state toggled:", isTimerRunning)); 
}


    function addTask() {
      const desc = document.getElementById("taskDesc").value;
      const startTime = document.getElementById("startTime").value;
      const endTime = document.getElementById("endTime").value;
      
      if (!desc || !startTime || !endTime) {
        alert("Please fill in all fields.");
        return;
      }
      
      if (endTime <= startTime) {
        alert("End time must be after start time.");
        return;
      }

      fetch("/add-task", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ 
          description: desc, 
          startTime: startTime,
          endTime: endTime 
        }),
      }).then(() => {
        loadTasks();
        document.getElementById("taskDesc").value = "";
        document.getElementById("startTime").value = "";
        document.getElementById("endTime").value = "";
      });
    }

    function removeTask(taskId) {
      fetch(`/remove-task?id=${taskId}`, { method: "POST" })
        .then(() => loadTasks());
    }

    function loadTasks() {
      fetch("/tasks")
        .then((response) => response.json())
        .then((tasks) => {
          const list = document.getElementById("taskList");
          list.innerHTML = "<h3>Tasks</h3>";
          if (tasks.length === 0) {
            list.innerHTML += '<p class="no-tasks">No tasks available.</p>';
          } else {
            tasks.forEach((task) => {
              const div = document.createElement("div");
              div.className = "task";
              
              const taskInfo = document.createElement("div");
              taskInfo.className = "task-info";
              taskInfo.textContent = `${task.description} - ${String(task.startHours).padStart(2, "0")}:${String(task.startMinutes).padStart(2, "0")} to ${String(task.endHours).padStart(2, "0")}:${String(task.endMinutes).padStart(2, "0")}`;
              
              const deleteBtn = document.createElement("button");
              deleteBtn.className = " delete-btn";
              deleteBtn.textContent = "Remove";
              deleteBtn.onclick = () => removeTask(task.id);
              
              div.appendChild(taskInfo);
              div.appendChild(deleteBtn);
              list.appendChild(div);
            });
          }
        });
    }

    setInterval(loadTasks, 5000);
    loadTasks();
  </script>
</body>
</html>
)=====";

void setupDisplay() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        delay(1000);
        ESP.restart();
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
}

void setupWiFi() {
    WiFi.mode(WIFI_STA);  // Set WiFi mode explicitly
    WiFi.begin(ssid, password);
    
    display.setTextSize(1);
    display.setCursor(25, 15);
    display.println("Connecting ...");
    display.display();
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        ESP.restart();  // Restart if connection fails
    }
    
    display.clearDisplay();
    display.setCursor(28, 15);
    display.println("Connected!");
    display.setCursor(22, 30);
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
}

void setupTime() {
    timeClient.begin();
    timeClient.setTimeOffset(TIMEZONE_OFFSET);
    timeClient.update();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(D2, D1);
    
    setupDisplay();
    setupWiFi();
    setupTime();
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/toggle-timer", HTTP_POST, handleToggleTimer);
    server.on("/set-timer", HTTP_POST, handleSetTimer);
    server.on("/start-stop-timer", HTTP_POST, handleStartStopTimer);
    server.on("/add-task", HTTP_POST, handleAddTask);
    server.on("/tasks", HTTP_GET, handleGetTasks);
    server.on("/remove-task", HTTP_POST, handleRemoveTask);
    
    server.begin();
}

void displayTimeAndTasks() {
    display.clearDisplay();
    
    // Display time
    display.setTextSize(2);
    String timeStr = timeClient.getFormattedTime();
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeStr.c_str(), 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 0);
    display.println(timeStr);
    // Display task
    if (currentDisplayTask && (millis() - lastTaskShowTime < DISPLAY_TASK_DURATION)) {
        display.setTextSize(1);
        String taskTime = String(currentDisplayTask->startHours) + ":" + 
                         (currentDisplayTask->startMinutes < 10 ? "0" : "") + 
                         String(currentDisplayTask->startMinutes);
        
        display.getTextBounds(taskTime.c_str(), 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 25);
        display.println(taskTime);
        
        display.setTextSize(2);
        String descStr = currentDisplayTask->description;
        if (descStr.length() > 16) {
            descStr = descStr.substring(0, 13) + "...";
        }
        display.getTextBounds(descStr.c_str(), 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 45);
        display.println(descStr);
    } else {
        currentDisplayTask = nullptr;  // Clear the pointer if display time expired
        int yPos = 25;
        for (const Task& task : tasks) {
            if (yPos < SCREEN_HEIGHT - 16) {
                display.setTextSize(1);
                String taskTime = String(task.endHours) + ":" + 
                               (task.endMinutes < 10 ? "0" : "") + 
                               String(task.endMinutes);
                display.setCursor(0, yPos);
                display.println(taskTime);
                
                display.setTextSize(2);
                String descStr = task.description;
                if (descStr.length() > 21) {
                    descStr = descStr.substring(0, 18) + "...";
                }
                display.setCursor(31, yPos);
                display.println(descStr);
                
                yPos += 17;
            }
        }
    }
    
    display.display();
}

void displayTimer() {
    display.clearDisplay();
    display.setTextSize(3);
    
    unsigned long currentTime = millis();
    int remainingSeconds;
    
    if (timerRunning) {
        if (currentTime < timerStartTime) { 
            timerStartTime = currentTime;
        }
        remainingSeconds = timerSeconds - ((currentTime - timerStartTime) / 1000);
        if (remainingSeconds <= 0) {
            timerRunning = false;
            remainingSeconds = 0;
        }
    } else {
        remainingSeconds = timerSeconds;
    }
    
    int hours = remainingSeconds / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;
    
    String timerStr = String(hours) + ":" + 
                      (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                      (seconds < 10 ? "0" : "") + String(seconds);
    
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timerStr.c_str(), 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print(timerStr);
    
    display.display();
}

void checkTasks() {
    unsigned long currentTime = millis();
    if (currentTime - lastTaskCheck < TASK_CHECK_INTERVAL) {
        return;
    }
    lastTaskCheck = currentTime;
    
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    
    // Remove expired tasks
    auto newEnd = std::remove_if(tasks.begin(), tasks.end(),
        [&](const Task& task) {
            return (currentHour > task.endHours) || 
                   (currentHour == task.endHours && currentMinute >= task.endMinutes);
        });
    tasks.erase(newEnd, tasks.end());
    
    // Check for tasks that need notification
    for (Task& task : tasks) {
        if (!task.notified && 
            task.startHours == currentHour && 
            task.startMinutes == currentMinute) {
            task.notified = true;
            currentDisplayTask = std::make_shared<Task>(task);
            lastTaskShowTime = currentTime;
        }
    }
}

void updateTimer() {
    if (!timerRunning) return;
    
    unsigned long currentTime = millis();
    if (currentTime < timerStartTime) { 
        timerStartTime = currentTime;
    }
    
    if ((currentTime - timerStartTime) / 1000 >= timerSeconds) {
        timerRunning = false;
    }
}

String generateTaskId() {
    static unsigned long lastId = 0;
    return String(++lastId);
}

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleToggleTimer() {
    timerMode = !timerMode;
    server.send(200, "text/plain", timerMode ? "ON" : "OFF");
}

void handleSetTimer() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, server.arg("plain"));
        
        if (error) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        if (!doc.containsKey("minutes")) {
            server.send(400, "text/plain", "Missing minutes parameter");
            return;
        }
        
        timerSeconds = doc["minutes"].as<int>() * 60;
        server.send(200, "text/plain", "Timer set");
    } else {
        server.send(400, "text/plain", "Missing data");
    }
}

void handleStartStopTimer() {
    if (!timerRunning && timerSeconds > 0) {
        timerStartTime = millis();
        timerRunning = true;
    } else {
        timerRunning = false;
    }
    server.send(200, "text/plain", timerRunning ? "Started" : "Stopped");
}

void handleAddTask() {
    if (tasks.size() >= MAX_TASKS) {
        server.send(400, "text/plain", "Maximum task limit reached");
        return;
    }
    
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, server.arg("plain"));
        
        if (error) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        Task newTask;
        newTask.description = doc["description"].as<String>();
        
        String startTimeStr = doc["startTime"].as<String>();
        String endTimeStr = doc["endTime"].as<String>();
        
        if (startTimeStr.length() < 5 || endTimeStr.length() < 5) {
            server.send(400, "text/plain", "Invalid time format");
            return;
        }
        
        newTask.startHours = startTimeStr.substring(0, 2).toInt();
        newTask.startMinutes = startTimeStr.substring(3, 5).toInt();
        newTask.endHours = endTimeStr.substring(0, 2).toInt();
        newTask.endMinutes = endTimeStr.substring(3, 5).toInt();
        
        if (newTask.startHours > 23 || newTask.startMinutes > 59 ||
            newTask.endHours > 23 || newTask.endMinutes > 59) {
            server.send(400, "text/plain", "Invalid time values");
            return;
        }
        
        newTask.notified = false;
        newTask.id = generateTaskId();
        
        tasks.push_back(newTask);
        server.send(200, "text/plain", "Task added");
    } else {
        server.send(400, "text/plain", "Missing data");
    }
}

void handleRemoveTask() {
    if (server.hasArg("id")) {
        String taskId = server.arg("id");
        
        auto newEnd = std::remove_if(tasks.begin(), tasks.end(),
            [&](const Task& task) { 
                if (task.id == taskId && currentDisplayTask && currentDisplayTask->id == taskId) {
                    currentDisplayTask = nullptr;  // Clear the pointer if we're removing the displayed task
                }
                return task.id == taskId; 
            });
        
        tasks.erase(newEnd, tasks.end());
        server.send(200, "text/plain", "Task removed");
    } else {
        server.send(400, "text/plain", "Missing task ID");
    }
}

void handleGetTasks() {
    DynamicJsonDocument doc(4096);  // Increased buffer size
    JsonArray array = doc.to<JsonArray>();
    
    for (const Task& task : tasks) {
        JsonObject taskObj = array.createNestedObject();
        taskObj["id"] = task.id;
        taskObj["description"] = task.description;
        taskObj["startHours"] = task.startHours;
        taskObj["startMinutes"] = task.startMinutes;
        taskObj["endHours"] = task.endHours;
        taskObj["endMinutes"] = task.endMinutes;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        setupWiFi();  // Attempt to reconnect if WiFi is lost..in Orbityyyyyy
    }
    
    server.handleClient();
    timeClient.update();
    
    if (timerMode) {
        displayTimer();
    } else {
        displayTimeAndTasks();
    }
    
    checkTasks();
    updateTimer();
    delay(10);
}