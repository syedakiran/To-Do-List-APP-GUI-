#include<iostream>
#include<map>
#include<vector>
#include<chrono>
#include<ctime>
#include<sstream>
#include<iomanip>
#include<limits>
#include<algorithm>
#include<fstream>
#include<thread>
#include<cstring>

// Simple HTTP server includes (cross-platform)
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // Force link with ws2_32 library
    #pragma comment(lib, "ws2_32")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #define closesocket close
    typedef int SOCKET;
#endif

enum class Priority{
    LOW=0,
    MEDIUM=1,
    HIGH=2, 
    URGENT=3
};

enum class itemstatus{
    Completed,
    Not_Completed
};

std::chrono::system_clock::time_point parse_date(const std::string &date_str) {
    std::tm t = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&t, "%Y-%m-%d"); 
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

std::string convert(Priority p) {
    switch (p) {
        case Priority::LOW: return "Low";
        case Priority::MEDIUM: return "Medium";
        case Priority::HIGH: return "High";
        case Priority::URGENT: return "Urgent";
        default: return "Unknown";
    }
}

// URL decode helper function
std::string url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::stringstream ss;
            ss << std::hex << str.substr(i + 1, 2);
            ss >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

class to_do_item{
public:
    std::string name;
    Priority priority;
    itemstatus status;
    std::string category;
    std::chrono::system_clock::time_point due_date;

    to_do_item(std::string name, Priority priority, std::string category,
               std::chrono::system_clock::time_point due_date = std::chrono::system_clock::now())
        : name(name), priority(priority), status(itemstatus::Not_Completed),
          category(category), due_date(due_date) {}

    to_do_item() : name(""), priority(Priority::LOW), status(itemstatus::Not_Completed), 
                   category(""), due_date(std::chrono::system_clock::now()){}
    
    std::string to_json() const {
        std::time_t due_time = std::chrono::system_clock::to_time_t(due_date);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", std::localtime(&due_time));
        
        std::stringstream ss;
        ss << "{";
        ss << "\"name\":\"" << name << "\",";
        ss << "\"priority\":" << static_cast<int>(priority) << ",";
        ss << "\"status\":\"" << (status == itemstatus::Completed ? "completed" : "pending") << "\",";
        ss << "\"category\":\"" << category << "\",";
        ss << "\"dueDate\":\"" << buffer << "\"";
        ss << "}";
        return ss.str();
    }
};

class to_do_list{
private:
    std::map<std::string,to_do_item> to_do_items;

public:
    void new_entry(const std::string& item_name, Priority p, std::string category, std::chrono::system_clock::time_point due_date) {
        if(to_do_items.count(item_name) > 0) {
            return;
        }
        to_do_items[item_name] = to_do_item(item_name, p, category, due_date);
    }

    void mark_complete(const std::string& item_name) {
        std::map<std::string,to_do_item>::iterator it = to_do_items.find(item_name);
        if (it != to_do_items.end()) {
            it->second.status = itemstatus::Completed;
        }
    }

    void remove(const std::string& item_name) {
        to_do_items.erase(item_name);
    }
    
    void clear_list() {
        to_do_items.clear();
    }
    
    std::string get_all_json() const {
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for(std::map<std::string,to_do_item>::const_iterator it = to_do_items.begin(); 
            it != to_do_items.end(); ++it) {
            if (!first) ss << ",";
            ss << it->second.to_json();
            first = false;
        }
        ss << "]";
        return ss.str();
    }

    void save_to_file(const std::string& filename = "todo_data.txt") const {
        std::ofstream outfile(filename);
        if (!outfile.is_open()) return;
        for (std::map<std::string,to_do_item>::const_iterator it = to_do_items.begin();
             it != to_do_items.end(); ++it) {
            const to_do_item& item = it->second;
            std::time_t due_time = std::chrono::system_clock::to_time_t(item.due_date);
            outfile << item.name << "|" << static_cast<int>(item.priority) << "|" 
                    << static_cast<int>(item.status) << "|" << item.category << "|" 
                    << due_time << "\n";
        }
        outfile.close();
    }

    void load_from_file(const std::string& filename = "todo_data.txt") {
        std::ifstream infile(filename);
        if (!infile.is_open()) return;
        std::string line;
        while (std::getline(infile, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string name_str, priority_str, status_str, category_str, due_date_str;
            if (!std::getline(ss, name_str, '|') ||
                !std::getline(ss, priority_str, '|') ||
                !std::getline(ss, status_str, '|') ||
                !std::getline(ss, category_str, '|') ||
                !std::getline(ss, due_date_str)) {
                continue;
            }
            int priority_int = std::stoi(priority_str);
            int status_int = std::stoi(status_str);
            std::time_t due_time_t = std::stoll(due_date_str);
            to_do_item loaded_item(name_str, static_cast<Priority>(priority_int), 
                                   category_str, std::chrono::system_clock::from_time_t(due_time_t));
            loaded_item.status = static_cast<itemstatus>(status_int);
            to_do_items[name_str] = loaded_item;
        }
        infile.close();
    }
};

// Global instance
to_do_list global_list;

// HTML page with embedded GUI - IMPROVED VERSION
std::string get_html_page() {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>To-Do List Manager</title>
    <style>
        * { 
            margin: 0; 
            padding: 0; 
            box-sizing: border-box; 
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1100px;
            margin: 0 auto;
            background: white;
            border-radius: 24px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
            overflow: hidden;
            animation: fadeIn 0.5s ease-in;
        }
        
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(-20px); }
            to { opacity: 1; transform: translateY(0); }
        }
        
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px 30px;
            text-align: center;
        }
        
        .header h1 { 
            font-size: 2.8em; 
            margin-bottom: 10px;
            font-weight: 700;
            letter-spacing: -0.5px;
        }
        
        .header p {
            font-size: 1.1em;
            opacity: 0.95;
            font-weight: 400;
        }
        
        .main-content { 
            padding: 35px; 
        }
        
        .controls {
            display: flex;
            gap: 12px;
            margin-bottom: 30px;
            flex-wrap: wrap;
        }
        
        .btn {
            padding: 14px 28px;
            border: none;
            border-radius: 12px;
            font-size: 1em;
            cursor: pointer;
            transition: all 0.3s ease;
            font-weight: 600;
            display: inline-flex;
            align-items: center;
            gap: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
        }
        
        .btn:active {
            transform: translateY(0);
        }
        
        .btn-primary { 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white; 
        }
        
        .btn-success { 
            background: linear-gradient(135deg, #28a745 0%, #20c997 100%);
            color: white; 
        }
        
        .btn-danger { 
            background: linear-gradient(135deg, #dc3545 0%, #c82333 100%);
            color: white; 
        }
        
        .tasks-container { 
            display: grid; 
            gap: 18px; 
        }
        
        .task-card {
            background: white;
            border: 2px solid #f0f0f0;
            border-radius: 16px;
            padding: 24px;
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }
        
        .task-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 5px;
            height: 100%;
            background: var(--accent-color);
        }
        
        .task-card:hover {
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.12);
            transform: translateY(-3px);
            border-color: #e0e0e0;
        }
        
        .task-card.completed { 
            opacity: 0.65; 
            background: #f8f9fa; 
        }
        
        .task-title { 
            font-size: 1.4em; 
            font-weight: 700; 
            color: #2c3e50;
            margin-bottom: 4px;
            line-height: 1.3;
        }
        
        .task-card.completed .task-title { 
            text-decoration: line-through; 
            color: #95a5a6; 
        }
        
        .priority-badge {
            display: inline-block;
            padding: 6px 16px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .priority-urgent { 
            background: linear-gradient(135deg, #dc3545 0%, #c82333 100%);
            color: white; 
        }
        
        .priority-high { 
            background: linear-gradient(135deg, #ffc107 0%, #ff9800 100%);
            color: #333; 
        }
        
        .priority-medium { 
            background: linear-gradient(135deg, #17a2b8 0%, #138496 100%);
            color: white; 
        }
        
        .priority-low { 
            background: linear-gradient(135deg, #28a745 0%, #20c997 100%);
            color: white; 
        }
        
        .task-details {
            color: #6c757d;
            margin: 16px 0;
            line-height: 1.8;
            font-size: 0.95em;
        }
        
        .task-details strong {
            color: #495057;
            font-weight: 600;
        }
        
        .task-actions {
            display: flex;
            gap: 10px;
            margin-top: 16px;
            flex-wrap: wrap;
        }
        
        .task-actions .btn {
            font-size: 0.9em;
            padding: 10px 20px;
        }
        
        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.6);
            align-items: center;
            justify-content: center;
            z-index: 1000;
            backdrop-filter: blur(4px);
        }
        
        .modal.show { 
            display: flex;
            animation: fadeIn 0.3s ease;
        }
        
        .modal-content {
            background: white;
            padding: 35px;
            border-radius: 20px;
            max-width: 520px;
            width: 90%;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
            animation: slideUp 0.3s ease;
        }
        
        @keyframes slideUp {
            from { opacity: 0; transform: translateY(30px); }
            to { opacity: 1; transform: translateY(0); }
        }
        
        .modal-content h2 {
            font-size: 1.8em;
            margin-bottom: 25px;
            color: #2c3e50;
            font-weight: 700;
        }
        
        .form-group { 
            margin-bottom: 22px; 
        }
        
        .form-group label { 
            display: block; 
            margin-bottom: 8px; 
            font-weight: 600;
            color: #495057;
            font-size: 0.95em;
        }
        
        .form-group input, .form-group select {
            width: 100%;
            padding: 12px 16px;
            border: 2px solid #e9ecef;
            border-radius: 10px;
            font-size: 1em;
            transition: all 0.3s;
            font-family: inherit;
        }
        
        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #667eea;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }
        
        .empty-state {
            text-align: center;
            padding: 60px 20px;
            color: #95a5a6;
        }
        
        .empty-state-icon {
            font-size: 4em;
            margin-bottom: 15px;
            opacity: 0.5;
        }
        
        .empty-state-text {
            font-size: 1.2em;
            font-weight: 500;
        }
        
        /* Responsive design */
        @media (max-width: 768px) {
            .header h1 { font-size: 2em; }
            .main-content { padding: 25px; }
            .task-card { padding: 20px; }
            .controls { flex-direction: column; }
            .btn { width: 100%; justify-content: center; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>&#128221; C++ To-Do List Manager</h1>
            <p>Powered by C++ Backend</p>
        </div>
        <div class="main-content">
            <div class="controls">
                <button class="btn btn-primary" onclick="showAddModal()">
                    <span>&#10133;</span> Add Task
                </button>
                <button class="btn btn-success" onclick="refreshTasks()">
                    <span>&#128260;</span> Refresh
                </button>
                <button class="btn btn-danger" onclick="clearAll()">
                    <span>&#128465;</span> Clear All
                </button>
            </div>
            <div id="tasksContainer" class="tasks-container"></div>
        </div>
    </div>

    <div id="taskModal" class="modal">
        <div class="modal-content">
            <h2>&#10133; Add New Task</h2>
            <form id="taskForm" onsubmit="addTask(event)">
                <div class="form-group">
                    <label>Task Name *</label>
                    <input type="text" id="taskName" placeholder="Enter task name..." required>
                </div>
                <div class="form-group">
                    <label>Priority *</label>
                    <select id="taskPriority" required>
                        <option value="0">Low</option>
                        <option value="1" selected>Medium</option>
                        <option value="2">High</option>
                        <option value="3">Urgent</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Category</label>
                    <input type="text" id="taskCategory" value="General" placeholder="Enter category...">
                </div>
                <div class="form-group">
                    <label>Due Date *</label>
                    <input type="date" id="taskDueDate" required>
                </div>
                <div style="display: flex; gap: 10px; margin-top: 25px;">
                    <button type="submit" class="btn btn-primary" style="flex: 1;">
                        <span>&#128190;</span> Save Task
                    </button>
                    <button type="button" class="btn btn-danger" onclick="closeModal()" style="flex: 1;">
                        <span>&#10006;</span> Cancel
                    </button>
                </div>
            </form>
        </div>
    </div>

    <script>
        const priorityLabels = ['Low', 'Medium', 'High', 'Urgent'];
        const priorityClasses = ['priority-low', 'priority-medium', 'priority-high', 'priority-urgent'];
        const priorityColors = ['#28a745', '#17a2b8', '#ffc107', '#dc3545'];

        function showAddModal() {
            document.getElementById('taskModal').classList.add('show');
            const today = new Date().toISOString().split('T')[0];
            document.getElementById('taskDueDate').value = today;
            document.getElementById('taskName').focus();
        }

        function closeModal() {
            document.getElementById('taskModal').classList.remove('show');
            document.getElementById('taskForm').reset();
        }

        // Close modal on outside click
        document.getElementById('taskModal').addEventListener('click', function(e) {
            if (e.target === this) {
                closeModal();
            }
        });

        async function addTask(e) {
            e.preventDefault();
            const name = document.getElementById('taskName').value;
            const priority = document.getElementById('taskPriority').value;
            const category = document.getElementById('taskCategory').value;
            const dueDate = document.getElementById('taskDueDate').value;

            const response = await fetch('/api/add', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `name=${encodeURIComponent(name)}&priority=${priority}&category=${encodeURIComponent(category)}&dueDate=${dueDate}`
            });

            if (response.ok) {
                closeModal();
                refreshTasks();
            }
        }

        async function refreshTasks() {
            const response = await fetch('/api/tasks');
            const tasks = await response.json();
            
            const container = document.getElementById('tasksContainer');
            if (tasks.length === 0) {
                container.innerHTML = `
                    <div class="empty-state">
                        <div class="empty-state-icon">&#128203;</div>
                        <div class="empty-state-text">No tasks yet. Create your first task!</div>
                    </div>
                `;
                return;
            }

            container.innerHTML = tasks.map(task => `
                <div class="task-card ${task.status === 'completed' ? 'completed' : ''}" style="--accent-color: ${priorityColors[task.priority]}">
                    <div style="display:flex;justify-content:space-between;align-items:start;margin-bottom:12px;flex-wrap:wrap;gap:10px;">
                        <div class="task-title">${escapeHtml(task.name)}</div>
                        <div class="priority-badge ${priorityClasses[task.priority]}">${priorityLabels[task.priority]}</div>
                    </div>
                    <div class="task-details">
                        <div><strong>&#128193; Category:</strong> ${escapeHtml(task.category)}</div>
                        <div><strong>&#128197; Due Date:</strong> ${formatDate(task.dueDate)}</div>
                        <div><strong>&#9989; Status:</strong> ${task.status === 'completed' ? '&#10003; Completed' : '&#9675; Pending'}</div>
                    </div>
                    <div class="task-actions">
                        ${task.status === 'pending' ? 
                            `<button class="btn btn-success" onclick="markComplete('${escapeHtml(task.name)}')">
                                <span>&#10003;</span> Complete
                            </button>` : ''}
                        <button class="btn btn-danger" onclick="deleteTask('${escapeHtml(task.name)}')">
                            <span>&#128465;</span> Delete
                        </button>
                    </div>
                </div>
            `).join('');
        }

        async function markComplete(name) {
            await fetch(`/api/complete?name=${encodeURIComponent(name)}`);
            refreshTasks();
        }

        async function deleteTask(name) {
            if (confirm(`Are you sure you want to delete "${name}"?`)) {
                await fetch(`/api/delete?name=${encodeURIComponent(name)}`);
                refreshTasks();
            }
        }

        async function clearAll() {
            if (confirm('\u26A0\uFE0F Delete ALL tasks? This action cannot be undone!')) {
                await fetch('/api/clear');
                refreshTasks();
            }
        }

        function formatDate(dateStr) {
            const date = new Date(dateStr);
            const options = { year: 'numeric', month: 'long', day: 'numeric' };
            return date.toLocaleDateString('en-US', options);
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        // Load tasks on start
        refreshTasks();
    </script>
</body>
</html>)HTML";
}

// Simple HTTP server
void start_server(int port = 8080) {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return;
    }
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed!" << std::endl;
        return;
    }
    
    // Enable address reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed! Port might be in use." << std::endl;
        closesocket(server_fd);
        return;
    }
    
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed!" << std::endl;
        closesocket(server_fd);
        return;
    }

    std::cout << "==================================" << std::endl;
    std::cout << "Server Started Successfully!" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Open your browser and go to:" << std::endl;
    std::cout << "http://localhost:" << port << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "==================================" << std::endl;

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);
        
        if (client_socket < 0) {
            continue;
        }
        
        char buffer[30000] = {0};
        int bytes_read = recv(client_socket, buffer, 30000, 0);
        
        if (bytes_read <= 0) {
            closesocket(client_socket);
            continue;
        }
        
        std::string request(buffer);
        std::string response;
        
        if (request.find("GET / ") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + get_html_page();
        }
        else if (request.find("GET /api/tasks") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + global_list.get_all_json();
        }
        else if (request.find("POST /api/add") != std::string::npos) {
            size_t body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                std::string body = request.substr(body_pos + 4);
                // Parse parameters
                std::string name, category, dueDate;
                int priority = 0;
                
                size_t pos = 0;
                while ((pos = body.find('&')) != std::string::npos) {
                    std::string param = body.substr(0, pos);
                    size_t eq = param.find('=');
                    if (eq != std::string::npos) {
                        std::string key = param.substr(0, eq);
                        std::string value = url_decode(param.substr(eq + 1));
                        
                        if (key == "name") name = value;
                        else if (key == "priority") priority = std::stoi(value);
                        else if (key == "category") category = value;
                        else if (key == "dueDate") dueDate = value;
                    }
                    
                    body.erase(0, pos + 1);
                }
                
                // Handle last parameter
                size_t eq = body.find('=');
                if (eq != std::string::npos) {
                    std::string key = body.substr(0, eq);
                    std::string value = url_decode(body.substr(eq + 1));
                    if (key == "name") name = value;
                    else if (key == "priority") priority = std::stoi(value);
                    else if (key == "category") category = value;
                    else if (key == "dueDate") dueDate = value;
                }
                
                global_list.new_entry(name, static_cast<Priority>(priority), category, parse_date(dueDate));
                global_list.save_to_file();
            }
            response = "HTTP/1.1 200 OK\r\n\r\nOK";
        }
        else if (request.find("GET /api/complete") != std::string::npos) {
            size_t pos = request.find("name=");
            if (pos != std::string::npos) {
                std::string name = request.substr(pos + 5);
                name = name.substr(0, name.find(' '));
                name = url_decode(name);
                global_list.mark_complete(name);
                global_list.save_to_file();
            }
            response = "HTTP/1.1 200 OK\r\n\r\nOK";
        }
        else if (request.find("GET /api/delete") != std::string::npos) {
            size_t pos = request.find("name=");
            if (pos != std::string::npos) {
                std::string name = request.substr(pos + 5);
                name = name.substr(0, name.find(' '));
                name = url_decode(name);
                global_list.remove(name);
                global_list.save_to_file();
            }
            response = "HTTP/1.1 200 OK\r\n\r\nOK";
        }
        else if (request.find("GET /api/clear") != std::string::npos) {
            global_list.clear_list();
            global_list.save_to_file();
            response = "HTTP/1.1 200 OK\r\n\r\nOK";
        }
        
        send(client_socket, response.c_str(), response.length(), 0);
        closesocket(client_socket);
    }
    
#ifdef _WIN32
    closesocket(server_fd);
    WSACleanup();
#endif
}

int main() {
    global_list.load_from_file();
    start_server(8080);
    return 0;
}